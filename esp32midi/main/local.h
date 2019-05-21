/*
 * local.h
 *
 *  Created on: 9 May 2019
 *      Author: ankrysm
 */

#ifndef ESP32MIDI_MAIN_LOCAL_H_
#define ESP32MIDI_MAIN_LOCAL_H_

// to make eclipse happy:
#ifndef size_t
#define size_t unsigned int
#endif

#ifndef false
#define false 0
#endif

#ifndef true
#define true 1
#endif

// end of happiness...

enum EVENT_STATE
	{no_event, need_event, has_event, has_end_of_track };

#define EVENT_STATE2TXT(c) ( \
	c==no_event ? "no event" : \
	c==need_event ? "need event" : \
	c==has_event ? "has event" : \
	c==has_end_of_track ? "end of track" : "???" \
)

// structures
// static midi-Data
typedef struct  {
	int  datalen;
    const char data[10];  // MIDI-Daten
} t_midi_data;



// Midi data from file
typedef struct midi_evt {
	long evt_time;
	long delta_time;
	unsigned char event;
	unsigned char metaevent;
	int status;
	size_t datalen;
	unsigned char *data;
} t_midi_evt;

// Track
typedef struct midi_track {
	int trackno;
	unsigned int len;
	unsigned char buf[256];
	long fpos; // file position
	unsigned int buflen; // number of bytes in buffer
	unsigned int rdpos; // read position on buffer
	//
	long track_time;
	unsigned char lastevent; // in case of repeated events
	int finished; // finished means: got end of track Event FF 21 00
	t_midi_evt evt;
	//
	struct midi_track *nxt;
} t_midi_track;

// Midi Song
typedef struct {
	FILE *fd;
	char *filepath;
	int format; // from header
	int ntracks;
	//
	long tpq; // division
	long microsecsperquarter; // tempo
	long microseconds_per_tick;
	long song_time; //
	long quantization; // 8,16,32,64
	// play parameter
	long timermillies; // timerperiod in ms
	long timerticks; // ticks per timer processing
	int64_t starttime;
	t_midi_track *tracks;
} t_midi_song;

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
void midi_reset();

// MIDI file
int handle_midifile(const char *filename);
int open_midifile(const char *filename);
int parse_midifile();

// Start Fileserver
esp_err_t start_file_server(const char *base_path);


#endif /* ESP32MIDI_MAIN_LOCAL_H_ */
