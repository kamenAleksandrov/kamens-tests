/* ======================= LED CONTROL FUNCTIONS ======================= */
/*
 * This tiny file handles everything about the blue LED.
 * Keeping it separate makes the main program smaller and easier to read.
 */

#include "led_control.h"

#include "driver/gpio.h"

/*
 * We choose one GPIO pin to be our "blue LED".
 * On some ESP32 dev boards, GPIO2 has an on-board LED.
 * You might need to change this to the right pin for your board.
 */
#define LED_GPIO GPIO_NUM_32

/* We keep the current LED state in a global variable.
 * 0 = off, 1 = on.
 * Only this file touches the variable, so no other file can break it.
 */
static int s_led_on = 0;

void led_control_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO), /* which pin we use */
        .mode = GPIO_MODE_OUTPUT,                /* we will drive it, not read it */
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    /* Start with LED off - 0, Led on - 1. */
    led_control_set(0);
}

void led_control_set(int on)
{
    s_led_on = on ? 1 : 0;
    gpio_set_level(LED_GPIO, s_led_on);
}

int led_control_is_on(void)
{
    return s_led_on;
}
