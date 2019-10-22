/*
 * midi_util.c
 *
 *  Created on: 9 May 2019
 *      Author: ankrysm
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
//#include "esp_sleep.h"
#include "local.h"

#define MIDI_TXD  (GPIO_NUM_17)
#define MIDI_RXD  (GPIO_NUM_16)
#define MIDI_RTS  (UART_PIN_NO_CHANGE)
#define MIDI_CTS  (UART_PIN_NO_CHANGE)

static const char* TAG = "midi_util";

#define BUF_SIZE (1024)

static esp_timer_handle_t periodic_timer = NULL;

// predefined signals
static t_midi_data okdata[]={
		{2,{0xC0, 14}},
		{3,{0x90, 72, 0x40}},
		{3,{0x90,76,0x40}},
		{3,{ 0x90, 79,0x40}},
		{-1,{0}} // Ende
};

static t_midi_data errdata[]={
		{2, {0xC0, 81}},
		{3, {0x90, 81, 0x40}},
		{3, {0x90, 76, 0x40}},
		{3, {0x90, 81, 0x40}},
		{3, {0x90, 76, 0x40}},
		{3, {0x90, 81, 0x40}},
		{0, {0}},
		{0, {0}},
		{6, {0x90, 76, 00, 0x90, 81 ,00}},
		{-1,{0}} // Ende
};

static int pos=0;
static t_midi_data *data = NULL;


void midi_out( const char *data, int len) {
    uart_write_bytes(UART_NUM_2, data, len);
}

void midi_out_evt( const char evt, const char *data, int len) {
    uart_write_bytes(UART_NUM_2, &evt, 1);
    uart_write_bytes(UART_NUM_2, data, len);
}

void midi_reset() {
    char *init_sequenz = calloc(40, sizeof(char));
    int pos=0;
    init_sequenz[pos++]=0xFF; //Midi-Reset
    for ( int i=0xC0; i <= 0xCF; i++) {
    	init_sequenz[pos++]=i;
    	init_sequenz[pos++]=0x00;
    }
    ESP_ERROR_CHECK(pos >= 40);

    midi_out(init_sequenz, pos );
    free(init_sequenz);

}


void send_nrpn(unsigned int message, unsigned int val) {
	/*
	 * using the 0xB0 Control Change command a NRPN-Message is transmitted;
	 * the control 99 (0x63) is set to the high byte
	 * the control 98 (0x62) is set to the low byte
	 * the control 06 (0x06) is set to the new value
	 *
	 * s. SAM2695 doc
	 */
	//               0     1   2     3     4   5     6     7   8
	char nrpn[]= {0xB0, 0x63, 00, 0xB0, 0x62, 00, 0xB0, 0x06, 00};

	nrpn[2] = (message & 0xFF00) >> 8;
	nrpn[5] = message & 0xFF;
	nrpn[8] = val > 0x7F ? 0x7F : val; // max. 0x7F

    uart_write_bytes(UART_NUM_2, nrpn, sizeof(nrpn));

    ESP_LOGI(TAG, "setvol(%x,%x) %x %x %x %x %x %x %x %x %x",message, val,nrpn[0],nrpn[1],nrpn[2],nrpn[3],nrpn[4],nrpn[5],nrpn[6],nrpn[7],nrpn[8]);

}

// TODO maybe there are problems while setting the value during play
void midi_volume(int vol) {
	/*
	 *  via NRPN 0x3707
	 */
	send_nrpn(0X3707, vol);
}

void midi_init() {
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 31250,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_2, &uart_config);
    uart_set_pin(UART_NUM_2, MIDI_TXD, MIDI_RXD, MIDI_RTS, MIDI_CTS);
    uart_driver_install(UART_NUM_2, BUF_SIZE * 2, 0, 0, NULL, 0);

    midi_reset();
    midi_volume(0x7f);
}

static void periodic_timer_callback(void* arg)
{
	//int64_t time_since_boot = esp_timer_get_time();

	t_midi_data *evt = &(data[pos]);
	int l = evt->datalen;

	//ESP_LOGI(TAG, "Periodic timer called, time since boot: %lld us, pos=%d l=%d", time_since_boot, pos, l);
	if (l < 0) {
		// Schluss, Timer stoppen
		ESP_ERROR_CHECK(esp_timer_stop(periodic_timer));
		//ESP_LOGI(TAG, "Periodic timer stopped");
		return;
	} else if (l > 0) {
		midi_out(evt->data, l);
	} else {
		//ESP_LOGI(TAG, "Pause");
	}
	pos++;

}

void play_arr(int ticks, t_midi_data arr[]) {

	if (periodic_timer == NULL) {
		// timer muss erzeugt werden
		const esp_timer_create_args_t periodic_timer_args = {
				.callback =	&periodic_timer_callback,
				// name is optional, but may help identify the timer when debugging
				.name = "periodic"
		};

		ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
	} else {
		// Timer stoppen, wenn er läuft
		esp_timer_stop(periodic_timer);
	}
	pos=0;
	data=&arr[0]; //arr;

    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, ticks*1000));

}

void play_ok() {
	play_arr(250, okdata);
}

void play_err() {
	play_arr(100, errdata);
}


