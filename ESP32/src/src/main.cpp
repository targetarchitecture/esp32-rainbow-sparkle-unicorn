/* 
Rainbow Sparkle Unicorn - SN4
*/
#include <Arduino.h>
#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "globals.h"
#include "microbit-i2c.h"
#include "music-dfplayer-dfrobot.h"
#include "encoders.h"
#include "routing.h"
#include "touch.h"
#include "DAC.h"
#include "ADC.h"
#include "light.h"
#include "switch.h"
#include "movement.h"
#include "MQTT.h"

QueueHandle_t Microbit_Transmit_Queue; //Queue to send messages to the Microbit
QueueHandle_t Microbit_Receive_Queue;  //Queue to recieve the messages from the Microbit
QueueHandle_t Music_Queue;             //Queue to store all of the DFPlayer commands from the Microbit
QueueHandle_t DAC_Queue;
QueueHandle_t Light_Queue;
QueueHandle_t ADC_Queue;
//QueueHandle_t Message_Queue; 
QueueHandle_t Movement_Queue;
QueueHandle_t MQTT_Queue;

extern PubSubClient MQTTClient;
void checkI2Cerrors(const char *area);

void setup()
{
  //Set UART log level
  //esp_log_level_set("SN4", ESP_LOG_VERBOSE);

  //stop bluetooth
  btStop();

  //start i2c
  Wire.begin(SDA, SCL);

  //checkI2Cerrors("main");

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("");

  //create i2c Semaphore , and set to useable
  i2cSemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(i2cSemaphore);

  //set up the main queues
  char TXtoBBCmessage[MAXBBCMESSAGELENGTH];
  char RXfromBBCmessage[MAXMESSAGELENGTH];
  char MAXUSBMessage[UARTMESSAGELENGTH];

  Microbit_Receive_Queue = xQueueCreate(UARTMESSAGELENGTH * 8, sizeof(uint8_t)); //1024 = 128x8
  Microbit_Transmit_Queue = xQueueCreate(50, sizeof(TXtoBBCmessage));  
  //Message_Queue = xQueueCreate(50, sizeof(MAXUSBMessage));

  Music_Queue = xQueueCreate(50, sizeof(RXfromBBCmessage));
  DAC_Queue = xQueueCreate(50, sizeof(RXfromBBCmessage));
  Light_Queue = xQueueCreate(50, sizeof(RXfromBBCmessage));
  Movement_Queue = xQueueCreate(50, sizeof(RXfromBBCmessage));
  MQTT_Queue = xQueueCreate(50, sizeof(RXfromBBCmessage));

  //get wifi going first as this seems to be problematic
  MQTT_setup();

  //call the feature setup methods
  music_setup();

  touch_setup();

  encoders_setup();

  DAC_setup();

  ADC_setup();

  light_setup();

  switch_setup();

  movement_setup();

  routing_setup();

  microbit_i2c_setup();

  // Serial.print("completed by ");
  // Serial.println( millis());
}

void loop()
{
  delay(1000);
}

messageParts processQueueMessage(const std::string msg, const std::string from)
{
  //Serial.printf("processQueueMessage (%s): %s\n", from.c_str(), msg.c_str());

  messageParts mParts = {};
  strcpy(mParts.fullMessage, msg.c_str());

  try
  {
    //https://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c
    std::string delim = ",";
    int index = 0;
    auto start = 0U;
    auto end = msg.find(delim);

    while (end != std::string::npos)
    {
      if (index == 0)
      {
        strcpy(mParts.identifier, msg.substr(start, end - start).c_str());
      }
      if (index == 1)
      {
        strcpy(mParts.value1, msg.substr(start, end - start).c_str());
      }
      if (index == 2)
      {
        strcpy(mParts.value2, msg.substr(start, end - start).c_str());
      }
      if (index == 3)
      {
        strcpy(mParts.value3, msg.substr(start, end - start).c_str());
      }
      if (index == 4)
      {
        strcpy(mParts.value4, msg.substr(start, end - start).c_str());
      }
      if (index == 5)
      {
        strcpy(mParts.value5, msg.substr(start, end - start).c_str());
      }
      if (index == 6)
      {
        strcpy(mParts.value6, msg.substr(start, end - start).c_str());
      }
      if (index == 7)
      {
        strcpy(mParts.value7, msg.substr(start, end - start).c_str());
      }

      start = end + delim.length();
      end = msg.find(delim, start);

      index++;
    }

    //it's a bit crap to repeat the logic - but it works
    if (index == 0)
    {
      strcpy(mParts.identifier, msg.substr(start, end - start).c_str());
    }
    if (index == 1)
    {
      strcpy(mParts.value1, msg.substr(start, end - start).c_str());
    }
    if (index == 2)
    {
      strcpy(mParts.value2, msg.substr(start, end - start).c_str());
    }
    if (index == 3)
    {
      strcpy(mParts.value3, msg.substr(start, end - start).c_str());
    }
    if (index == 4)
    {
      strcpy(mParts.value4, msg.substr(start, end - start).c_str());
    }
    if (index == 5)
    {
      strcpy(mParts.value5, msg.substr(start, end - start).c_str());
    }
    if (index == 6)
    {
      strcpy(mParts.value6, msg.substr(start, end - start).c_str());
    }
    if (index == 7)
    {
      strcpy(mParts.value7, msg.substr(start, end - start).c_str());
    }

    // Serial.print("identifier:");
    // Serial.println(mParts.identifier);
    // Serial.print("value1:");
    // Serial.println(mParts.value1);
    // Serial.print("value2:");
    // Serial.println(mParts.value2);
    // Serial.print("value3:");
    // Serial.println(mParts.value3);
    // Serial.print("value4:");
    // Serial.println(mParts.value4);
    // Serial.print("value5:");
    // Serial.println(mParts.value5);
    // Serial.print("value6:");
    // Serial.println(mParts.value6);
    // Serial.print("value7:");
    // Serial.println(mParts.value7);
    // Serial.print("fullMessage:");
    // Serial.println(mParts.fullMessage);
  }
  catch (const std::exception &e)
  {
    Serial.printf("\n\n\nEXCEPTION: %s \n\n\n", e.what());
  }

  return mParts;
}

void POST(uint8_t flashes)
{
  //TODO: debate which tasks need stopping?
  vTaskDelete(ADCTask);

  pinMode(ONBOARDLED, OUTPUT);

  uint32_t speed = 150;

  for (;;)
  {
    for (size_t i = 0; i < flashes; i++)
    {
      digitalWrite(ONBOARDLED, HIGH);
      delay(speed);
      digitalWrite(ONBOARDLED, LOW);
      delay(speed);
    }
    delay(1000);
  }
}

// int printToSerial(const char *format, ...)
// {

//   int len = 0;

// #ifdef WRITETOSERIAL

//   char loc_buf[64];
//   char *temp = loc_buf;
//   va_list arg;
//   va_list copy;
//   va_start(arg, format);
//   va_copy(copy, arg);
//   len = vsnprintf(temp, sizeof(loc_buf), format, copy);
//   va_end(copy);
//   if (len < 0)
//   {
//     va_end(arg);
//     return 0;
//   };
//   if (len >= sizeof(loc_buf))
//   {
//     temp = (char *)malloc(len + 1);
//     if (temp == NULL)
//     {
//       va_end(arg);
//       return 0;
//     }
//     len = vsnprintf(temp, len + 1, format, arg);
//   }
//   va_end(arg);

//   len = Serial.println(temp);

//   if (temp != loc_buf)
//   {
//     free(temp);
//   }

// #endif

//   return len;
// }

void checkI2Cerrors(const char *area)
{
  if (Wire.lastError() != 0)
  {
    Serial.printf("i2C error @ %s: %s \n", area, Wire.getErrorText(Wire.lastError()));

    // if (MQTTClient.connected())
    // {
    //    MQTTClient.publish("i2c errors",  Wire.getErrorText(Wire.lastError()));
    // }

    //TODO: Check to see if this is still needed
    // Wire.clearWriteError();
  }
}
