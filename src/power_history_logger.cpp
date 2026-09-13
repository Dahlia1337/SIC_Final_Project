#include "power_history_logger.h"
#include <time.h>

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

static PowerMetrics current_power = {0};
static unsigned long last_update_ms = 0;
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

void logger_init() {
    current_power.energy_wh = 0.0f;
    last_update_ms = millis();
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
                current_power.energy_wh = doc["energy_wh"] | 0.0f;

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

                Serial.printf("✅ Đã khôi phục lịch sử từ LittleFS: 1h(%d), 24h(%d), 7d(%d), 30d(%d)\n",
                              count_1h, count_24h, count_7d, count_30d);
            }
        }
    }
}

void logger_update(float temp, float humi, int fan_pwm, bool is_swing) {
    unsigned long now_ms = millis();
    float delta_s = (now_ms - last_update_ms) / 1000.0f;
    if (delta_s <= 0 || delta_s > 60.0f) delta_s = 2.0f;
    last_update_ms = now_ms;

    // 1. Tính toán công suất từng thành phần
    float fan_ratio = (float)fan_pwm / 100.0f;
    if (fan_ratio > 1.0f) fan_ratio = 1.0f;
    if (fan_ratio < 0.0f) fan_ratio = 0.0f;

    // Công suất quạt (tối đa ~1.5W ở 100%)
    current_power.p_fan = 1.50f * powf(fan_ratio, 1.25f);

    // Công suất động cơ bước đảo gió (khoảng ~0.9W khi đang quét)
    current_power.p_stepper = is_swing ? 0.90f : 0.0f;

    // Công suất ESP32-S3 + WiFi + TinyML (trung bình ~0.50W)
    current_power.p_esp = 0.50f;

    // Công suất thiết bị ngoại vi: LCD 1602 (0.12W) + NeoPixel (0.08W) + DHT22 (0.01W) ~ 0.20W
    current_power.p_peripherals = 0.20f;

    // Tổng công suất tức thời
    current_power.p_total = current_power.p_fan + current_power.p_stepper + current_power.p_esp + current_power.p_peripherals;

    // Tích phân điện năng tiêu thụ (Wh)
    current_power.energy_wh += current_power.p_total * (delta_s / 3600.0f);

    // Tính % điện năng tiết kiệm được nhờ AI (so với chạy 100% quạt + đảo gió liên tục ~3.1W)
    float p_benchmark = 1.50f + 0.90f + 0.50f + 0.20f; // 3.10W
    float saved = ((p_benchmark - current_power.p_total) / p_benchmark) * 100.0f;
    current_power.saved_pct = (saved > 0.0f) ? saved : 0.0f;

    uint32_t ts = get_current_ts();

    // 2. Ghi mẫu 1 Giờ (Mỗi 60 giây = 1 phút)
    if (now_ms - last_1h_sample_ms >= 60000UL || count_1h == 0) {
        last_1h_sample_ms = now_ms;
        push_point(buf_1h, SIZE_1H, head_1h, count_1h, temp, humi, ts);
    }

    // 3. Ghi mẫu 24 Giờ (Mỗi 15 phút = 900.000 ms)
    if (now_ms - last_24h_sample_ms >= 900000UL || count_24h == 0) {
        last_24h_sample_ms = now_ms;
        push_point(buf_24h, SIZE_24H, head_24h, count_24h, temp, humi, ts);
    }

    // 4. Ghi mẫu 7 Ngày (Mỗi 2 giờ = 7.200.000 ms)
    if (now_ms - last_7d_sample_ms >= 7200000UL || count_7d == 0) {
        last_7d_sample_ms = now_ms;
        push_point(buf_7d, SIZE_7D, head_7d, count_7d, temp, humi, ts);
    }

    // 5. Ghi mẫu 30 Ngày (Mỗi 6 giờ = 21.600.000 ms)
    if (now_ms - last_30d_sample_ms >= 21600000UL || count_30d == 0) {
        last_30d_sample_ms = now_ms;
        push_point(buf_30d, SIZE_30D, head_30d, count_30d, temp, humi, ts);
    }

    // 6. Định kỳ lưu LittleFS mỗi 10 phút để tránh chai Flash
    if (now_ms - last_fs_save_ms >= 600000UL) {
        last_fs_save_ms = now_ms;
        logger_save_fs();
    }
}

PowerMetrics logger_get_power() {
    return current_power;
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
    doc["energy_wh"] = serialized(String(current_power.energy_wh, 2));

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
    Serial.println("💾 Đã lưu lịch sử vi khí hậu và điện năng xuống LittleFS.");
}
