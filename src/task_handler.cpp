#include "task_handler.h"
#include "task_check_info.h"
#include "task_webserver.h"
#include "stepper_control.h"
#include "component_control.h"
#include <Preferences.h>

Preferences auto_prefs;

// Khai báo các ngưỡng nhiệt và tốc độ nấc Auto (lưu toàn cục)
float auto_t1 = 28.0;
int   auto_s1 = 50;
float auto_t2 = 32.0;
int   auto_s2 = 100;

// Hàm tải cấu hình Auto từ NVS khi ESP32 khởi động
void load_auto_config()
{
    auto_prefs.begin("fan_auto", false);
    auto_t1 = auto_prefs.getFloat("t1", 28.0);
    auto_s1 = auto_prefs.getInt("s1", 50);
    auto_t2 = auto_prefs.getFloat("t2", 32.0);
    auto_s2 = auto_prefs.getInt("s2", 100);
    auto_prefs.end();

    Serial.printf("⚙️ [Auto Config Loaded]: T1=%.1fC -> %d%% | T2=%.1fC -> %d%%\n",
                  auto_t1, auto_s1, auto_t2, auto_s2);
}

void handleWebSocketMessage(String message)
{
    Serial.println("📩 [WS Inbound]: " + message);
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, message);
    if (error)
    {
        Serial.print("❌ Lỗi parse JSON: ");
        Serial.println(error.c_str());
        return;
    }

    // 1. Gói tin điều khiển hoạt động quạt & stepper
    if (doc.containsKey("cmd"))
    {
        String cmd = doc["cmd"].as<String>();

        if (cmd == "control")
        {
            String mode = doc["mode"] | "MANUAL";
            int speed = doc["fan_speed"] | 0;
            bool swing = doc["swing_enable"] | false;
            int angle = doc["target_angle"] | 0; // Mặc định 0 độ theo dải [-90, 90]

            Serial.printf("🎯 Mode: %s | Fan: %d%% | Swing: %s | Angle: %d°\n",
                          mode.c_str(), speed, swing ? "ON" : "OFF", angle);

            // Chuyển chế độ Auto / Manual
            if (mode == "AUTO") {
                system_state = true;
            } else {
                system_state = false;
                fan_speed = speed;
                fan_control(speed);
            }

            // Điều khiển góc và đảo gió (hoạt động độc lập cả khi Auto)
            stepper_toggle_swing(swing);
            if (!swing) {
                stepper_set_angle(angle);
            }
        }
        // Nhận cài đặt ngưỡng 2 nấc nhiệt độ Auto
        else if (cmd == "set_auto_cfg")
        {
            auto_t1 = doc["t1"] | 28.0;
            auto_s1 = doc["s1"] | 50;
            auto_t2 = doc["t2"] | 32.0;
            auto_s2 = doc["s2"] | 100;

            // Lưu trực tiếp vào Preferences (NVS)
            auto_prefs.begin("fan_auto", false);
            auto_prefs.putFloat("t1", auto_t1);
            auto_prefs.putInt("s1", auto_s1);
            auto_prefs.putFloat("t2", auto_t2);
            auto_prefs.putInt("s2", auto_s2);
            auto_prefs.end();

            Serial.printf("💾 Đã lưu cấu hình Auto: T1=%.1fC -> %d%% | T2=%.1fC -> %d%%\n",
                          auto_t1, auto_s1, auto_t2, auto_s2);
        }
        else if (cmd == "calib_zero")
        {
            Serial.println("⚙️ Nhận lệnh đặt lại mốc 0°");
            stepper_calibrate_zero();
        }
    }
    // 2. Gói tin cấu hình WiFi
    else if (doc["page"] == "setting")
    {
        String ssid = doc["value"]["ssid"] | "";
        String pass = doc["value"]["password"] | "";
        
        Serial.println("📥 Đã nhận cấu hình WiFi mới từ Web:");
        Serial.println(" - SSID: " + ssid);
        Serial.println(" - PASS: " + pass);

        Save_info_File(ssid, pass);

        String resp = "{\"page\":\"setting_saved\"}";
        Webserver_sendata(resp);

        vTaskDelay(1500 / portTICK_PERIOD_MS);
        ESP.restart();
    }
}

// Logic kiểm tra & cập nhật tốc độ tự động (gọi tuần tự theo chu kỳ đo cảm biến)
void update_auto_fan_logic()
{
    if (!system_state) return; // Đang ở MANUAL thì bỏ qua

    const float HYSTERESIS = 0.5; // Khoảng trễ nhiệt độ tránh bật tắt liên tục
    int target_speed = 0;

    // Phân nấc nhiệt độ với khoảng trễ
    if (glob_temperature >= auto_t2) {
        target_speed = auto_s2; // Nấc 2 (Mạnh)
    } 
    else if (glob_temperature >= auto_t1) {
        // Nếu nhiệt độ rớt xuống dưới T2 nhưng chưa thấp hơn (T2 - HYSTERESIS)
        // và trước đó đang chạy nấc 2 thì giữ nấc 2
        if (fan_speed == auto_s2 && glob_temperature > (auto_t2 - HYSTERESIS)) {
            target_speed = auto_s2;
        } else {
            target_speed = auto_s1; // Nấc 1 (Vừa)
        }
    } 
    else {
        // Nếu trước đó đang chạy nấc 1 nhưng nhiệt độ chưa rớt qua (T1 - HYSTERESIS)
        // thì vẫn giữ nấc 1
        if (fan_speed == auto_s1 && glob_temperature > (auto_t1 - HYSTERESIS)) {
            target_speed = auto_s1;
        } else {
            target_speed = 0; // Dưới T1 tắt quạt
        }
    }

    // Chỉ xuất xung PWM và cập nhật khi tốc độ thay đổi
    if (fan_speed != target_speed) {
        fan_speed = target_speed;
        fan_control(fan_speed);
        Serial.printf("🤖 [Auto Action] Temp: %.1fC -> Fan set: %d%%\n", glob_temperature, fan_speed);
    }
}