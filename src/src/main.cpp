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

Preferences preferences;

QueueHandle_t Sound_Queue;
QueueHandle_t DAC_Queue;
QueueHandle_t Light_Queue;
QueueHandle_t Movement_Queue;
QueueHandle_t MQTT_Command_Queue;

SemaphoreHandle_t i2cSemaphore;
bool POSTerror = false;

void setup()
{
  preferences.begin(BOARDNAME, false);
  esp_log_level_set(BOARDNAME, ESP_LOG_VERBOSE);
  pinMode(ONBOARDLED, OUTPUT);

  Wire.begin(SDA, SCL);
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  i2cSemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(i2cSemaphore);

  Sound_Queue = xQueueCreate(10, sizeof(messageParts));
  DAC_Queue = xQueueCreate(10, sizeof(messageParts));
  Light_Queue = xQueueCreate(30, sizeof(messageParts));
  Movement_Queue = xQueueCreate(30, sizeof(messageParts));
  MQTT_Command_Queue = xQueueCreate(30, sizeof(messageParts));

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

  Serial << BOARDNAME << " initialization complete at " << millis() << "ms" << endl;
}

void POST(uint8_t flashes)
{
  vTaskSuspendAll(); // Freezes the scheduler kernel tick safely
  POSTerror = true;
  uint32_t speed_us = 150 * 1000; 

  for (;;)
  {
    for (size_t i = 0; i < flashes; i++)
    {
      digitalWrite(ONBOARDLED, HIGH);
      esp_rom_delay_us(speed_us); // Hardware delay avoids kernel panic when scheduler is suspended
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
    Wire.clearWriteError();
  }
}

void loop()
{
  if (!POSTerror)
  {
    digitalWrite(ONBOARDLED, WiFi.isConnected());
  }
  vTaskDelay(pdMS_TO_TICKS(1000));
}
