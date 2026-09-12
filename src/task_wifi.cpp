#include "task_wifi.h"

bool isAPMode = false;

void startAP()
{
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID.c_str(), AP_PASS.c_str());
    isAPMode = true;
    Serial.println("📡 Đã bật chế độ AP!");
    Serial.print("👉 Truy cập IP AP để cài đặt WiFi: ");
    Serial.println(WiFi.softAPIP());
}

bool startSTA()
{

    if (WIFI_SSID.isEmpty())
    {
        Serial.println("⚠️ Chưa có cấu hình SSID!");
        return false;
    }

    Serial.printf("🔄 Đang kết nối tới WiFi: %s...\n", WIFI_SSID.c_str());
    WiFi.mode(WIFI_STA);

    if (WIFI_PASS.isEmpty())
    {
        WiFi.begin(WIFI_SSID.c_str());
    }
    else
    {
        WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
    }

    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 200)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        Serial.print(".");
        timeout++;
    }
    Serial.println();
    

    if (WiFi.status() == WL_CONNECTED)
    {
        isAPMode = false;
        Serial.print("✅ STA Đã kết nối. Local IP: ");
        Serial.println(WiFi.localIP());

        if (MDNS.begin(mdnsHost)) {
            MDNS.addService("http", "tcp", 80);
            Serial.printf("🌐 mDNS đã chạy! Bạn có thể truy cập qua: http://%s.local\n", mdnsHost);
        } else {
            Serial.println("❌ Lỗi khởi động mDNS!");
        }

        // Đồng bộ thời gian thực qua NTP (Múi giờ GMT+7 Việt Nam)
        configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com");
        Serial.println("⏰ Đang đồng bộ thời gian NTP...");
        
        return true;
    }
    else
    {
        Serial.println("❌ STA Kết nối Thất bại/Timeout.");
        return true;
    }
}

void WiFi_Init()
{
    if (!startSTA())
    {
        startAP();
    }
}

bool Wifi_reconnect()
{
    if (isAPMode) return true;

    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    Serial.println("⚠️ Mất kết nối WiFi STA! Đang thử kết nối lại...");
    return startSTA();
}
