#include <Arduino.h>
#include "WifiMgr.h"

String storedSSID;
String storedWifiPassword;

void WiFiEvent(WiFiEvent_t event)
{
    switch (event)
    {
        case ARDUINO_EVENT_WIFI_READY:
            Serial.println("WiFi interface ready.");
            break;
        case ARDUINO_EVENT_WIFI_STA_START:
            Serial.println("WiFi client station engine started.");
            break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("Associated with access point.");
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("Disconnected from WiFi. Initiating background retry...");
            WiFi.begin(storedSSID.c_str(), storedWifiPassword.c_str());
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.printf("Obtained IP configuration: %s at %dms\n", WiFi.localIP().toString().c_str(), millis());
            break;
        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
            Serial.println("IP lease expired or dropped.");
            break;
        default:
            break;
    }
}

void Wifi_setup()
{
    storedSSID = preferences.getString("ssid", "");
    storedWifiPassword = preferences.getString("password", "");

    if (storedSSID.length() == 0)
    {
        Serial.println("SSID not configured in NVM. Network interface offline.");
        return;
    }

    WiFi.onEvent(WiFiEvent);
    WiFi.begin(storedSSID.c_str(), storedWifiPassword.c_str());
}