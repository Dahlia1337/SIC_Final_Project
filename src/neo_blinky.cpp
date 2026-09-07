#include "neo_blinky.h"
#include "global.h"

Adafruit_NeoPixel rgb(1, NEO_PIN, NEO_GRB + NEO_KHZ800);

void rgb_setup()
{
    rgb.begin();
    rgb.setBrightness(10);
    rgb.setBrightness(20);
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

void rgb_display_comfort(int comfort_state)
{
    switch (comfort_state)
    {
    case 0: // COLD -> Xanh dương
        rgb.setPixelColor(0, rgb.Color(0, 50, 255));
        break;
    case 1: // COMFORT -> Xanh lá cây dịu
        rgb.setPixelColor(0, rgb.Color(0, 255, 50));
        break;
    case 2: // WARM_HUMID -> Vàng / Cam
        rgb.setPixelColor(0, rgb.Color(255, 140, 0));
        break;
    case 3: // HOT -> Đỏ rực
        rgb.setPixelColor(0, rgb.Color(255, 0, 0));
        break;
    default:
        rgb.setPixelColor(0, rgb.Color(0, 0, 0)); // Tắt
        break;
    }
    rgb.show();
}