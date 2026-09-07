#include "temp_humi.h"

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

        Send_data_webserver(glob_temperature, glob_humidity);

        Serial.printf("Hum: %.1f%%  Temp: %.1fC\n", glob_humidity, glob_temperature);

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
        case 2: doc["comfort_label"] = "Nóng ẩm (WARM_HUMID)"; break;
        case 3: doc["comfort_label"] = "Nóng gắt (HOT)"; break;
        default: doc["comfort_label"] = "Đang phân tích..."; break;
    }
    // {"type":"sensor","temp":30.4,"humi":70.9}
    
    String output;
    serializeJson(doc, output);
    Webserver_sendata(output);
}
