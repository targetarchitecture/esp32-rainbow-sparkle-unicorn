#include <Arduino.h>
#include <cstring>
#include <sstream>
#include <algorithm> // Required for std::remove and std::find
#include "IoT.h"
#include "messaging.h"

WiFiClient client;
PubSubClient MQTTClient;

std::string mqtt_server;
std::string mqtt_user;
std::string mqtt_password;

QueueHandle_t MQTT_Publish_Queue;

struct MessageToPublish
{
    char topic[100];
    char payload[100];
};

TaskHandle_t MQTTCommandTask;
TaskHandle_t MQTTPublishTask;

volatile bool ConnectSubscriptions = true;

std::vector<std::string> SubscribedTopics;
std::vector<std::string> UnsubscribedTopics;

// Mutex to safeguard vector allocations across competing FreeRTOS threads
static SemaphoreHandle_t vectorMutex = NULL;

void MQTT_setup()
{
    MQTT_Publish_Queue = xQueueCreate(10, sizeof(MessageToPublish));
    vectorMutex = xSemaphoreCreateMutex();

    // Pull configurations securely out of Non-Volatile Storage (Preferences)
    mqtt_server = preferences.getString("mqtt_server", "").c_str();
    mqtt_user = preferences.getString("mqtt_user", "public").c_str();
    mqtt_password = preferences.getString("mqtt_password", "public").c_str();

    Serial << "MQTT Server from NVM: " << mqtt_server.c_str() << "\r\n";

    if (mqtt_server.empty())
    {
        Serial << "MQTT Server not set. Exiting MQTT configuration." << endl;
        return;
    }

    MQTTClient.setClient(client);
    MQTTClient.setServer(mqtt_server.c_str(), 1883);
    MQTTClient.setCallback(recieveMessage);

    xTaskCreatePinnedToCore(
        MQTT_command_task,   
        "MQTT Command Task", 
        3072,                // Incremented slightly to support safe string stream operations safely
        NULL,                
        MQTT_task_Priority,  
        &MQTTCommandTask,    
        1);

    xTaskCreatePinnedToCore(
        MQTT_Publish_task,         
        "MQTT Publish Task",       
        4096,                      // Optimized stack frame footprint
        NULL,                      
        MQTT_client_task_Priority, 
        &MQTTPublishTask,          
        1);
}

void recieveMessage(char *topic, byte *payload, unsigned int length)
{
    std::string receivedMsg;
    receivedMsg.reserve(length);

    for (unsigned int i = 0; i < length; i++)
    {
        receivedMsg += (char)payload[i];
    }

    // Preserve structural token matching expected by MakeCode split('',''): MQTT:'topic','payload'
    char msgtosend[MAXBBCMESSAGELENGTH];
    snprintf(msgtosend, sizeof(msgtosend), "MQTT:'%s','%s'", topic, receivedMsg.c_str());
    sendToMicrobit(msgtosend);
}

void checkMQTTconnection()
{
    if (MQTTClient.connected()) return;

    Serial.println("MQTTClient NOT Connected. Attempting broker connection...");

    String wifiMacString = WiFi.macAddress();
    std::string mqtt_client = std::string(BOARDNAME) + "_" + wifiMacString.c_str();

    Serial << mqtt_client.c_str() << ":" << mqtt_user.c_str() << ":" << mqtt_password.c_str() << endl;

    // Execute single connection attempt per loop step instead of locking up the thread context
    if (MQTTClient.connect(mqtt_client.c_str(), mqtt_user.c_str(), mqtt_password.c_str()))
    {
        Serial.println("MQTTClient successfully connected.");
        ConnectSubscriptions = true; // Signal topic subscription re-synchronization
    }
}

void setupSubscriptions()
{
    if (vectorMutex == NULL) return;

    if (xSemaphoreTake(vectorMutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        // Process active subscriptions
        for (size_t i = 0; i < SubscribedTopics.size(); i++)
        {
            Serial << "Subscribing to: " << SubscribedTopics[i].c_str() << endl;
            MQTTClient.subscribe(SubscribedTopics[i].c_str());
        }

        // Process outstanding unsubscriptions
        for (size_t i = 0; i < UnsubscribedTopics.size(); i++)
        {
            Serial << "Unsubscribing from: " << UnsubscribedTopics[i].c_str() << endl;
            MQTTClient.unsubscribe(UnsubscribedTopics[i].c_str());
        }
        UnsubscribedTopics.clear(); // Clear out actioned arrays markers

        xSemaphoreGive(vectorMutex);
    }
}

void MQTT_Publish_task(void *pvParameter)
{
    for (;;)
    {
        if (!WiFi.isConnected())
        {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        if (!MQTTClient.connected())
        {
            checkMQTTconnection();
            vTaskDelay(pdMS_TO_TICKS(1000)); // Paced re-evaluation spacing checkpoint
            continue;
        }

        // Re-establish network topic links if newly connected
        if (ConnectSubscriptions)
        {
            setupSubscriptions();
            ConnectSubscriptions = false;
        }

        MessageToPublish msg;
        // Non-blocking drain of outbound messaging elements
        if (xQueueReceive(MQTT_Publish_Queue, &msg, 0) == pdTRUE)
        {
            MQTTClient.publish(msg.topic, msg.payload);
        }

        MQTTClient.loop();
        vTaskDelay(pdMS_TO_TICKS(20)); // Streamlined 50Hz scheduling pacing step
    }
}

void MQTT_command_task(void *pvParameter)
{
    for (;;)
    {
        messageParts parts;
        // Block until the micro:bit issues an IoT command via UART
        xQueueReceive(MQTT_Command_Queue, &parts, portMAX_DELAY);

        std::string identifier = parts.identifier;

        if (identifier.compare("PUBLISH") == 0)
        {
            struct MessageToPublish msg = {};
            std::istringstream f(parts.part1);
            std::string part;
            int index = 0;

            // Split parameters divided by pipe characters ('|')
            while (std::getline(f, part, '|'))
            {
                if (index == 0)
                {
                    strncpy(msg.topic, part.c_str(), sizeof(msg.topic) - 1);
                    msg.topic[sizeof(msg.topic) - 1] = '\0';
                }
                else if (index == 1)
                {
                    strncpy(msg.payload, part.c_str(), sizeof(msg.payload) - 1);
                    msg.payload[sizeof(msg.payload) - 1] = '\0';
                }
                index++;
            }

            xQueueSend(MQTT_Publish_Queue, &msg, portMAX_DELAY);
        }
        else if (identifier.compare("SUBSCRIBE") == 0)
        {
            subscribe(parts.part1);
        }
        else if (identifier.compare("UNSUBSCRIBE") == 0)
        {
            unsubscribe(parts.part1);
        }
    }
}

void unsubscribe(std::string topic)
{
    if (vectorMutex == NULL) return;

    if (xSemaphoreTake(vectorMutex, portMAX_DELAY) == pdTRUE)
    {
        UnsubscribedTopics.push_back(topic);
        
        // Evict matching tracking tokens cleanly out of standard active allocations array
        SubscribedTopics.erase(
            std::remove(SubscribedTopics.begin(), SubscribedTopics.end(), topic), 
            SubscribedTopics.end()
        );
        
        xSemaphoreGive(vectorMutex);
        ConnectSubscriptions = true;
    }
}

void subscribe(std::string topic)
{
    if (vectorMutex == NULL) return;

    if (xSemaphoreTake(vectorMutex, portMAX_DELAY) == pdTRUE)
    {
        // Prevent storing duplicates if micro:bit runs subscription blocks in loops
        if (std::find(SubscribedTopics.begin(), SubscribedTopics.end(), topic) == SubscribedTopics.end())
        {
            SubscribedTopics.push_back(topic);
        }
        
        xSemaphoreGive(vectorMutex);
        ConnectSubscriptions = true;
    }
}
