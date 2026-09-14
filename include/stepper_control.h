#ifndef _STEPPER_CONTROL_H
#define _STEPPER_CONTROL_H

#include "global.h"
#include <AccelStepper.h>
#include <Preferences.h>



extern bool swing_mode_enable;
extern int current_target_angle;

void stepper_init();
void stepper_set_angle(int angle);
void stepper_toggle_swing(bool enable);
void stepper_calibrate_zero();
bool stepper_is_active();
void task_stepper(void *pvParameters);

#endif // _STEPPER_CONTROL_H