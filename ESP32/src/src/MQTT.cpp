#include <Arduino.h>
#include "MQTT.h"

WiFiClient client;
PubSubClient MQTTClient;

std::string MQTT_SERVER = "";
std::string MQTT_CLIENTID = "";
std::string MQTT_USERNAME = "";
std::string MQTT_KEY = "";
std::string WIFI_SSID = "";
std::string WIFI_PASSPHRASE = "";

std::string IP_ADDRESS = "";

TaskHandle_t MQTTTask;
TaskHandle_t MQTTClientTask;

volatile bool ConnectWifi = false;
volatile bool ConnectMQTT = false;
volatile bool ConnectSubscriptions = false;

unsigned long lastMQTTStatusSent = 0;

std::list<MQTTSubscription> MQTTSubscriptions;

void MQTT_setup()
{
  pinMode(ONBOARDLED, OUTPUT);

  xTaskCreatePinnedToCore(
      MQTT_task,          /* Task function. */
      "MQTT Task",        /* name of task. */
      17000,              /* Stack size of task (uxTaskGetStackHighWaterMark:16084) */
      NULL,               /* parameter of the task */
      MQTT_task_Priority, /* priority of the task */
      &MQTTTask,          /* Task handle to keep track of created task */
      1);

  xTaskCreatePinnedToCore(
      MQTTClient_task,           /* Task function. */
      "MQTTClient Task",         /* name of task. */
      17000,                     /* Stack size of task (uxTaskGetStackHighWaterMark:16084) */
      NULL,                      /* parameter of the task */
      MQTT_client_task_Priority, /* priority of the task */
      &MQTTClientTask,           /* Task handle to keep track of created task */
      1);
}

void Wifi_connect()
{
  Serial.println("Connecting to Wifi");
  Serial.println(WIFI_SSID.c_str());
  Serial.println(WIFI_PASSPHRASE.c_str());

  WiFi.mode(WIFI_OFF);
  delay(500);
  WiFi.mode(WIFI_STA);
  delay(500);

  //connect
  uint32_t speed = 500;
  while (WiFi.status() != WL_CONNECTED)
  {
    WiFi.begin(WIFI_SSID.c_str(), WIFI_PASSPHRASE.c_str());

    digitalWrite(ONBOARDLED, HIGH);
    delay(speed);
    digitalWrite(ONBOARDLED, LOW);
    delay(speed);

    Serial.println(WiFi.status());
  }

  char msgtosend[MAXBBCMESSAGELENGTH];
  sprintf(msgtosend, "G1,%s", WiFi.localIP().toString().c_str());

  sendToMicrobit(msgtosend);
}

// void MQTT_connect2()
// {
//   MQTTClient.setClient(client);
//   MQTTClient.setServer(MQTT_SERVER.c_str(), 1883);
//   MQTTClient.setCallback(recieveMessage);

//   checkMQTTconnection(true);

// //connect
// uint32_t speed = 250;
// for (size_t i = 0; i < 4; i++)
// {
//   digitalWrite(ONBOARDLED, HIGH);
//   delay(speed);
//   digitalWrite(ONBOARDLED, LOW);
//   delay(speed);
// }
//}

void recieveMessage(char *topic, byte *payload, unsigned int length)
{
  // Serial.print("Message arrived [");
  // Serial.print(topic);
  // Serial.print("] ");

  std::string receivedMsg;

  for (int i = 0; i < length; i++)
  {
    char c = payload[i];

    //Serial.print(c);

    receivedMsg += c;
  }
  //Serial.println();

  char msgtosend[MAXBBCMESSAGELENGTH];
  sprintf(msgtosend, "G3,'%s','%s'", topic, receivedMsg.c_str());
  sendToMicrobit(msgtosend);
}

void checkMQTTconnection()
{
  if (MQTTClient.connected() == false)
  {
    Serial.println("MQTTClient NOT Connected :(");

    delay(500);

    MQTTClient.connect(MQTT_CLIENTID.c_str(), MQTT_USERNAME.c_str(), MQTT_KEY.c_str());

    if (MQTTClient.connected() == true)
    {
      //set to true to get the subscriptions setup again
      ConnectSubscriptions = true;

      //send status regardless of timings
      sendMQTTConnectionStatus();
    }
  }

  //send a connection status messaGE EVERY 10 SECONDS
  unsigned long currentMillis = millis();

  if (currentMillis - lastMQTTStatusSent >= 10 * 1000)
  {
    lastMQTTStatusSent = currentMillis;

    sendMQTTConnectionStatus();
  }
}

void sendMQTTConnectionStatus()
{
  if (MQTTClient.connected() == true)
  {
    //Serial.println("G2,1");

    char msgtosend[MAXBBCMESSAGELENGTH];
    sprintf(msgtosend, "G2,1");
    sendToMicrobit(msgtosend);
  }
  else
  {
    //Serial.println("G2,0");

    char msgtosend[MAXBBCMESSAGELENGTH];
    sprintf(msgtosend, "G2,0");
    sendToMicrobit(msgtosend);
  }
}

void setupSubscriptions()
{
  std::list<MQTTSubscription>::iterator it;

  for (it = MQTTSubscriptions.begin(); it != MQTTSubscriptions.end(); it++)
  {
    // Access the object through iterator
    //auto topic = it->topic.c_str();
    auto subscribe = it->subscribe;

    if (subscribe == true)
    {
      Serial.print("subscribing to:");
      Serial.println(it->topic.c_str());

      MQTTClient.subscribe(it->topic.c_str());
    }
    else
    {
      Serial.print("unsubscribing to:");
      Serial.println(it->topic.c_str());

      MQTTClient.unsubscribe(it->topic.c_str());
    }
  }
}

void MQTTClient_task(void *pvParameter)
{
  MQTTMessage msg;

  for (;;)
  {
    if (ConnectWifi == true)
    {
      //Serial.println("ConnectWifi == true");

      if (WiFi.isConnected() == false)
      {
        //Serial.println("WiFi.isConnected() == false");

        Wifi_connect();
      }

      if (ConnectMQTT == true)
      {
        //Serial.println("ConnectMQTT == true");

        //only bother if actually connected to internet!!
        if (WiFi.isConnected() == true)
        {
          //Serial.println("WiFi.isConnected() == true");

          checkMQTTconnection();

          if (MQTTClient.connected())
          {
            //Serial.println("MQTTClient.connected()");

            //check the message queue and if empty just proceed passed
            if (xQueueReceive(MQTT_Message_Queue, &msg, 0) == pdTRUE)
            {
              // Serial.print("publish topic:");
              // Serial.print(msg.topic.c_str());
              // Serial.print("\t\t");
              // Serial.print("payload:");
              // Serial.print(msg.payload.c_str());
              // Serial.println("");

              foo("publish topic:",msg.topic.c_str());

              MQTTClient.publish(msg.topic.c_str(), msg.payload.c_str());
            }

            //check to see if we need to remake the subscriptions
            if (ConnectSubscriptions == true)
            {
              //Serial.println("ConnectSubscriptions == true");

              //set up subscription topics
              setupSubscriptions();

              ConnectSubscriptions = false;
            }

            //must call the loop method!
            MQTTClient.loop();

            //https://github.com/256dpi/arduino-mqtt/blob/master/examples/ESP32DevelopmentBoard/ESP32DevelopmentBoard.ino
            delay(10); // <- fixes some issues with WiFi stability ???
          }
        }
        else
        {
          Serial.println("WiFi not connected,cannot send MQTT message");
        }
      }
    }

    delay(100);
  }

  vTaskDelete(NULL);
}

void MQTT_task(void *pvParameter)
{
  messageParts parts;

  // UBaseType_t uxHighWaterMark;
  // uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
  // Serial.print("MQTT_task uxTaskGetStackHighWaterMark:");
  // Serial.println(uxHighWaterMark);

  for (;;)
  {
    char msg[MAXESP32MESSAGELENGTH] = {0};

    //show WiFi connection
    if (WiFi.isConnected() == true)
    {
      digitalWrite(ONBOARDLED, HIGH);
    }
    else
    {
      digitalWrite(ONBOARDLED, LOW);
    }

    //wait for new MQTT command in the queue
    xQueueReceive(MQTT_Queue, &msg, portMAX_DELAY);

    // Serial.print("MQTT_Queue:");
    // Serial.println(msg);

    //TODO: see if need this copy of msg.
    std::string X = msg;

    parts = processQueueMessage(X.c_str(), "MQTT");

    // if (strncmp(parts.identifier, "T1",2) == 0)
    // {
    //   std::string str(parts.value1);
    //   WIFI_SSID = str;
    // }
    // else if (strncmp(parts.identifier, "T2",2) == 0)
    // {
    //   std::string str(parts.value1);
    //   WIFI_PASSPHRASE = str;
    // }
    // else

    if (strncmp(parts.identifier, "T3", 2) == 0)
    {
      std::string strA(parts.value1);
      WIFI_SSID = strA;

      std::string strB(parts.value2);
      WIFI_PASSPHRASE = strB;

      ConnectWifi = true;
    }
    else if (strncmp(parts.identifier, "T4", 2) == 0)
    {
      //send status to try to get the BBC loop to stop if it senses it's connected
      sendMQTTConnectionStatus();

      std::string str(parts.value1);
      MQTT_SERVER = str;

      //set this up as early as possible
      MQTTClient.setClient(client);
      MQTTClient.setServer(MQTT_SERVER.c_str(), 1883);
      MQTTClient.setCallback(recieveMessage);
    }
    else if (strncmp(parts.identifier, "T5", 2) == 0)
    {
      //send status to try to get the BBC loop to stop if it senses it's connected
      sendMQTTConnectionStatus();

      std::string str(parts.value1);
      MQTT_CLIENTID = str;
    }
    else if (strncmp(parts.identifier, "T6", 2) == 0)
    {
      //send status to try to get the BBC loop to stop if it senses it's connected
      sendMQTTConnectionStatus();

      std::string str(parts.value1);
      MQTT_USERNAME = str;
    }
    else if (strncmp(parts.identifier, "T7", 2) == 0)
    {
      //send status to try to get the BBC loop to stop if it senses it's connected
      sendMQTTConnectionStatus();

      std::string str(parts.value1);
      MQTT_KEY = str;
    }
    else if (strncmp(parts.identifier, "T8", 2) == 0)
    {
      //send status to try to get the BBC loop to stop if it senses it's connected
      sendMQTTConnectionStatus();

      Serial.print("MQTT_SERVER:");
      Serial.println(MQTT_SERVER.length());
      Serial.print("MQTT_KEY:");
      Serial.println(MQTT_KEY.length());
      Serial.print("MQTT_USERNAME:");
      Serial.println(MQTT_USERNAME.length());
      Serial.print("MQTT_CLIENTID:");
      Serial.println(MQTT_CLIENTID.length());

      //only give the order to connect if we really do have all of the pieces
      if (MQTT_SERVER.length() > 1 && MQTT_KEY.length() > 1 && MQTT_USERNAME.length() > 1 && MQTT_CLIENTID.length() > 1)
      {
        Serial.println("All good to connect to the MQTT server");

        ConnectMQTT = true;
      }
    }
    else if (strncmp(parts.identifier, "T9", 2) == 0)
    {
      std::string topic(parts.value1);
      std::string payload(parts.value2);

      MQTTMessage msg = {topic, payload};

      xQueueSend(MQTT_Message_Queue, &msg, portMAX_DELAY);
    }
    else if (strncmp(parts.identifier, "T10", 3) == 0)
    {
      //add to the list (if it doesn't exist)
      std::string topic(parts.value1);
      bool found = false;

      std::list<MQTTSubscription>::iterator it;

      for (it = MQTTSubscriptions.begin(); it != MQTTSubscriptions.end(); it++)
      {
        // Access the object through iterator
        auto MQTTSubscriptionTopic = it->topic;

        if (topic == MQTTSubscriptionTopic)
        {
          Serial.print("Update to subscription list: ");
          Serial.println(parts.value1);

          it->subscribe = true;
          found = true;
        }
      }

      if (found == false)
      {
        MQTTSubscriptions.push_back({topic, true});

        //Serial.print("Add to subscription list: ");
        //Serial.println(parts.value1);
      }

      //set to true to get the subscriptions setup again
      ConnectSubscriptions = true;
    }
    else if (strncmp(parts.identifier, "T11", 3) == 0)
    {
      std::string topic(parts.value1);

      std::list<MQTTSubscription>::iterator it;

      for (it = MQTTSubscriptions.begin(); it != MQTTSubscriptions.end(); it++)
      {
        // Access the object through iterator
        auto MQTTSubscriptionTopic = it->topic;

        if (topic == MQTTSubscriptionTopic)
        {
          // Serial.print("Update to unsubsripton list: ");
          // Serial.println(parts.value1);

          it->subscribe = false;
        }
      }

      //set to true to get the subscriptions setup again
      ConnectSubscriptions = true;
    }
    else if (strncmp(parts.identifier, "T12", 3) == 0)
    {
      //reset variables
      ConnectMQTT = false;
      ConnectWifi = false;

      //turn off WiFi
      WiFi.mode(WIFI_OFF);
      delay(500);

      //turn off LED
      digitalWrite(ONBOARDLED, LOW);
    }
  }

  vTaskDelete(NULL);
}

// template <class... Args>;
// void foo(const std::string &format, Args... args)
// {
//   Serial.printf(format.c_str(), args...);

//   if (MQTTClient.connected() == true)
//   {
//     char payload[MAXBBCMESSAGELENGTH];
//     sprintf(payload, format.c_str(), args...);

//     auto chipID = ESP.getEfuseMac();
//     std::string topic;

//     topic = "debug/";
//     topic += chipID;

//     MQTTMessage msg = {topic, payload};

//     xQueueSend(MQTT_Message_Queue, &msg, portMAX_DELAY);
//   }
// }