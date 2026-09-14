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

struct ComfortHealthMetrics {
    float heat_index;        // °C (Chỉ số cảm nhận nhiệt NOAA)
    float dew_point;         // °C (Nhiệt độ điểm sương Magnus)
    float comfort_score;     // 0 - 100% (Điểm chất lượng vi khí hậu)
    uint8_t hi_risk_level;   // 0: An toàn (<27°C), 1: Caution (27-32°C), 2: Extreme caution (32-41°C), 3: Danger (41-54°C), 4: Extreme danger (>=54°C)
    uint8_t mold_risk_level; // 0: Khô thoáng, 1: Cảnh giác, 2: Nguy cơ nồm ẩm cao
    
    // Thống kê phân lớp TinyML tích lũy
    uint32_t count_total;
    uint32_t count_cold;
    uint32_t count_comfort;
    uint32_t count_warm;
    uint32_t count_hot;

    float pct_cold;
    float pct_comfort;
    float pct_warm;
    float pct_hot;
};

void logger_init();
void logger_update(float temp, float humi, int comfort_class);
ComfortHealthMetrics logger_get_health();
String logger_get_history_json(const String& range);
void logger_save_fs();

#endif // __POWER_HISTORY_LOGGER_H__
