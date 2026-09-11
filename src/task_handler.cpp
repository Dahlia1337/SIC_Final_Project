#include "task_handler.h"
#include "task_check_info.h"
#include "task_webserver.h"
#include "stepper_control.h"
#include "component_control.h"
#include "neo_blinky.h"

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
void update_auto_fan_logic() {
    if (!system_state) return; // Nếu đang MANUAL thì bỏ qua

    int target_speed = 0;
    int neo_color_hue = 0; // Hue (0 - 65535)

    switch (comfort_class) {
        case 0: // COLD -> Tắt quạt, đèn tím/xanh dương đậm
            target_speed = 0;
            neo_color_hue = 45000;
            break;
        case 1: // COMFORT -> Quạt thoang thoảng 35%, đèn xanh lá
            target_speed = 35;
            neo_color_hue = 21845;
            break;
        case 2: // WARM_HUMID -> Quạt 70%, đèn vàng/cam
            target_speed = 70;
            neo_color_hue = 10922;
            break;
        case 3: // HOT -> Quạt 100%, đèn đỏ
            target_speed = 100;
            neo_color_hue = 0;
            break;
    }

    if (fan_speed != target_speed) {
        fan_speed = target_speed;
        fan_control(fan_speed);
    }
    
    rgb_control(neo_color_hue);
}