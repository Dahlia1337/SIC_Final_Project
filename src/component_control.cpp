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
    lcd_setup();
    vTaskDelay(1500 / portTICK_PERIOD_MS);
    lcd.clear();

    sta_connected_millis = millis();

    char line1[17];
    char line2[17];

    int last_mode = -1;

    while (true)
    {
        int current_mode = 0;
        if (isAPMode)
        {
            current_mode = 1;
            snprintf(line1, sizeof(line1), "MODE: AP CONFIG ");
            snprintf(line2, sizeof(line2), "IP:%-13s", WiFi.softAPIP().toString().c_str());
        }
        else if (millis() - sta_connected_millis < 20000)
        {
            current_mode = 2;
            snprintf(line1, sizeof(line1), "WiFi Connected! ");
            snprintf(line2, sizeof(line2), "IP:%-13s", WiFi.localIP().toString().c_str());
        }
        else
        {
            current_mode = 3;
            const char* comfort_labels[] = {"COLD", "COMF", "WARM", "HOT "};

            int comfort_idx = (comfort_class >= 0 && comfort_class < 4) ? comfort_class : 1;
            char mode_char = system_state ? 'A' : 'M';

            // Dòng 1: Nhiệt độ + Độ ẩm
            snprintf(line1, sizeof(line1), "T:%2.0f\337C H:%2.0f%% [%c]", glob_temperature, glob_humidity, mode_char);

            // Dòng 2: Chế độ, tốc độ quạt và Nhãn AI
            snprintf(line2, sizeof(line2), "F:%-3d%%   AI:%-4s", fan_speed, comfort_labels[comfort_idx]);
        }

        if (current_mode != last_mode)
        {
            lcd.clear();
            last_mode = current_mode;
        }

        lcd.setCursor(0, 0);
        lcd.print(line1);

        lcd.setCursor(0, 1);
        lcd.print(line2);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void component_reset()
{
    analogWrite(FAN_PIN, 0);
}