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

static *t_midi_evt(t_midi_track *track) {
	return NULL;
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

static int readNxtEvent(FILE *fd, t_midi_track *trck) {
	int phase = 0;

	long pause=0;
	unsigned char event =0;
	unsigned char metaevent=0;
	int endofTrack = false;

	size_t szData = 16; // initial ssize of data
	unsigned char *data = calloc(szData,sizeof(unsigned char)); // TODO free
	size_t datalen=0; // number of bytes to read
	size_t datapos=0;

	do {
		unsigned char c = readNxtTrackData(fd, trck);

		switch(phase) {
			case 0: // get pause
				pause = pause << 7 + (c & 0x7F);
				if ( c < 0x80) {
					// pause completed
					phase = 1;
				}
				break;
			case 1: // get event
				if ( c < 0x80 ) {
					// use last event
					event = trck->lastevent;
					datalen = -1; // 1 byte less to read

					if ( datapos >= szData) {
						szData += 16;
						data = realloc(data, szData); // TODO free
					}
					data[datapos++] = c;

				} else {
					// new event
					event = trck->lastevent = c;
					datalen=0;
				}
				if ( event == 0xFF ) {
					// meta event, read further data
					phase = 4;
				} else {
					// other events, decide on bit 8-5 what to do
					unsigned char evtc = event & 0xF0;
					if ( evtc == 0xC0 || evtc == 0xD0) {
						// 1 data byte expected
						datalen++;
						if ( datalen == 0 ) {
							// completed, maybe as a result of lastevent
							phase = 99;
							break;
						}
						phase = 3; // read more data
					} else if ( evtc ==  0xF0) {
						// Spezialfall sysex evt:
						// das sind mit F0 und F7 geklammerte Midi-Daten
						// bei evt=F0 muss in den Daten das Start-F0 hinzugefügt werden,
						// die Daten sollten das Ende-F7-Datum enthalten
						// bei evt=F7 muss F0 und F7n in den Daten enthalten sein

						phase = 2; // read event len
						datalen=0;
					} else {
						// all others need 2 data bytes
						datalen += 2;
						phase = 3;
					}
				} // other events
				break;
			case 2: // read event len, handle data like in pause
				datalen = datalen << 7 + (c & 0x7F);
				if ( c < 0x80 ) {
					// datalen complete
					if ( datalen == 0) {
						phase = 99; // completed
					} else {
						phase = 3; // read data
					}
				}
				break;
			case 3: // read data
				if ( datapos >= szData) {
					szData += 16;
					data = realloc(data, szData);
				}
				data[datapos++] = c;
				datalen--;
				if ( datalen == 0 ) {
					// completed
					phase = 99;
				}
				break;
			case 4: // "meta event" nummber
				metaevent = c;
				if ( c== 0x2F) {
					endofTrack = true;
				}
				phase = 2;
				break;
			default:
		        ESP_LOGE(TAG, "unexpected phase %d", phase);
		        phase=99;
		}
	} while (phase < 99);

	// all data for midi event complete.
	// TODO create structure
}

int parse_midifile(const char *filepath, t_midi *midi) {
    FILE *fd = NULL;
    struct stat file_stat;

    char buf[32];
    int rc = -1;

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG, "Failed to stat file : %s", filepath);
        return -1;
    }

    fpos_t fsz = file_stat.st_size;

    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to open existing file : %s", filepath);
        return -1;
    }
    ESP_LOGI(TAG, "File opened: %s, size=%ld", filepath, fsz);
	do {
		memset(buf, 0, sizeof(buf));
		fread(buf, 1, 4, fd);
		// must start with "MThd"
		if ( strncmp(buf, "MThd", 4)) {
	        ESP_LOGE(TAG, "not a MIDI file");
			break;
		}
		// 4 byte headerlen
		long headerLen = read_long(4, fd);
		if ( headerLen != 6 ) {
	        ESP_LOGE(TAG, "header len is not 6");
			break;
		}
		// 2 byte format
		midi->format = read_long(2, fd);
		// 2 byte #tracks
		midi->ntracks = read_long(2, fd);
		// 2 byte division resp. tpq
		midi->tpq = read_long(2, fd);
		midi->microsecsperquarter =500000; // Tempo 120 = 500ms je 1/4 = 500 000 µs
	    ESP_LOGI(TAG, "Midi-Format=%d, tracks=%d, tpq=%ld",
	    		midi->format, midi->ntracks, midi->tpq);


	    // TODO Tracks
	    int trackno=0;
	    fpos_t fpos=14; // beginning of first track
	    int failed=false;
	    while ( fpos < fsz) {
	    	if (fsetpos(fd, &fpos)) {
		        ESP_LOGE(TAG, "fsetpos failed at %ld", fpos);
		        failed=true;
		        break;
	    	}

	    	// Read chunk type 4 byte
			memset(buf, 0, sizeof(buf));
			fread(buf, 1, 4, fd);
			// 4 byte headerlen
			long trackLen = read_long(4, fd);

			// actual file postion as base for track len
			fgetpos(fd, &fpos);

			// must start with "MTrk"
			if ( strncmp(buf, "MTrk", 4)) {
		        ESP_LOGI(TAG, "not a TrackChunk '%s', len=%ld", buf,trackLen);
			} else {
				// it's a track chunk
		        ESP_LOGI(TAG, "it's a TrackChunk len='%ld'",  trackLen);
		        t_midi_track *trck = calloc(1, sizeof(t_midi_track));
		        trck->len = trackLen;
		        trck->trackno = ++trackno;
		        trck->rdpos=0;
		        trck->buflen=0;
		        trck->finished = 0;
			}
			fpos += trackLen;
	    };
	    if ( failed ) {
	    	break;
	    }
		rc=0;
	} while(0);

	fclose(fd);
    ESP_LOGI(TAG, "File parse complete");

    return rc;
}

int handle_midifile(const char *filename) {
	t_midi t;

	memset(&t, 0, sizeof(t));
	int rc=parse_midifile(filename, &t);

	return rc;

}
