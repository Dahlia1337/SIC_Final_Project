
#ifndef __TASK_HANDLER_H__
#define __TASK_HANDLER_H__

#include <ArduinoJson.h>

extern float auto_t1;
extern int   auto_s1;
extern float auto_t2;
extern int   auto_s2;

void load_auto_config();
void handleWebSocketMessage(String message);
void update_auto_fan_logic();

#endif