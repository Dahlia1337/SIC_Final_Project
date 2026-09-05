#ifndef __LED_BLINKY__
#define __LED_BLINKY__
#include <Arduino.h>
#include "global.h"

void led_setup();
void led_blinky(void *pvParameters);
void ledControl(int state);

#endif