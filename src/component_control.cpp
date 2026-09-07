#include "component_control.h"

LiquidCrystal_I2C lcd(LCD_I2C_ADDR, 16, 2);

unsigned long sta_connected_millis = 0;

void fan_setup()
{
    pinMode(FAN_IN1, OUTPUT);
    pinMode(FAN_IN2, OUTPUT);
    pinMode(FAN_PIN, OUTPUT);
    digitalWrite(FAN_IN1, LOW);
    digitalWrite(FAN_IN2, HIGH);
};

void fan_control(int state)
{
    int speed = map(state, 0, 100, 0, 255);
    analogWrite(FAN_PIN, speed);
};

void lcd_setup(){
    Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
    lcd.begin();
    lcd.backlight();
    lcd.clear();
    
    lcd.setCursor(0, 0);
    lcd.print(" Smart Fan Node ");
    lcd.setCursor(0, 1);
    lcd.print(" Initializing...");
};

void task_lcd(void *pvParameters)
{
    sta_connected_millis = 0;
    lcd_setup();
    vTaskDelay(1500 / portTICK_PERIOD_MS);
    lcd.clear();

    char line1[17];
    char line2[17];

    while (true)
    {
        if (isAPMode)
        {
            snprintf(line1, sizeof(line1), "MODE: AP CONFIG ");
            snprintf(line2, sizeof(line2), "IP:%-13s", WiFi.softAPIP().toString().c_str());
        }
        else if (millis() - sta_connected_millis < 10000)
        {
            snprintf(line1, sizeof(line1), "WiFi Connected! ");
            snprintf(line2, sizeof(line2), "IP:%-13s", WiFi.localIP().toString().c_str());
        }
        else
        {
            snprintf(line1, sizeof(line1), "T:%4.1fC H:%3.0f%% ", glob_temperature, glob_humidity);

            char mode_char = system_state ? 'A' : 'M'; 

            if (swing_mode_enable)
            {
                snprintf(line2, sizeof(line2), "[%c] F:%3d%% SW:ON", mode_char, fan_speed);
            }
            else
            {
                snprintf(line2, sizeof(line2), "[%c] F:%3d%% A:%+3d\xDF", mode_char, fan_speed, current_target_angle);
            }
        }

        lcd.setCursor(0, 0);
        lcd.print(line1);

        lcd.setCursor(0, 1);
        lcd.print(line2);

        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void component_reset()
{
    analogWrite(FAN_PIN, 0);
}