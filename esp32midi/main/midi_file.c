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
