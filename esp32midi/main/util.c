/*
 * util.c
 *
 *  Created on: 9 May 2019
 *      Author: ankrysm
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
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




