/*
 * local.h
 *
 *  Created on: 9 May 2019
 *      Author: ankrysm
 */

#ifndef ESP32MIDI_MAIN_LOCAL_H_
#define ESP32MIDI_MAIN_LOCAL_H_

#ifndef size_t
#define size_t unsigned int
#endif

#ifndef false
#define false 0
#endif

#ifndef true
#define true 1
#endif

// structures
// static midi-Data
typedef struct  {
	int  datalen;
    const char data[10];  // MIDI-Daten
} t_midi_data;



// Midi data from file
typedef struct {
	int pause;
	size_t datalen;
	unsigned char *data;
} t_midi_evt;

// Track
typedef struct midi_track {
	int trackno;
	unsigned int len;
	unsigned char buf[256];
	unsigned long fpos; // file position
	unsigned int buflen; // number of bytes in buffer
	unsigned int rdpos; // read position on buffer
	unsigned char lastevent;
	int finished;
    struct midi_track *nxt;
} t_midi_track;

// Midi Song
typedef struct {
	int format;
	int ntracks;
	long tpq; // division
	long microsecsperquarter;
	t_midi_data *tracks;
} t_midi;

// Prototypes
// GPIO/LED
void led_init();
void blue_on();
void blue_off();

// MIDI
void midi_init();
void midi_out( const char *data, int len);
void play_ok();
void play_err();

// MIDI file
int handle_midifile(const char *filename);
int parse_midifile(const char *filename, t_midi *midi);

// Start Fileserver
esp_err_t start_file_server(const char *base_path);


#endif /* ESP32MIDI_MAIN_LOCAL_H_ */
