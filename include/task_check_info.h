#ifndef __TASK_CHECK_INFO_H__
#define __TASK_CHECK_INFO_H__

#include <ArduinoJson.h>
#include "LittleFS.h"
#include "global.h"
#include "task_wifi.h"

extern bool check_info_File(bool check);
extern void Load_info_File();
extern void Delete_info_File();
extern void Save_info_File(String WIFI_SSID, String WIFI_PASS);

#endif