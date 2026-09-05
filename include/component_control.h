#ifndef __COMPONENT_CONTROL_H__
#define __COMPONENT_CONTROL_H__

// #include <HardwareSerial.h>
#include <Arduino.h>
#include <Wire.h>
#include "LiquidCrystal_I2C.h"
#include "stepper_control.h"

#include "global.h"

void component_reset();

void lcd_setup();
void task_lcd(void *pvParameters);


void fan_setup();
void fan_control(int state);


#endif