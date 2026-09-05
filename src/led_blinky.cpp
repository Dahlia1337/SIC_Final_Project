#include "led_blinky.h"
#include "component_control.h"

void led_setup()
{
    pinMode(LED1_PIN, OUTPUT);
    pinMode(LED2_PIN, OUTPUT);
    digitalWrite(LED1_PIN, HIGH);
    digitalWrite(LED2_PIN, HIGH);
};

void led_blinky(void *pvParameters)
{

    while (1)
    {   
        digitalWrite(LED1_PIN, LOW);
        digitalWrite(LED2_PIN, HIGH);
        vTaskDelay(pdMS_TO_TICKS(1000));

        digitalWrite(LED1_PIN, HIGH);
        digitalWrite(LED2_PIN, LOW);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
};

void ledControl(int state)
{
    if (state == 0)
    {
        digitalWrite(LED2_PIN, HIGH);
    }
    else
    {
        digitalWrite(LED2_PIN, LOW);
    }
}
