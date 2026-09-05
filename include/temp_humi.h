#ifndef __temp_humi__
#define __temp_humi__
#include <Arduino.h>
#include "DHT.h"
#include "global.h"
#include "freertos/semphr.h"
#include "task_webserver.h"
void task_sensor(void *pvParameters);
void sensor_setup();

#endif