#ifndef __TASK_WIFI_H__
#define __TASK_WIFI_H__

#include "global.h"

#include <WiFi.h>
#include <task_webserver.h>

extern bool Wifi_reconnect();
extern void startAP();
extern void WiFi_Init();

#endif