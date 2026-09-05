#include "neo_blinky.h"
#include "global.h"

Adafruit_NeoPixel rgb(1, NEO_PIN, NEO_GRB + NEO_KHZ800);

void rgb_setup()
{
    rgb.begin();
    rgb.setBrightness(10);
};

void rgb_control(int color)
{
    rgb.setPixelColor(0, rgb.gamma32(rgb.ColorHSV(color)));
    rgb.show();
};

void neo_animation(void *pvParameters)
{
    while (true)
    {
        // Vòng lặp tạo màu cầu vồng (0 -> 65535)
        for (long firstPixelHue = 0; firstPixelHue < 65536; firstPixelHue += 256)
        {
            // Chuyển đổi giá trị Hue sang màu RGB
            int pixelHue = firstPixelHue + (0 * 65536L / rgb.numPixels());
            rgb.setPixelColor(0, rgb.gamma32(rgb.ColorHSV(pixelHue)));
            rgb.show();

            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
};
