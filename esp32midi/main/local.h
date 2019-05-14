/*
 * local.h
 *
 *  Created on: 9 May 2019
 *      Author: ankrysm
 */

#ifndef BLINK_ESP32_MAIN_LOCAL_H_
#define BLINK_ESP32_MAIN_LOCAL_H_

// Strukturen
typedef struct {
	int  datalen;
    const char data[10];  // MIDI-Daten
} t_midi_data;

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

// Start Fileserver
esp_err_t start_file_server(const char *base_path);


#endif /* BLINK_ESP32_MAIN_LOCAL_H_ */
