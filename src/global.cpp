#include "global.h"

float glob_temperature = 20;
float glob_humidity = 30;
float led_state = 0;
int fan_speed = 0;
int fan_state = 0;
bool system_state = false;

String WIFI_SSID = "";
String WIFI_PASS = "";

String AP_SSID = "SIC - Final_project - Group_7";
String AP_PASS = "12345678";
boolean isWifiConnected = false;

String mdnsHost = "smartfan";