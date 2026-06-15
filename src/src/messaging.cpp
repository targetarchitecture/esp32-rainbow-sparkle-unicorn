#include <Arduino.h>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include "messaging.h"

static uint32_t safe_string_to_uint32(const std::string& str) {
    if (str.empty()) return 0;
    try {
        return static_cast<uint32_t>(std::stoul(str));
    } 
    catch (...) {
        return 0; // Fallback value safe default execution parameter
    }
}

void dealWithMessage(std::string message)
{
    // String modifications step length stripping shifted directly into stream accumulator
    messageParts queuedMsg = processQueueMessage(message);
    std::string identifier = queuedMsg.identifier;

    if (identifier.compare("RESTART") == 0)
    {
        ESP.restart();
    }
    else if (identifier.compare("SVOL") == 0 || identifier.compare("SFILECOUNT") == 0 ||
             identifier.compare("SPLAY") == 0 || identifier.compare("SPAUSE") == 0 ||
             identifier.compare("SRESUME") == 0 || identifier.compare("SSTOP") == 0)
    {
        xQueueSend(Sound_Queue, &queuedMsg, portMAX_DELAY);
    }
    else if (identifier.compare("SBUSY") == 0)
    {
        std::string requestMessage = "SBUSY:" + std::to_string(digitalRead(DFPLAYER_BUSY));
        sendToMicrobit(requestMessage);
    }
    else if (identifier.compare("LBLINK") == 0 || identifier.compare("LBREATHE") == 0 ||
             identifier.compare("LLEDONOFF") == 0 || identifier.compare("LLEDALLOFF") == 0 ||
             identifier.compare("LLEDALLON") == 0 || identifier.compare("LLEDINTENSITY") == 0)
    {
        xQueueSend(Light_Queue, &queuedMsg, portMAX_DELAY);
    }
    else if (identifier.compare("PUBLISH") == 0 || identifier.compare("SUBSCRIBE") == 0 ||
             identifier.compare("UNSUBSCRIBE") == 0)
    {
        xQueueSend(MQTT_Command_Queue, &queuedMsg, portMAX_DELAY);
    }
    else if (identifier.compare("DIAL1") == 0 || identifier.compare("DIAL2") == 0)
    {
        xQueueSend(DAC_Queue, &queuedMsg, portMAX_DELAY);
    }
    else if (identifier.compare("MSTOP") == 0 || identifier.compare("MANGLE") == 0 ||
             identifier.compare("MLINEAR") == 0 || identifier.compare("MSMOOTH") == 0 ||
             identifier.compare("MBOUNCY") == 0 || identifier.compare("MPWM") == 0)
    {
        xQueueSend(Movement_Queue, &queuedMsg, portMAX_DELAY);
    }
    else if (identifier.compare("ROTARY1") == 0)
    {
        std::string requestMessage = "ROTARY1:" + std::to_string(encoder1.getCount()); // Live counter read
        sendToMicrobit(requestMessage);
    }
    else if (identifier.compare("ROTARY2") == 0)
    {
        std::string requestMessage = "ROTARY2:" + std::to_string(encoder2.getCount());
        sendToMicrobit(requestMessage);
    }
    else if (identifier.compare("SLIDER1") == 0)
    {
        std::string requestMessage = "SLIDER1:" + std::to_string(analogRead(ADC1));
        sendToMicrobit(requestMessage);
    }
    else if (identifier.compare("SLIDER2") == 0)
    {
        std::string requestMessage = "SLIDER2:" + std::to_string(analogRead(ADC2));
        sendToMicrobit(requestMessage);
    }
    else if (identifier.compare("SSTATE") == 0)
    {
        std::string swithStates = "SSTATE:";
        for (size_t i = 0; i < 16; i++)
        {
            swithStates.append(switchArray[i] == LOW ? "L" : "H");
        }
        sendToMicrobit(swithStates);
    }
    else if (identifier.compare("TTHRSLD") == 0)
    {
        touch_deal_with_message(queuedMsg);
    }
    else if (identifier.compare("DEBUG") == 0)
    {
        Serial << queuedMsg.part1 << " " << queuedMsg.value1 << endl;
    }
    else if (identifier.compare("NVMSSID") == 0) preferences.putString("ssid", queuedMsg.part1);
    else if (identifier.compare("NVMPASSWORD") == 0) preferences.putString("password", queuedMsg.part1);
    else if (identifier.compare("NVMMQTTSERVER") == 0) preferences.putString("mqtt_server", queuedMsg.part1);
    else if (identifier.compare("NVMMQTTUSER") == 0) preferences.putString("mqtt_user", queuedMsg.part1);
    else if (identifier.compare("NVMMQTTPASSWORD") == 0) preferences.putString("mqtt_password", queuedMsg.part1);
}

messageParts processQueueMessage(std::string msg)
{
    std::istringstream f(msg);
    std::string part;
    messageParts mParts = {}; 
    int index = 0;

    while (std::getline(f, part, ','))
    {
        switch (index)
        {
            case 0:
                strncpy(mParts.identifier, part.c_str(), sizeof(mParts.identifier) - 1);
                mParts.identifier[sizeof(mParts.identifier) - 1] = '\0';
                break;
            case 1:
                mParts.value1 = safe_string_to_uint32(part);
                strncpy(mParts.part1, part.c_str(), sizeof(mParts.part1) - 1);
                mParts.part1[sizeof(mParts.part1) - 1] = '\0';
                break;
            case 2:
                mParts.value2 = safe_string_to_uint32(part);
                strncpy(mParts.part2, part.c_str(), sizeof(mParts.part2) - 1);
                mParts.part2[sizeof(mParts.part2) - 1] = '\0';
                break;
            case 3: mParts.value3 = safe_string_to_uint32(part); break;
            case 4: mParts.value4 = safe_string_to_uint32(part); break;
            case 5: mParts.value5 = safe_string_to_uint32(part); break;
            case 6: mParts.value6 = safe_string_to_uint32(part); break;
            case 7: mParts.value7 = safe_string_to_uint32(part); break;
        }
        index++;
    }
    return mParts;
}
