#include "stepper_control.h"

#define IN1 15
#define IN2 16
#define IN3 17
#define IN4 18

// Nếu dùng HALF4WIRE thì 90 độ ~ 512 bước (với loại 2048 step/rev)
#define STEPS_PER_90_DEG   512

AccelStepper stepper(AccelStepper::FULL4WIRE, IN1, IN3, IN2, IN4);
Preferences stepper_prefs;

bool swing_mode_enable = false;
int current_target_angle = 0; // Mặc định hướng 0 độ (chính diện)
static int swing_direction = 1;
static unsigned long last_save_time = 0;
static bool need_save = false;

void stepper_init()
{
    stepper_prefs.begin("step_cfg", false);
    current_target_angle = stepper_prefs.getInt("last_angle", 0);
    stepper_prefs.end();

    stepper.setMaxSpeed(400.0);
    stepper.setAcceleration(200.0);

    // Ánh xạ dải [-90, 90] sang [-512, 512]
    long initial_steps = map(current_target_angle, -90, 90, -STEPS_PER_90_DEG, STEPS_PER_90_DEG);
    stepper.setCurrentPosition(initial_steps);
    stepper.moveTo(initial_steps);
    stepper.disableOutputs(); // Khởi tạo ở trạng thái đứng yên: ngắt dòng cuộn dây (0W)

    Serial.printf("🎯 Stepper Ready: Vị trí %d° (%ld steps)\n", current_target_angle, initial_steps);
}

void stepper_set_angle(int angle)
{
    // Giới hạn trong khoảng [-90, +90]
    if (angle < -90) angle = -90;
    if (angle > 90)  angle = 90;
    
    current_target_angle = angle;
    long target_steps = map(current_target_angle, -90, 90, -STEPS_PER_90_DEG, STEPS_PER_90_DEG);
    stepper.enableOutputs();
    stepper.moveTo(target_steps);

    need_save = true;
    last_save_time = millis();
}

void stepper_toggle_swing(bool enable)
{
    swing_mode_enable = enable;
    stepper.enableOutputs();
    if (enable) {
        stepper.moveTo(STEPS_PER_90_DEG); // Quét sang +90 độ trước
        swing_direction = 1;
    } else {
        stepper_set_angle(current_target_angle); // Quay về góc đặt cố định
    }
}

void stepper_calibrate_zero()
{
    stepper.setCurrentPosition(0);
    current_target_angle = 0;
    stepper.disableOutputs();
    
    stepper_prefs.begin("step_cfg", false);
    stepper_prefs.putInt("last_angle", 0);
    stepper_prefs.end();
    
    Serial.println("⚙️ Đã Calib vị trí hiện tại thành 0°!");
}

bool stepper_is_active()
{
    return swing_mode_enable || (stepper.distanceToGo() != 0);
}

void task_stepper(void *pvParameters)
{
    stepper_init();

    while (true)
    {
        if (swing_mode_enable)
        {
            if (stepper.distanceToGo() == 0)
            {
                if (swing_direction == 1) {
                    stepper.moveTo(-STEPS_PER_90_DEG); // Đổi hướng sang -90 độ
                    swing_direction = 0;
                } else {
                    stepper.moveTo(STEPS_PER_90_DEG);  // Đổi hướng sang +90 độ
                    swing_direction = 1;
                }
            }
            stepper.run();
        }
        else
        {
            if (stepper.distanceToGo() != 0)
            {
                stepper.run();
            }
            else
            {
                // Khi đứng yên và không đảo gió: ngắt điện cuộn dây để triệt tiêu holding current (0W)
                stepper.disableOutputs();

                if (need_save && (millis() - last_save_time > 2000))
                {
                    stepper_prefs.begin("step_cfg", false);
                    stepper_prefs.putInt("last_angle", current_target_angle);
                    stepper_prefs.end();
                    need_save = false;
                    Serial.printf("💾 Đã lưu góc %d° vào NVS\n", current_target_angle);
                }
            }
        }

        vTaskDelay(2 / portTICK_PERIOD_MS);
    }
}