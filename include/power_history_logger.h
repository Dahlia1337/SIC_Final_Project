#ifndef __POWER_HISTORY_LOGGER_H__
#define __POWER_HISTORY_LOGGER_H__

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

struct HistoryPoint {
    uint32_t timestamp; // Unix timestamp hoặc millis()
    float temp;
    float humi;
};

struct PowerMetrics {
    float p_fan;          // W (Quạt PWM)
    float p_stepper;      // W (Động cơ bước đảo gió)
    float p_esp;          // W (ESP32-S3 + WiFi + TinyML)
    float p_peripherals;  // W (LCD 1602 + NeoPixel + DHT22)
    float p_total;        // W (Tổng công suất tức thời)
    float energy_wh;      // Wh (Điện năng tiêu thụ tích lũy)
    float saved_pct;      // % (Điện năng tiết kiệm được so với 100% quạt)
};

void logger_init();
void logger_update(float temp, float humi, int fan_pwm, bool is_swing);
PowerMetrics logger_get_power();
String logger_get_history_json(const String& range);
void logger_save_fs();

#endif // __POWER_HISTORY_LOGGER_H__
