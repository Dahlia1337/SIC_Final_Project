#include "temp_humi.h"
#include "power_history_logger.h"
#include "stepper_control.h"
#include "esp_heap_caps.h"

// Khai báo biến toàn cục

DHT dht(DHTPIN, DHTTYPE);

void Send_data_webserver (float temp, float humi);

void sensor_setup()
{
    dht.begin();
    Serial.println("---DHT22 sensor ready---");
}

void task_sensor(void *pvParameters)
{

    while (1)
    {
        float temperature = dht.readTemperature();
        float humidity = dht.readHumidity();

        if (isnan(temperature) || isnan(humidity))
        {
            Serial.println("Failed to read DHT!");
            temperature = 0;
            humidity = 0;
        }
        else
        {
            glob_temperature = temperature;
            glob_humidity = humidity;
        }

        update_auto_fan_logic();

        // Cập nhật mô-đun phân tích tiện nghi nhiệt và sức khỏe
        logger_update(glob_temperature, glob_humidity, comfort_class);

        Send_data_webserver(glob_temperature, glob_humidity);

        //Serial.printf("Hum: %.1f%%  Temp: %.1fC | Fan: %d%%\n", glob_humidity, glob_temperature, fan_speed);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void Send_data_webserver (float temp, float humi)
{
    JsonDocument doc;
    doc["type"] = "sensor";
    doc["temp"] = temp;
    doc["humi"] = humi;
    doc["comfort"] = comfort_class;
    doc["is_auto"] = system_state;
    
    switch (comfort_class) {
        case 0: doc["comfort_label"] = "Lạnh (COLD)"; break;
        case 1: doc["comfort_label"] = "Dễ chịu (COMFORT)"; break;
        case 2: doc["comfort_label"] = "Nóng ẩm (WARM)"; break;
        case 3: doc["comfort_label"] = "Nóng gắt (HOT)"; break;
        default: doc["comfort_label"] = "Đang phân tích..."; break;
    }

    // Bổ sung dữ liệu phân tích tiện nghi nhiệt & sức khỏe
    ComfortHealthMetrics h = logger_get_health();
    JsonObject hObj = doc["health"].to<JsonObject>();
    hObj["hi"] = serialized(String(h.heat_index, 1));
    hObj["dp"] = serialized(String(h.dew_point, 1));
    hObj["score"] = serialized(String(h.comfort_score, 1));
    hObj["hi_lvl"] = h.hi_risk_level;
    hObj["mold_lvl"] = h.mold_risk_level;
    hObj["p_cld"] = serialized(String(h.pct_cold, 1));
    hObj["p_cmf"] = serialized(String(h.pct_comfort, 1));
    hObj["p_wrm"] = serialized(String(h.pct_warm, 1));
    hObj["p_hot"] = serialized(String(h.pct_hot, 1));
    hObj["c_cld"] = h.count_cold;
    hObj["c_cmf"] = h.count_comfort;
    hObj["c_wrm"] = h.count_warm;
    hObj["c_hot"] = h.count_hot;
    hObj["total"] = h.count_total;

    // TinyML Intelligence Panel data
    JsonObject aiObj = doc["ai"].to<JsonObject>();
    // Confidence scores (xác suất 4 lớp phân loại)
    aiObj["p0"] = serialized(String(comfort_probs[0] * 100.0f, 1)); // COLD %
    aiObj["p1"] = serialized(String(comfort_probs[1] * 100.0f, 1)); // COMFORT %
    aiObj["p2"] = serialized(String(comfort_probs[2] * 100.0f, 1)); // WARM_HUMID %
    aiObj["p3"] = serialized(String(comfort_probs[3] * 100.0f, 1)); // HOT %
    // Confidence cao nhất (độ tự tin)
    float max_p = 0.0f;
    for (int i = 0; i < 4; i++) if (comfort_probs[i] > max_p) max_p = comfort_probs[i];
    aiObj["conf"] = serialized(String(max_p * 100.0f, 1));
    // Tốc độ thay đổi vi khí hậu (dT/dt, dH/dt — mỗi 2 giây)
    aiObj["dt"] = serialized(String(ai_delta_t, 2));
    aiObj["dh"] = serialized(String(ai_delta_h, 2));
    // Số lần inference & RAM heap còn trống
    aiObj["infer"] = (unsigned long)ai_inference_count;
    aiObj["heap"] = (uint32_t)esp_get_free_heap_size();
    // Phân bổ tích lũy (reuse từ logger)
    aiObj["c_cld"] = h.count_cold;
    aiObj["c_cmf"] = h.count_comfort;
    aiObj["c_wrm"] = h.count_warm;
    aiObj["c_hot"] = h.count_hot;
    aiObj["total"] = h.count_total;
    
    String output;
    serializeJson(doc, output);
    Webserver_sendata(output);
}
