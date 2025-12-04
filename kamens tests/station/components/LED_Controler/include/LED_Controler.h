#pragma once

#include "esp_err.h"

/* ======================= LED CONTROL HEADER ======================= */
/*
 * This header is the "instruction sheet" for the LED helper.
 * It tells other C files which LED functions exist so they can use them.
 */
void led_control_init(void);
void led_control_set(int on);
int led_control_is_on(void);
