
#ifndef __TASK_WEBSERVER_H__
#define __TASK_WEBSERVER_H__

#include "global.h"

#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <ElegantOTA.h>
#include <task_handler.h>
#include <ESPmDNS.h>

extern AsyncWebServer server;
extern AsyncWebSocket ws;

void Webserver_stop();
void Webserver_reconnect();
void Webserver_sendata(String data);
void Webserver_send_sensor(float temp, float humi);

#endif