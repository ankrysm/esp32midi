/*
 * util.c
 *
 *  Created on: 9 May 2019
 *      Author: ankrysm
 */

#include "local.h"

#define BLUE_GPIO 2
void led_init() {
    gpio_pad_select_gpio(BLUE_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLUE_GPIO, GPIO_MODE_OUTPUT);
}

void blue_on() {
	gpio_set_level(BLUE_GPIO, 1);
}

void blue_off() {
	gpio_set_level(BLUE_GPIO, 0);
}


int random_number(int maxval) {
    uint32_t rr = esp_random();
    int r = maxval * (1.0 * rr / UINT32_MAX);
    return r;
}

