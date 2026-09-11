#include "global.h"
#include "temp_humi.h"

// include task
#include "task_wifi.h"
#include "led_blinky.h"
#include "neo_blinky.h"
#include "task_check_info.h"
#include "task_webserver.h"
#include "component_control.h"
#include "task_toogle_boot.h"
#include "stepper_control.h"
#include "tinyml.h"

void setup()
{
    Serial.begin(115200);
    delay(5000);
    Serial.println("------ ESP32 start ------");
    check_info_File(0);
    // Delete_info_File();

    sensor_setup();
    rgb_setup();
    led_setup();
    fan_setup();
    stepper_init();

    WiFi_Init();
    Webserver_reconnect();

    xTaskCreate(Task_Toogle_BOOT,"Task Boot", 4096, NULL, 1, NULL);
    xTaskCreate(led_blinky, "Task LED Blink", 2048, NULL, 2, NULL);
    //xTaskCreate(neo_animation, "Task NEO Blink", 2048, NULL, 1, NULL);
    xTaskCreate(task_sensor, "Task TEMP HUMI", 4096, NULL, 2, NULL);
    xTaskCreate(task_stepper, "Task Stepper", 4096, NULL, 3, NULL);
    xTaskCreate(task_lcd, "Task LCD 1602", 4096, NULL, 1, NULL);
    xTaskCreate(tiny_ml_task, "Task TinyML", 8192, NULL, 4, NULL);
}

void loop()
{
    Webserver_reconnect();
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 5000)
    {
        lastCheck = millis();
        Wifi_reconnect();
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
}
