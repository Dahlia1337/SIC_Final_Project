#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <WiFi.h>

#define BOOT_PIN        0

#define LED1_PIN        43
#define LED2_PIN        44
#define NEO_PIN         48

#define FAN_PIN         5
#define FAN_IN1         6
#define FAN_IN2         7

#define DHTPIN          4
#define DHTTYPE         DHT22

#define LCD_SDA_PIN     11
#define LCD_SCL_PIN     12
#define LCD_I2C_ADDR    0x27

extern float glob_temperature;
extern float glob_humidity;
extern float led_state;
extern int fan_speed;
extern int fan_state;
extern bool system_state;

extern String WIFI_SSID;
extern String WIFI_PASS;

extern String AP_SSID;
extern String AP_PASS;
extern boolean isWifiConnected;

extern String mdnsHost;

extern bool isAPMode;
extern unsigned long sta_connected_millis;

extern int comfort_class; // 0: COLD, 1: COMFORT, 2: WARM_HUMID, 3: HOT

#endif