#include "task_check_info.h"

void Load_info_File()
{
	File file = LittleFS.open("/info.dat", "r");
	if (!file)
	{
		return;
	}
	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, file);
	if (error)
	{
		Serial.print(F("deserializeJson() failed: "));
	}
	else
	{
		WIFI_SSID = strdup(doc["WIFI_SSID"]);
		WIFI_PASS = strdup(doc["WIFI_PASS"]);
	}
	file.close();
}

void Delete_info_File()
{
	if (LittleFS.exists("/info.dat"))
	{		
		LittleFS.remove("/info.dat");
		Serial.println("-------------------Deleted LittleFS File----------------");
		ESP.restart();
	}
	Serial.println("-------------------Fail Deleted----------------");
}

void Save_info_File(String wifi_ssid, String wifi_pass)
{
	Serial.println(wifi_ssid);
	Serial.println(wifi_pass);

	JsonDocument doc;
	doc["WIFI_SSID"] = wifi_ssid;
	doc["WIFI_PASS"] = wifi_pass;

	File configFile = LittleFS.open("/info.dat", "w");
	if (configFile)
	{
		serializeJson(doc, configFile);
		configFile.close();
	}
	else
	{
		Serial.println("Unable to save the configuration.");
	}
	ESP.restart();
};

bool check_info_File(bool check)
{
	if (!check)
	{
		if (!LittleFS.begin(true))
		{
			Serial.println("❌ Lỗi khởi động LittleFS!");
			return false;
		}
		Load_info_File();
	}

	if (WIFI_SSID.isEmpty() && WIFI_PASS.isEmpty())
	{
		if (!check)
		{
			startAP();
		}
		return false;
	}
	return true;
}