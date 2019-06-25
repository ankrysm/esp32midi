/*
 * gpio.c
 *
 *  Created on: 20 Jun 2019
 *      Author: ankrysm
 *
 * from gpio_example_main.c
 */

#include "local.h"

static const char *TAG="gpio";

#define GPIO_INPUT_IO_0     GPIO_NUM_4
#define GPIO_INPUT_IO_1     GPIO_NUM_5
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0) | (1ULL<<GPIO_INPUT_IO_1))
#define ESP_INTR_FLAG_DEFAULT 0

#define GPIO_TIMER_MAX_COUNT 5

struct gpio_event {
	uint64_t pin;
	int32_t val_sum;
};

static xQueueHandle gpio_evt_queue = NULL;


static esp_timer_handle_t periodic_timer = NULL;
static int32_t timer_count=0;
static uint64_t gpio_timeout = 20000; // 20 ms
static uint64_t gpio_num = 0; // which GPIO is activated
static int32_t gpio_val_sum = 0;

static void periodic_timer_callback(void* arg) {

	int32_t val = gpio_get_level(gpio_num);

	// ESP_LOGI(TAG, "GPIO cnt=%d: pin: %lld val: %d", timer_count, gpio_num, val);

	gpio_val_sum +=val;

	if (timer_count++ >= GPIO_TIMER_MAX_COUNT) {
		struct gpio_event evt;
		evt.pin = gpio_num;
		evt.val_sum = gpio_val_sum;
		// all done, timer stop, send event
		esp_timer_stop(periodic_timer);
	    xQueueSendFromISR(gpio_evt_queue, &evt, NULL);

	    gpio_num = 0; // rearm ISR
	    gpio_val_sum = 0;
	}

}


/*
 * called dwhen the button is pressed
 */
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t the_gpio_num = (uint32_t) arg;
    if ( gpio_num > 0) {
    	return; // Timer is activated
    }
    gpio_num = the_gpio_num;
	timer_count = 0;

    //int32_t val = gpio_get_level(gpio_num) > 0 ? gpio_num : -gpio_num;
    //xQueueSendFromISR(gpio_evt_queue, &val, NULL);
    esp_err_t rc = esp_timer_start_periodic( periodic_timer, gpio_timeout);
    switch (rc) {
    case ESP_OK:
    	// started successfully
    	break;
    case ESP_ERR_INVALID_STATE:
    	// is already running, doesn't matter
    	break;
    case ESP_ERR_INVALID_ARG:
    	ESP_LOGE(TAG, "esp_timer_start failed: invalid args");
    	break;
    default:
    	ESP_LOGE(TAG, "esp_timer_start failed: rc=%d",rc);
    }
}


/**
 * handle butoon pressed events
 */
static void gpio_main_task(void* arg)
{
	struct gpio_event evt;
    ESP_LOGI(TAG, "Start gpio_task_example");

    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &evt, portMAX_DELAY)) {
        	ESP_LOGI(TAG, "GPIO val: %lld %d", evt.pin,evt.val_sum);
        	if ( evt.val_sum > 2) {
            	ESP_LOGI(TAG, "GPIO not an expected event");
        		continue;
        	}
        	ESP_LOGI(TAG, "GPIO raise event");
            char strftime_buf[64];
            time_t now;
            struct tm timeinfo;
            time(&now);

            localtime_r(&now, &timeinfo);
            strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        	ESP_LOGI(TAG, "%s GPIO raise event",strftime_buf);

        	// play without delay
        	handle_play_random_midifile(BASE_PATH, 0);
        }
    }
}

void init_gpio() {
    ESP_LOGI(TAG, "Start init_gpio");

    gpio_config_t io_conf;

    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_NEGEDGE; //GPIO_INTR_ANYEDGE; //GPIO_PIN_INTR_POSEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;

    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    //change gpio intrrupt type for one pin
    //gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_ANYEDGE);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(struct gpio_event));
    //start gpio task
    xTaskCreate(gpio_main_task, "gpio_task_example", 4096, NULL, 10, NULL);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void*) GPIO_INPUT_IO_1);

    // generate timer
	const esp_timer_create_args_t periodic_timer_args = {
			.callback =	&periodic_timer_callback,
			// name is optional, but may help identify the timer when debugging
			.name = "periodic_gpio"
			//.arg = (void*) &gpio_num
	};

	ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));


}
