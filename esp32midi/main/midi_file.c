/*
 * midi_file.c
 *
 *  Created on: 14 May 2019
 *      Author: akrysmanski
 */

#include "local.h"

static const char *TAG = "midi_file";

static esp_timer_handle_t periodic_midi_timer = NULL;

static 	t_midi_song *globalSongData=NULL;

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


static void clearEvent(t_midi_evt *evt) {
	evt->event = 0;
	evt->metaevent = 0;
	evt->delta_ticks = 0;
	evt->evt_ticks = 0;
	evt->status = no_event;
	evt->datalen = 0;

	if ( evt->data) {
		free(evt->data);
	}
	evt->data = NULL;
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

static void readNxtEvent(t_midi_track *trck) {
	int phase = 0;

	t_midi_evt *evt = &(trck->evt);
	clearEvent(evt);

	size_t szData = 16; // initial ssize of data
	evt->data = calloc(szData, sizeof(unsigned char));
	evt->datalen = 0;

	size_t datalen = 0; // number of bytes to read

	do {
		unsigned char c = readNxtTrackData(globalSongData->fd, trck);

		switch (phase) {
			case 0: // get delta time
				evt->delta_ticks = (evt->delta_ticks << 7) + (c & 0x7F);
				if (c < 0x80) {
					// pause completed
					phase = 1;
					trck->track_ticks += TICKFACTOR * evt->delta_ticks;
					evt->evt_ticks = trck->track_ticks;
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
					evt->status = has_event;
					phase = 99;
				}
				break;
			case 4: // "meta event" nummber
				evt->metaevent = c;
				if (c == 0x2F) {
					evt->status = has_end_of_track;
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
	if (globalSongData == NULL) {
		// create new data
		globalSongData = calloc(1, sizeof(t_midi_song));

	} else {
		// reset data
		if (globalSongData->tracks) {
			while (globalSongData->tracks) {
				t_midi_track *tmp = globalSongData->tracks->nxt;
				globalSongData->tracks = globalSongData->tracks->nxt;
				if ( tmp ) {
					free(tmp);
				}
			}
		}

		if (globalSongData->filepath) {
			free(globalSongData->filepath);
		}

		if (globalSongData->fd) {
			fclose(globalSongData->fd);
		}

		memset(globalSongData, 0, sizeof(t_midi_song));
	}

}

/**
 * timing has changed
 */
static void calcTimermillies() {
	/*
	ticks_per_quarter (tpq) = <PPQ from the header>
	µs_per_quarter = <Tempo in latest Set Tempo event>
	µs_per_tick = µs_per_quarter / ticks_per_quarter
	seconds_per_tick = µs_per_tick / 1.000.000
	seconds = ticks * seconds_per_tick
	*/

	globalSongData->timermillies = 0; // const value
	ESP_LOGI(TAG, "calcTimermillies: microsecPerQuarter=%ld, tempo=%ld BPM, microseconds_per_tick=%ld",
			globalSongData->microsecsperquarter,
			60000000/globalSongData->microsecsperquarter,
			globalSongData->microseconds_per_tick);

	do {
		globalSongData->timermillies += DELTATIMERMILLIES;
		long quantization = globalSongData->microsecsperquarter/1000/globalSongData->timermillies;
		if ( quantization <= 0)
			quantization=1;

		globalSongData->timer_ticks = TICKFACTOR * globalSongData->tpq / quantization;
		globalSongData->microseconds_per_tick = globalSongData->microsecsperquarter / globalSongData->tpq;

		if ( globalSongData->timer_ticks < 1 ) {
			globalSongData->timer_ticks = 1;
		}

		ESP_LOGI(TAG, "calcTimermillies: quantization=%ld -> millies=%ld, tickspercycle = %ld",
				quantization,
				globalSongData->timermillies,
				globalSongData->timer_ticks
				);
	} while ( globalSongData->timer_ticks < 100);
}

/**
 * open a midi file and initialize song structure
 */
int open_midifile(const char *filepath) {

	struct stat file_stat;

	char buf[32];
	int rc = -1;


	globalSongData->filepath = strdup(filepath);

	// try to open file
	if (stat(filepath, &file_stat) == -1) {
		ESP_LOGE(TAG, "Failed to stat file : %s", filepath);
		return -1;
	}

	fpos_t fsz = file_stat.st_size;

	globalSongData->fd = fopen(filepath, "r");
	if (!globalSongData->fd) {
		ESP_LOGE(TAG, "Failed to open existing file : %s", filepath);
		return -1;
	}
	ESP_LOGI(TAG, "File opened: %s, size=%ld", filepath, fsz);

	t_midi_track *last_track = NULL;

	do {
		memset(buf, 0, sizeof(buf));
		fread(buf, 1, 4, globalSongData->fd);
		// must start with "MThd"
		if (strncmp(buf, "MThd", 4)) {
			ESP_LOGE(TAG, "not a MIDI file");
			break;
		}
		// 4 byte headerlen
		long headerLen = read_long(4, globalSongData->fd);
		if (headerLen != 6) {
			ESP_LOGE(TAG, "header len is not 6");
			break;
		}
		// 2 byte format
		globalSongData->format = read_long(2, globalSongData->fd);
		// 2 byte #tracks
		globalSongData->ntracks = read_long(2, globalSongData->fd);
		// 2 byte division resp. ticks per quarter
		globalSongData->tpq = read_long(2, globalSongData->fd);

		globalSongData->microsecsperquarter = 500000; // Tempo 120 = 500ms je 1/4 = 500 000 µs
		globalSongData->timermillies = 0;

		calcTimermillies();

		globalSongData->song_ticks = 0;

		ESP_LOGI(TAG, "Midi-Format=%d, tracks=%d, tpq=%ld, timerticks=%ld, timermillies=%ld",
				globalSongData->format, globalSongData->ntracks,
				globalSongData->tpq, globalSongData->timer_ticks,
				globalSongData->timermillies);

		// Tracks
		int trackno = 0;
		fpos_t fpos = 14; // beginning of first track
		int failed = false;
		while (fpos < fsz) {
			if (fsetpos(globalSongData->fd, &fpos)) {
				ESP_LOGE(TAG, "fsetpos failed at %ld", fpos);
				failed = true;
				break;
			}

			// Read chunk type 4 byte
			memset(buf, 0, sizeof(buf));
			fread(buf, 1, 4, globalSongData->fd);
			// 4 byte headerlen
			long trackLen = read_long(4, globalSongData->fd);

			// actual file position - track data starts here
			fgetpos(globalSongData->fd, &fpos);

			// must start with "MTrk"
			if (strncmp(buf, "MTrk", 4)) {
				ESP_LOGI(TAG, "fpos %ld: not a MidiTrackChunk '%s', len=%ld", fpos, buf, trackLen);
			} else {
				// it's a track chunk
				ESP_LOGI(TAG, "fpos %ld: MidiTrackChunk(%d) len='%ld'", fpos, trackno, trackLen);

				t_midi_track *trck = calloc(1, sizeof(t_midi_track));
				trck->len = trackLen;
				trck->trackno = trackno++;
				trck->rdpos = 0;
				trck->buflen = 0;
				trck->finished = false;
				trck->lastevent = 0;
				trck->fpos = fpos;
				trck->track_ticks = 0;
				trck->evt.status = need_event;

				// add to song
				if (last_track) {
					last_track->nxt = trck;
					last_track = last_track->nxt;
				} else {
					globalSongData->tracks = trck;
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

static void printEvent(int trackno, t_midi_evt *evt, const char *msg) {
	char txt[1024];
	memset(txt, 0, sizeof(txt));
	if (evt->datalen > 0) {
		for (int i = 0; i < evt->datalen; i++) {
			snprintf(&txt[strlen(txt)], sizeof(txt) - strlen(txt), "%02X ", evt->data[i]);
		}
		snprintf(&txt[strlen(txt)], sizeof(txt) - strlen(txt), "%s"," '");
		for (int i = 0; i < evt->datalen; i++) {
			snprintf(&txt[strlen(txt)], sizeof(txt) - strlen(txt), "%c",
					isprint((uchar)evt->data[i]) ? evt->data[i] :'.');
		}
		snprintf(&txt[strlen(txt)], sizeof(txt) - strlen(txt), "%s","'");
	}
	ESP_LOGI(TAG, "track %d, T=%7ld, DT=%5ld, E=%x, M=%x, L=%d %s %s %s",
			trackno,
			evt->evt_ticks,
			evt->delta_ticks,
			evt->event, evt->metaevent, evt->datalen,
			evt->status != has_event ? EVENT_STATE2TXT(evt->status):"",
			msg,
			txt);

}

/**
 * single cycle of playing/printing midi events
 */
int parse_midifile() {
	if (!globalSongData || !globalSongData->tracks || !globalSongData->fd) {
		ESP_LOGE(TAG, "parse_midifile: nodata / notracks or no open file");
		return -1;
	}

	if ( globalSongData->song_ticks < 0 ) {
		// wait a while until web server is not busy
		globalSongData->song_ticks++;
		return 1;
	} if (globalSongData->song_ticks == 0 ) {
		// playing the song will start
		ESP_LOGI(TAG, "---- Start %s -----", globalSongData->filepath);
	}
	char *data=NULL;
	char *ptr=NULL;
	size_t datalen = 0;
	size_t sz_data=256;

#ifdef WITH_PRINING_MIDIFILES
	if ( globalSongData->printonly)
		ESP_LOGI(TAG, "---- %6ld -----",globalSongData->song_ticks);
#endif

	long min_deltatime=LONG_MAX;
	int activeTracks=0;
	for (t_midi_track *midi_track = globalSongData->tracks; midi_track; midi_track = midi_track->nxt) {
		if (midi_track->finished )
			continue;

		t_midi_evt *evt = &(midi_track->evt);

		while( !midi_track->finished) {
			if ( evt->status == need_event) {
				// needs an event
				readNxtEvent(midi_track);
			}

			if ( evt->status == has_end_of_track) {
				midi_track->finished = true;
				clearEvent(evt);
				ESP_LOGI(TAG,
						"%6ld: end of track %d",
						globalSongData->song_ticks,
						midi_track->trackno);
				break; // with next track
			}

			if (evt->status != has_event ) {
				printEvent( midi_track->trackno, evt, "ups");
				ESP_LOGE(TAG, "track %d: should have an event at fpos %ld", midi_track->trackno, midi_track->fpos);
				midi_track->finished = true;
				break;  // ups
			}

			if (evt->evt_ticks > globalSongData->song_ticks ) {
				// have to wait
				long dt = evt->evt_ticks - globalSongData->song_ticks;
				if (dt < min_deltatime) {
					min_deltatime = dt;
				}
				break; // while
			}

			// process event
			if ( evt->event == 0xFF && evt->metaevent == 0x51) {
				// *** new tempo
				printEvent( midi_track->trackno, evt, "new tempo");
				long tempo =0;
				for ( int i=0; i< evt->datalen; i++){
					tempo = tempo << 8 | evt->data[i];
				}
				if ( tempo == 0 ){
					ESP_LOGE(TAG, "track %d: could not calculate tempo at fpos %ld", midi_track->trackno, midi_track->fpos);
				} else {
					ESP_LOGI(TAG,
							"%6ld: track %d: new tempo %ld",
							//globalSongData->song_ticks * globalSongData->microseconds_per_tick / 1000,
							globalSongData->song_ticks,
							midi_track->trackno, tempo);
					globalSongData->microsecsperquarter = tempo;
					calcTimermillies();
#ifdef WITH_PRINING_MIDIFILES
					if ( ! globalSongData->printonly ) {
#endif
						// restart timer
						esp_timer_stop(periodic_midi_timer);
						ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_midi_timer, globalSongData->timermillies*1000));
#ifdef WITH_PRINING_MIDIFILES
					}
#endif
				}
			} else if ( (evt->event & 0xF0) != 0xF0 ) {
				// *** event to play
#ifdef WITH_PRINING_MIDIFILES
				if ( globalSongData->printonly) {
					printEvent( midi_track->trackno, evt, "play");
				} else {
#endif
					//play
					midi_out_evt(evt->event, evt->data, evt->datalen);
					if ( ! ptr  ) {
						ptr = data = calloc(sz_data, sizeof(char));
					}
					int new_datalen = datalen + evt->datalen +1;
					if ( new_datalen < sz_data) {
						*ptr=evt->event;
						ptr++;
						memcpy(ptr,evt->data, evt->datalen);
						ptr += evt->datalen;
						datalen= new_datalen;
					} else {
						ESP_LOGE(TAG, "track %d: too many data needed %d, max %d", midi_track->trackno, new_datalen, sz_data);
					}
#ifdef WITH_PRINING_MIDIFILES
 				}
#endif
			} else {
#ifdef WITH_PRINING_MIDIFILES
				if ( globalSongData->printonly) {
					printEvent( midi_track->trackno, evt, "ignored");
				}
#endif
			}

			readNxtEvent(midi_track);

		} // while, one track completed

		if ( ! midi_track->finished) {
			activeTracks++;
		}
	} // all tracks processed

	// something to play?
	if ( datalen > 0) {
		midi_out(data,datalen);
		free(data);
	}

	if ( activeTracks > 0) {
#ifdef WITH_PRINING_MIDIFILES
		if ( globalSongData->printonly) {
			if ( min_deltatime < 0) {
				min_deltatime = 1;
			}
			globalSongData->song_ticks += min_deltatime;
		} else {
#endif
			if ( globalSongData->timer_ticks == 0) {
				ESP_LOGE(TAG, "NO TIMERTICKS");
				return 0;

			}
			globalSongData->song_ticks += globalSongData->timer_ticks;
#ifdef WITH_PRINING_MIDIFILES
		}
#endif
	} else {
		ESP_LOGI(TAG,
				"%6ld: end of song %s",
				globalSongData->song_ticks, globalSongData->filepath);

	}

	return activeTracks;
}

static void periodic_midi_timer_callback(void* arg) {

	int n =parse_midifile(false);

	if (n <= 0) {
		// Schluss, Timer stoppen
		ESP_ERROR_CHECK(esp_timer_stop(periodic_midi_timer));
		int64_t time = esp_timer_get_time();
		ESP_LOGI(TAG, "Periodic midi timer stopped, duration %lld ms", (time - globalSongData->starttime)/1000 );
		initSongData();
	}
}

int handle_play_midifile(const char *filename) {
	int rc = -1;
	do {
		if (periodic_midi_timer) {
			esp_timer_stop(periodic_midi_timer);
		}

		// initialize song-data
		initSongData();

#ifdef WITH_PRINING_MIDIFILES
		globalSongData->printonly = false;
#endif

		if (open_midifile(filename)) break;

		midi_reset();

		globalSongData->song_ticks = -200; // delay until start playing XXX

		if (periodic_midi_timer == NULL) {
			// timer muss erzeugt werden
			const esp_timer_create_args_t periodic_timer_args = {
					.callback =	&periodic_midi_timer_callback,
					// name is optional, but may help identify the timer when debugging
					.name = "periodic_midi"
			};

			ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_midi_timer));
		} else {
			// Timer stoppen, wenn er läuft
			esp_timer_stop(periodic_midi_timer);
		}
		globalSongData->starttime = esp_timer_get_time();
		ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_midi_timer, globalSongData->timermillies*1000));
		ESP_LOGI(TAG, "playing midifile started");

		rc = 0;
	} while(0);

	return rc;
}

#ifdef WITH_PRINING_MIDIFILES
int handle_print_midifile(const char *filename) {
	int rc = -1;
	do {
		if (periodic_midi_timer) {
			esp_timer_stop(periodic_midi_timer);
		}

		// initialize song-data
		initSongData();

		globalSongData->printonly = true;

		if (open_midifile(filename)) break;

		midi_reset();

		while (parse_midifile(true) > 0) {
			esp_task_wdt_reset();
		}

		initSongData();
		ESP_LOGI(TAG, "printing midifile end");

		rc = 0;
	} while(0);

	return rc;
}
#endif

int handle_stop_midifile() {
	if (periodic_midi_timer) {
		esp_timer_stop(periodic_midi_timer);
	}

	// initialize song-data
	initSongData();

	midi_reset();
	return 0;

}

int handle_play_random_midifile(const char *dirpath) {

	char entrypath[256+1];

    struct dirent *entry;
    struct stat entry_stat;

    int max =0;
    DIR *dir = opendir(dirpath);
    if (!dir) {
        ESP_LOGE(TAG, "handle_play_random_midifile; Failed to stat dir : %s", dirpath);
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
    	if (entry->d_type == DT_DIR)
    		continue;
        snprintf(entrypath, sizeof(entrypath),"%s/%s", dirpath, entry->d_name);
        if (stat(entrypath, &entry_stat) == -1) {
            ESP_LOGE(TAG, "handle_play_random_midifile: %s: Failed to stat %s", entrypath, entry->d_name);
            continue;
        }
        if (! IS_FILE_EXT(entrypath, ".mid")) {
    		ESP_LOGI(TAG, "handle_play_random_midifile: %s is not a midifile", entrypath);
    		continue;
        }
		ESP_LOGI(TAG, "handle_play_random_midifile[%d]: %s is a midifile", ++max, entrypath);

    }
    closedir(dir);

    int r = 1 + esp_random() % (max - 1);

    ESP_LOGI(TAG, "handle_play_random_midifile: random number is %d/%d", r, max);

    dir = opendir(dirpath);
    if (!dir) {
        ESP_LOGE(TAG, "handle_play_random_midifile: Failed to stat dir : %s", dirpath);
        return -1;
    }
    while ((entry = readdir(dir)) != NULL) {
    	if (entry->d_type == DT_DIR)
    		continue;
        snprintf(entrypath, sizeof(entrypath),"%s/%s", dirpath, entry->d_name);
        if (stat(entrypath, &entry_stat) == -1) {
            ESP_LOGE(TAG, "handle_play_random_midifile: %s: Failed to stat %s", entrypath, entry->d_name);
            continue;
        }
        if (! IS_FILE_EXT(entrypath, ".mid")) {
    		ESP_LOGI(TAG, "handle_play_random_midifile: %s is not a midifile", entrypath);
    		continue;
        }
        r--;
		ESP_LOGI(TAG, "handle_play_random_midifile[%d]: %s is a midifile", r, entrypath);
		if ( r > 0 ){
			continue;
		}
		ESP_LOGI(TAG, "handle_play_random_midifile: play %s", entrypath);
		handle_play_midifile(entrypath);
		break;
    }
    closedir(dir);


    return 0;
}
