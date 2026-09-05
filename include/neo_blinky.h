#ifndef __NEO_BLINKY__
#define __NEO_BLINKY__
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

void rgb_setup();

void rgb_control(int color);

//void neo_blinky(void *pvParameters);

void neo_animation(void *pvParameters);


#endif