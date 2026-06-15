/* 
Rainbow Sparkle Unicorn - SN10
*/
#include <Arduino.h>
#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <Preferences.h>
#include "defines.h"

#include "messaging.h"
#include "microbit-uart.h"
#include "sound.h"
#include "encoders.h"
#include "touch.h"
#include "DAC.h"
#include "ADC.h"
#include "light.h"
#include "switch.h"
#include "movement.h"
#include "IoT.h"
#include "WifiMgr.h"

void checkI2Cerrors(std::string area);
void runTests();

Preferences preferences;

QueueHandle_t Sound_Queue;
QueueHandle_t DAC_Queue;
QueueHandle_t Light_Queue;
QueueHandle_t Movement_Queue;
QueueHandle_t MQTT_Command_Queue;

SemaphoreHandle_t i2cSemaphore;

bool POSTerror = false;

extern std::string requestMessage;

extern void dealWithMessage(std::string message);

void setup()
{
  preferences.begin(BOARDNAME, false);

  //Set UART log level
  esp_log_level_set(BOARDNAME, ESP_LOG_VERBOSE);

  pinMode(ONBOARDLED, OUTPUT);

  //start i2c
  Wire.begin(SDA, SCL);

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("");
  Serial.println("");

  //create i2c Semaphore , and set to useable
  i2cSemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(i2cSemaphore);

  //set up the main queues
  Sound_Queue = xQueueCreate(5, sizeof(messageParts));
  DAC_Queue = xQueueCreate(5, sizeof(messageParts));
  Light_Queue = xQueueCreate(30, sizeof(messageParts));
  Movement_Queue = xQueueCreate(30, sizeof(messageParts));
  MQTT_Command_Queue = xQueueCreate(30, sizeof(messageParts));

  //call the microbit first and then the other setup methods
  microbit_setup();
  Wifi_setup();
  sound_setup();
  touch_setup();
  encoders_setup();
  DAC_setup();
  ADC_setup();
  light_setup();
  switch_setup();
  movement_setup();
  MQTT_setup();

  Serial << BOARDNAME << " completed in " << millis() << "ms" << endl;

  runTests();
}

void POST(uint8_t flashes)
{
  vTaskSuspendAll(); 
  POSTerror = true;
  uint32_t speed_us = 150 * 1000; // Convert ms to microseconds

  for (;;)
  {
    for (size_t i = 0; i < flashes; i++)
    {
      digitalWrite(ONBOARDLED, HIGH);
      esp_rom_delay_us(speed_us);
      digitalWrite(ONBOARDLED, LOW);
      esp_rom_delay_us(speed_us);
    }
    esp_rom_delay_us(1000 * 1000);
  }
}

void checkI2Cerrors(std::string area)
{
  if (Wire.getWriteError() != 0)
  {
    //Serial << "i2C error @ " << area.c_str() << ":" << Wire.getErrorText(Wire.lastError()) << endl;

    Wire.clearWriteError();
  }
}

void loop()
{
  if (POSTerror == false)
  {
    digitalWrite(ONBOARDLED, WiFi.isConnected());
  }

  delay(1000);
}

void runTests()
{

  //  dealWithMessage("LLEDINTENSITY,0,0 ");
  //  dealWithMessage("LLEDINTENSITY,1,0 ");
  //  dealWithMessage("LLEDINTENSITY,2,0 ");

  //dealWithMessage("SUBSCRIBE,ps2/buttons ");

  //dealWithMessage("STARTING ");
  // dealWithMessage("MLINEAR,8,0,180,10,100,500 ");

  // dealWithMessage("LLEDALLON ");
  // dealWithMessage("LBLINK,0,1000,1000 ");
  // dealWithMessage("LBREATHE,7,1000,1000,500,500 ");

  // dealWithMessage("MPWM,8,500 ");
}
