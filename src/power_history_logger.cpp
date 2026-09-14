#include "power_history_logger.h"
#include <time.h>
#include <math.h>

#define SIZE_1H   60   // 60 điểm (mỗi 1 phút)
#define SIZE_24H  96   // 96 điểm (mỗi 15 phút)
#define SIZE_7D   84   // 84 điểm (mỗi 2 giờ)
#define SIZE_30D  120  // 120 điểm (mỗi 6 giờ)

static HistoryPoint buf_1h[SIZE_1H];
static int count_1h = 0;
static int head_1h = 0;

static HistoryPoint buf_24h[SIZE_24H];
static int count_24h = 0;
static int head_24h = 0;

static HistoryPoint buf_7d[SIZE_7D];
static int count_7d = 0;
static int head_7d = 0;

static HistoryPoint buf_30d[SIZE_30D];
static int count_30d = 0;
static int head_30d = 0;

static ComfortHealthMetrics current_health = {0};
static unsigned long last_1h_sample_ms = 0;
static unsigned long last_24h_sample_ms = 0;
static unsigned long last_7d_sample_ms = 0;
static unsigned long last_30d_sample_ms = 0;
static unsigned long last_fs_save_ms = 0;

static const char* HISTORY_FILE = "/history.json";

static void push_point(HistoryPoint* buf, int max_size, int& head, int& count, float temp, float humi, uint32_t ts) {
    buf[head].timestamp = ts;
    buf[head].temp = temp;
    buf[head].humi = humi;
    head = (head + 1) % max_size;
    if (count < max_size) count++;
}

static uint32_t get_current_ts() {
    time_t now;
    time(&now);
    if (now > 1600000000) {
        return (uint32_t)now;
    }
    return (uint32_t)(millis() / 1000);
}

// Công thức tính Heat Index (NOAA Rothfusz regression)
static float compute_heat_index(float t_c, float rh) {
    if (t_c < 20.0f) return t_c;
    float t_f = t_c * 1.8f + 32.0f;
    float hi_f = 0.5f * (t_f + 61.0f + ((t_f - 68.0f) * 1.2f) + (rh * 0.094f));
    if (hi_f >= 80.0f) {
        hi_f = -42.379f + 2.04901523f * t_f + 10.14333127f * rh
               - 0.22475541f * t_f * rh - 0.00683783f * t_f * t_f
               - 0.05481717f * rh * rh + 0.00122874f * t_f * t_f * rh
               + 0.00085282f * t_f * rh * rh - 0.00000199f * t_f * t_f * rh * rh;
        if (rh < 13.0f && t_f >= 80.0f && t_f <= 112.0f) {
            hi_f -= ((13.0f - rh) / 4.0f) * sqrtf((17.0f - fabsf(t_f - 95.0f)) / 17.0f);
        } else if (rh > 85.0f && t_f >= 80.0f && t_f <= 87.0f) {
            hi_f += ((rh - 85.0f) / 10.0f) * ((87.0f - t_f) / 5.0f);
        }
    }
    return (hi_f - 32.0f) / 1.8f;
}

// Công thức tính Nhiệt độ điểm sương (Magnus-Tetens)
static float compute_dew_point(float t_c, float rh) {
    if (rh <= 0.0f) rh = 0.01f;
    if (rh > 100.0f) rh = 100.0f;
    const float a = 17.27f;
    const float b = 237.7f;
    float alpha = ((a * t_c) / (b + t_c)) + logf(rh / 100.0f);
    return (b * alpha) / (a - alpha);
}

void logger_init() {
    memset(&current_health, 0, sizeof(current_health));
    current_health.comfort_score = 100.0f;
    current_health.pct_comfort = 100.0f;

    last_1h_sample_ms = millis();
    last_24h_sample_ms = millis();
    last_7d_sample_ms = millis();
    last_30d_sample_ms = millis();
    last_fs_save_ms = millis();

    if (!LittleFS.begin(true)) {
        Serial.println("❌ Lỗi mount LittleFS trong logger!");
        return;
    }

    // Đọc lịch sử đã lưu từ LittleFS
    if (LittleFS.exists(HISTORY_FILE)) {
        File file = LittleFS.open(HISTORY_FILE, "r");
        if (file) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, file);
            file.close();
            if (!err) {
                current_health.count_total = doc["cnt_tot"] | 0;
                current_health.count_cold = doc["cnt_cld"] | 0;
                current_health.count_comfort = doc["cnt_cmf"] | 0;
                current_health.count_warm = doc["cnt_wrm"] | 0;
                current_health.count_hot = doc["cnt_hot"] | 0;

                // Load 1h
                JsonArray arr1h = doc["h1"];
                if (arr1h) {
                    for (JsonObject obj : arr1h) {
                        push_point(buf_1h, SIZE_1H, head_1h, count_1h, obj["t"], obj["h"], obj["ts"]);
                    }
                }

                // Load 24h
                JsonArray arr24h = doc["h24"];
                if (arr24h) {
                    for (JsonObject obj : arr24h) {
                        push_point(buf_24h, SIZE_24H, head_24h, count_24h, obj["t"], obj["h"], obj["ts"]);
                    }
                }

                // Load 7d
                JsonArray arr7d = doc["d7"];
                if (arr7d) {
                    for (JsonObject obj : arr7d) {
                        push_point(buf_7d, SIZE_7D, head_7d, count_7d, obj["t"], obj["h"], obj["ts"]);
                    }
                }

                // Load 30d
                JsonArray arr30d = doc["d30"];
                if (arr30d) {
                    for (JsonObject obj : arr30d) {
                        push_point(buf_30d, SIZE_30D, head_30d, count_30d, obj["t"], obj["h"], obj["ts"]);
                    }
                }

                Serial.printf("✅ Đã khôi phục lịch sử từ LittleFS: 1h(%d), 24h(%d), 7d(%d), 30d(%d), tổng mẫu(%lu)\n",
                              count_1h, count_24h, count_7d, count_30d, (unsigned long)current_health.count_total);
            }
        }
    }
}

void logger_update(float temp, float humi, int comfort_class) {
    unsigned long now_ms = millis();

    // 1. Tính toán Heat Index & Phân cấp rủi ro sức khỏe NOAA NWS
    current_health.heat_index = compute_heat_index(temp, humi);
    if (current_health.heat_index < 27.0f) {
        current_health.hi_risk_level = 0; // An toàn (< 80°F / < 27°C)
    } else if (current_health.heat_index < 32.0f) {
        current_health.hi_risk_level = 1; // Caution (80–90°F / 27–32°C)
    } else if (current_health.heat_index < 41.0f) {
        current_health.hi_risk_level = 2; // Extreme caution (90–105°F / 32–41°C)
    } else if (current_health.heat_index < 54.0f) {
        current_health.hi_risk_level = 3; // Danger (105–129°F / 41–54°C)
    } else {
        current_health.hi_risk_level = 4; // Extreme danger (>= 130°F / >= 54°C)
    }

    // 2. Tính toán Nhiệt độ điểm sương & Đánh giá nguy cơ nồm ẩm
    current_health.dew_point = compute_dew_point(temp, humi);
    float diff = temp - current_health.dew_point;
    if (diff <= 2.0f || humi >= 80.0f) {
        current_health.mold_risk_level = 2; // Cao (Nguy cơ nồm ẩm & ngưng tụ)
    } else if (diff <= 4.0f || humi >= 70.0f) {
        current_health.mold_risk_level = 1; // Cảnh giác (Trung bình)
    } else {
        current_health.mold_risk_level = 0; // Thấp / Khô thoáng
    }

    // 3. Tích lũy phân loại của TinyML & Tính Điểm Tiện Nghi (Comfort Score)
    if (comfort_class >= 0 && comfort_class <= 3) {
        current_health.count_total++;
        if (comfort_class == 0) current_health.count_cold++;
        else if (comfort_class == 1) current_health.count_comfort++;
        else if (comfort_class == 2) current_health.count_warm++;
        else if (comfort_class == 3) current_health.count_hot++;
    }

    if (current_health.count_total > 0) {
        float score_sum = (current_health.count_comfort * 1.0f) +
                          (current_health.count_cold * 0.6f) +
                          (current_health.count_warm * 0.4f) +
                          (current_health.count_hot * 0.1f);
        current_health.comfort_score = (score_sum / (float)current_health.count_total) * 100.0f;
        current_health.pct_cold = ((float)current_health.count_cold * 100.0f) / current_health.count_total;
        current_health.pct_comfort = ((float)current_health.count_comfort * 100.0f) / current_health.count_total;
        current_health.pct_warm = ((float)current_health.count_warm * 100.0f) / current_health.count_total;
        current_health.pct_hot = ((float)current_health.count_hot * 100.0f) / current_health.count_total;
    } else {
        current_health.comfort_score = 100.0f;
        current_health.pct_comfort = 100.0f;
    }

    uint32_t ts = get_current_ts();

    // 4. Ghi mẫu 1 Giờ (Mỗi 60 giây = 1 phút)
    if (now_ms - last_1h_sample_ms >= 60000UL || count_1h == 0) {
        last_1h_sample_ms = now_ms;
        push_point(buf_1h, SIZE_1H, head_1h, count_1h, temp, humi, ts);
    }

    // 5. Ghi mẫu 24 Giờ (Mỗi 15 phút = 900.000 ms)
    if (now_ms - last_24h_sample_ms >= 900000UL || count_24h == 0) {
        last_24h_sample_ms = now_ms;
        push_point(buf_24h, SIZE_24H, head_24h, count_24h, temp, humi, ts);
    }

    // 6. Ghi mẫu 7 Ngày (Mỗi 2 giờ = 7.200.000 ms)
    if (now_ms - last_7d_sample_ms >= 7200000UL || count_7d == 0) {
        last_7d_sample_ms = now_ms;
        push_point(buf_7d, SIZE_7D, head_7d, count_7d, temp, humi, ts);
    }

    // 7. Ghi mẫu 30 Ngày (Mỗi 6 giờ = 21.600.000 ms)
    if (now_ms - last_30d_sample_ms >= 21600000UL || count_30d == 0) {
        last_30d_sample_ms = now_ms;
        push_point(buf_30d, SIZE_30D, head_30d, count_30d, temp, humi, ts);
    }

    // 8. Định kỳ lưu LittleFS mỗi 10 phút để tránh chai Flash
    if (now_ms - last_fs_save_ms >= 600000UL) {
        last_fs_save_ms = now_ms;
        logger_save_fs();
    }
}

ComfortHealthMetrics logger_get_health() {
    return current_health;
}

static void serialize_buffer(JsonArray& arr, HistoryPoint* buf, int max_size, int head, int count) {
    int start = (count < max_size) ? 0 : head;
    for (int i = 0; i < count; i++) {
        int idx = (start + i) % max_size;
        JsonObject obj = arr.add<JsonObject>();
        obj["ts"] = buf[idx].timestamp;
        obj["t"] = serialized(String(buf[idx].temp, 1));
        obj["h"] = serialized(String(buf[idx].humi, 1));
    }
}

String logger_get_history_json(const String& range) {
    JsonDocument doc;
    JsonArray dataArr = doc["data"].to<JsonArray>();

    if (range == "1h") {
        serialize_buffer(dataArr, buf_1h, SIZE_1H, head_1h, count_1h);
    } else if (range == "24h") {
        serialize_buffer(dataArr, buf_24h, SIZE_24H, head_24h, count_24h);
    } else if (range == "7d") {
        serialize_buffer(dataArr, buf_7d, SIZE_7D, head_7d, count_7d);
    } else if (range == "30d") {
        serialize_buffer(dataArr, buf_30d, SIZE_30D, head_30d, count_30d);
    } else {
        serialize_buffer(dataArr, buf_1h, SIZE_1H, head_1h, count_1h);
    }

    doc["range"] = range;
    doc["count"] = dataArr.size();

    String out;
    serializeJson(doc, out);
    return out;
}

void logger_save_fs() {
    File file = LittleFS.open(HISTORY_FILE, "w");
    if (!file) return;

    JsonDocument doc;
    doc["cnt_tot"] = current_health.count_total;
    doc["cnt_cld"] = current_health.count_cold;
    doc["cnt_cmf"] = current_health.count_comfort;
    doc["cnt_wrm"] = current_health.count_warm;
    doc["cnt_hot"] = current_health.count_hot;

    JsonArray arr1h = doc["h1"].to<JsonArray>();
    serialize_buffer(arr1h, buf_1h, SIZE_1H, head_1h, count_1h);

    JsonArray arr24h = doc["h24"].to<JsonArray>();
    serialize_buffer(arr24h, buf_24h, SIZE_24H, head_24h, count_24h);

    JsonArray arr7d = doc["d7"].to<JsonArray>();
    serialize_buffer(arr7d, buf_7d, SIZE_7D, head_7d, count_7d);

    JsonArray arr30d = doc["d30"].to<JsonArray>();
    serialize_buffer(arr30d, buf_30d, SIZE_30D, head_30d, count_30d);

    serializeJson(doc, file);
    file.close();
    Serial.println("💾 Đã lưu thống kê tiện nghi sức khỏe và lịch sử vi khí hậu xuống LittleFS.");
}
