/*
 * local.h
 *
 *  Created on: 9 May 2019
 *      Author: ankrysm
 */

#ifndef ESP32MIDI_MAIN_LOCAL_H_
#define ESP32MIDI_MAIN_LOCAL_H_

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_event.h"
//#include "esp_event_loop.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_http_server.h"



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

#ifndef SNTP_SYNC_STATUS_RESET
#define SNTP_SYNC_STATUS_RESET 0
#endif

#ifndef GPIO_PIN_INTR_POSEDGE
#define GPIO_PIN_INTR_POSEDGE 1
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

// Max length a file path can have on storage
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

// Max size of an individual file. Make sure this
// value is same as that set in upload_script.html
#define MAX_FILE_SIZE   (200*1024) // 200 KB
#define MAX_FILE_SIZE_STR "200KB"
#define BASE_PATH "/spiffs"

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

#define DELAY_MILLIES 2000 // 2 secs
//#define WITH_PRINING_MIDIFILES

enum enum_action {
	action_none,
	action_stop,
	action_pause,
	action_play,
	action_playnext,
	action_playnext_with_delay,
	action_setvolume
};


enum enum_play_status {
	play_status_new, play_status_actual, play_status_played
};

enum enum_flags {
	flags_none     = 0x0000,
	flags_repeat   = 0x0001,
	flags_shuffle  = 0x0002,
	flags_play_all = 0x0004
};

enum enum_actions {
	actions_none,
	actions_play,
	actions_stop,
	actions_next,
	actions_first
};

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
	int blink_cnt;
	int is_on;
#ifdef WITH_PRINING_MIDIFILES
	int printonly;
#endif
	int64_t starttime;
	t_midi_track *tracks;
} t_midi_song;

typedef struct PLAYLIST_ENTRY{
	char *path;
	int play_status;
	long sortkey;
	struct PLAYLIST_ENTRY *nxt;
} T_PLAYLIST_ENTRY;



// Prototypes
// gpio.c
void init_gpio();

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
void send_nrpn(unsigned int message, unsigned int val);
void midi_volume(int vol);

// MIDI file
int open_midifile(const char *filename);
int parse_midifile();
int handle_play_midifile(const char *filename, int with_delay, int with_full_volume);
int handle_print_midifile(const char *filename);
void handle_play_next_from_playlist(int force_repeat, int with_delay, int with_full_volume);
int handle_stop_midifile();
int handle_next_from_playlist();
void setVolume(int vol);
int getVolume();
char *sGetVolume();

// Start Fileserver
esp_err_t start_file_server(const char *base_path);

// sntp
void test_sntp();
void obtain_time(void);
void initialize_sntp(void);

// util
int random_number(int);

// playlist
void freeplaylist(void);
void restart_playlist(void);
void dump_playlist(void);
void update_playlist(void);
T_PLAYLIST_ENTRY *nxtentry(void);
T_PLAYLIST_ENTRY *actualplayedentry(void);
char *nxtplaylistentry(int force_repeat);
int setplaylistposition(const char *filename);
esp_err_t build_playlist(const char *dirpath);

// variables

#endif /* ESP32MIDI_MAIN_LOCAL_H_ */
