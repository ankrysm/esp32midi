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

#ifndef SNTP_OPMODE_POLL
#define SNTP_OPMODE_POLL 0
#endif

// end of happiness...

#ifndef uchar
#define uchar unsigned char
#endif

enum EVENT_STATE
	{no_event, need_event, has_event, has_end_of_track };

#define EVENT_STATE2TXT(c) ( \
	c==no_event ? "'no event'" : \
	c==need_event ? "'need event'" : \
	c==has_event ? "'has event'" : \
	c==has_end_of_track ? "'end of track'" : "???" \
)

// for better resolution: ticks multiplied by this factor
#define TICKFACTOR 10

// for calculate timing
#define DELTATIMERMILLIES 10

//#define WITH_PRINING_MIDIFILES

// structures
// static midi-Data
typedef struct  {
	int  datalen;
    const char data[10];  // MIDI-Daten
} t_midi_data;



// Midi data from file
typedef struct midi_evt {
	long evt_ticks;
	long delta_ticks;
	unsigned char event;
	unsigned char metaevent;
	int status;
	size_t datalen;
	char *data;
} t_midi_evt;

// Track
typedef struct midi_track {
	int trackno;
	unsigned int len;
	char buf[256];
	long fpos; // file position
	unsigned int buflen; // number of bytes in buffer
	unsigned int rdpos; // read position on buffer
	//
	long track_ticks;
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
	long song_ticks; // ticks from the beginning
	// play parameter
	long timermillies; // timerperiod in ms
	long timer_ticks; // ticks per timer processing
#ifdef WITH_PRINING_MIDIFILES
	int printonly;
#endif
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
void midi_out_evt( const char evt, const char *data, int len);
void play_ok();
void play_err();
void midi_reset();

// MIDI file
int open_midifile(const char *filename);
int parse_midifile();
int handle_play_midifile(const char *filename);
int handle_print_midifile(const char *filename);
int handle_stop_midifile();

// Start Fileserver
esp_err_t start_file_server(const char *base_path);

// sntp
void test_sntp();
void obtain_time(void);
void initialize_sntp(void);






#endif /* ESP32MIDI_MAIN_LOCAL_H_ */
