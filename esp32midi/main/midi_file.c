/*
 * midi_file.c
 *
 *  Created on: 14 May 2019
 *      Author: akrysmanski
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sys/stat.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "local.h"

static const char *TAG = "midi_file";

static 	t_midi_song *song=NULL;

static long read_long(size_t n, FILE *fd) {
    unsigned char buf[32];
	memset(buf, 0, sizeof(buf));
	size_t nrd=fread(buf, 1, n, fd);
	if ( nrd < n) {
        ESP_LOGE(TAG, "not enough data read: %d/%d", nrd, n);
        return 0L;
	}
	long val=0L;
	for (int i=0;i <nrd; i++) {
		val = val << 8 | buf[i];
	}
	return val;
}


/**
 * gets the next byte from stream,
 * fills the tracks buffer if necessary
 */
static unsigned char readNxtTrackData(FILE *fd, t_midi_track *trck) {
	if ( trck->rdpos >= trck->buflen) {
		// need new data for buffer
		trck->rdpos=0;

		fsetpos(fd, &(trck->fpos));
		trck->buflen=fread(trck->buf, 1, sizeof(trck->buf), fd);
		if ( trck->buflen < 1) {
			// EOF or read error
			trck->finished = true;
			return '\0';
		}
		fgetpos(fd, &(trck->fpos));
	}
    return trck->buf[(trck->rdpos)++];
}

static void readNxtEvent(t_midi_track *trck, t_midi_evt *evt) {
	int phase = 0;

	evt->pause = 0;
	evt->event = 0;
	evt->metaevent = 0;
	evt->endofTrack = false;

	if (evt->data) {
		free(evt->data);
	}

	size_t szData = 16; // initial ssize of data
	evt->data = calloc(szData, sizeof(unsigned char));
	evt->datalen = 0;

	size_t datalen = 0; // number of bytes to read

	do {
		unsigned char c = readNxtTrackData(song->fd, trck);

		switch (phase) {
			case 0: // get pause
				evt->pause = (evt->pause << 7) + (c & 0x7F);
				if (c < 0x80) {
					// pause completed
					phase = 1;
				}
				break;
			case 1: // get event
				if (c < 0x80) {
					// use last event
					evt->event = trck->lastevent;
					datalen = -1; // 1 byte less to read

					if (evt->datalen >= szData) {
						szData += 16;
						evt->data = realloc(evt->data, szData);
					}
					evt->data[(evt->datalen)++] = c;

				} else {
					// new event
					evt->event = trck->lastevent = c;
					datalen = 0;
				}
				if (evt->event == 0xFF) {
					// meta event, read further data
					phase = 4;
				} else {
					// other events, decide on bit 8-5 what to do
					unsigned char evtc = evt->event & 0xF0;
					if (evtc == 0xC0 || evtc == 0xD0) {
						// 1 data byte expected
						datalen++;
						if (datalen == 0) {
							// completed, maybe as a result of lastevent
							phase = 99;
							break;
						}
						phase = 3; // read more data
					} else if (evtc == 0xF0) {
						// Spezialfall sysex evt:
						// das sind mit F0 und F7 geklammerte Midi-Daten
						// bei evt=F0 muss in den Daten das Start-F0 hinzugefügt werden,
						// die Daten sollten das Ende-F7-Datum enthalten
						// bei evt=F7 muss F0 und F7n in den Daten enthalten sein

						phase = 2; // read event len
						datalen = 0;
					} else {
						// all others need 2 data bytes
						datalen += 2;
						phase = 3;
					}
				} // other events
				break;
			case 2: // read event len, handle data like in pause
				datalen = (datalen << 7) + (c & 0x7F);
				if (c < 0x80) {
					// datalen complete
					if (datalen == 0) {
						phase = 99; // completed
					} else {
						phase = 3; // read data
					}
				}
				break;
			case 3: // read data
				if (evt->datalen >= szData) {
					szData += 16;
					evt->data = realloc(evt->data, szData);
				}
				evt->data[(evt->datalen)++] = c;
				datalen--;
				if (datalen == 0) {
					// completed
					phase = 99;
				}
				break;
			case 4: // "meta event" nummber
				evt->metaevent = c;
				if (c == 0x2F) {
					evt->endofTrack = true;
				}
				phase = 2;
				break;
			default:
				ESP_LOGE(TAG, "unexpected phase %d", phase);
				phase = 99;
		}
	} while (phase < 99);

	// all data for midi event complete.
}

static void initSongData() {
	if (song == NULL) {
		// create new data
		song = calloc(1, sizeof(t_midi_song));

	} else {
		// reset data
		if (song->tracks) {
			while (song->tracks) {
				t_midi_track *tmp = song->tracks->nxt;
				song->tracks->nxt = song->tracks->nxt->nxt;

				free(tmp);
			}
		}

		if (song->filepath) {
			free(song->filepath);
		}

		if (song->fd) {
			fclose(song->fd);
		}

		memset(song, 0, sizeof(t_midi_song));
	}

}

int open_midifile(const char *filepath) {

	struct stat file_stat;

	char buf[32];
	int rc = -1;

	// initialize song-data
	initSongData();

	song->filepath = strdup(filepath);

	// try to open file
	if (stat(filepath, &file_stat) == -1) {
		ESP_LOGE(TAG, "Failed to stat file : %s", filepath);
		return -1;
	}

	fpos_t fsz = file_stat.st_size;

	song->fd = fopen(filepath, "r");
	if (!song->fd) {
		ESP_LOGE(TAG, "Failed to open existing file : %s", filepath);
		return -1;
	}
	ESP_LOGI(TAG, "File opened: %s, size=%ld", filepath, fsz);

	t_midi_track *last_track = NULL;

	do {
		memset(buf, 0, sizeof(buf));
		fread(buf, 1, 4, song->fd);
		// must start with "MThd"
		if (strncmp(buf, "MThd", 4)) {
			ESP_LOGE(TAG, "not a MIDI file");
			break;
		}
		// 4 byte headerlen
		long headerLen = read_long(4, song->fd);
		if (headerLen != 6) {
			ESP_LOGE(TAG, "header len is not 6");
			break;
		}
		// 2 byte format
		song->format = read_long(2, song->fd);
		// 2 byte #tracks
		song->ntracks = read_long(2, song->fd);
		// 2 byte division resp. tpq
		song->tpq = read_long(2, song->fd);
		song->microsecsperquarter = 500000; // Tempo 120 = 500ms je 1/4 = 500 000 µs
		ESP_LOGI(TAG, "Midi-Format=%d, tracks=%d, tpq=%ld",
				song->format, song->ntracks, song->tpq);

		// Tracks
		int trackno = 0;
		fpos_t fpos = 14; // beginning of first track
		int failed = false;
		while (fpos < fsz) {
			if (fsetpos(song->fd, &fpos)) {
				ESP_LOGE(TAG, "fsetpos failed at %ld", fpos);
				failed = true;
				break;
			}

			// Read chunk type 4 byte
			memset(buf, 0, sizeof(buf));
			fread(buf, 1, 4, song->fd);
			// 4 byte headerlen
			long trackLen = read_long(4, song->fd);

			// actual file position - track data starts here
			fgetpos(song->fd, &fpos);

			// must start with "MTrk"
			if (strncmp(buf, "MTrk", 4)) {
				ESP_LOGI(TAG, "not a MidiTrackChunk '%s', len=%ld", buf, trackLen);
			} else {
				// it's a track chunk
				trackno++;
				ESP_LOGI(TAG, "MidiTrackChunk(%d) len='%ld'", trackno, trackLen);

				t_midi_track *trck = calloc(1, sizeof(t_midi_track));
				trck->len = trackLen;
				trck->trackno = trackno;
				trck->rdpos = 0;
				trck->buflen = 0;
				trck->finished = false;
				trck->lastevent = 0;
				trck->fpos = fpos;

				// add to song
				if (last_track) {
					last_track->nxt = trck;
					last_track = last_track->nxt;
				} else {
					song->tracks = trck;
					last_track = trck;
				}
			}
			fpos += trackLen;
		};
		if (failed) {
			break;
		}
		rc = 0;
	} while (0);

	ESP_LOGI(TAG, "File open complete");

	return rc;
}


int parse_midifile() {
	int rc = 0;

	if (!song || !song->tracks) {
		ESP_LOGE(TAG, "parse_midifile: NODATA");
		return 0;
	}

	ESP_LOGI(TAG, "parse midifile Start");

	for (t_midi_track *t = song->tracks; t; t = t->nxt) {
		t_midi_evt evt;
		memset(&evt, 0, sizeof(evt));

		readNxtEvent(t, &evt);

		char txt[256];
		memset(txt, 0, sizeof(txt));
		if (evt.datalen > 0) {
			for (int i = 0; i < evt.datalen; i++) {
				snprintf(&txt[strlen(txt)], sizeof(txt) - strlen(txt), " %02X", evt.data[i]);
			}
		}
		ESP_LOGI(TAG, "track %d, Pause=%ld Event=%x Datalen=%d '%s'", t->trackno, evt.pause, evt.event, evt.datalen, txt);

		if (evt.data) {
			free(evt.data);
		}
		// TODO weitere events parsen

	}

	ESP_LOGI(TAG, "parse midifile ended");
	return rc;
}


int handle_midifile(const char *filename) {

	int rc = -1;
	do {
		if (open_midifile(filename)) break;
		if (parse_midifile()) break;

	} while(0);

	if ( song && song->fd) {
		fclose(song->fd);
		song->fd=NULL;
	}

	// check if needed TODO
	initSongData();

	return rc;

}
