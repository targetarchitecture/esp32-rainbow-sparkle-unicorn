#include <Arduino.h>
#include "touch.h"

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

Adafruit_MPR121 cap = Adafruit_MPR121();
volatile uint16_t lasttouched = 0;
TaskHandle_t TouchTask;

void touch_setup()
{
    uint8_t touchThreshold = preferences.getUShort("tch_threshold", 12U);
    uint8_t releaseThreshold = preferences.getUShort("tch_release", 6U);

    xSemaphoreTake(i2cSemaphore, portMAX_DELAY);
    if (!cap.begin(0x5A, &Wire, touchThreshold, releaseThreshold))
    {
        Serial.println("MPR121 not found");
        POST(3);
    }
    xSemaphoreGive(i2cSemaphore);

    lasttouched = readAndSetTouchArray();
    xTaskCreatePinnedToCore(touch_task, "Touch Task", 3000, NULL, touch_task_Priority, &TouchTask, 1);
}

void touch_task(void *pvParameter)
{
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        readAndSetTouchArray();
    }
}

uint16_t readAndSetTouchArray()
{
    xSemaphoreTake(i2cSemaphore, portMAX_DELAY);
    uint16_t currtouched = cap.touched();
    xSemaphoreGive(i2cSemaphore);

    for (uint8_t i = 0; i < 12; i++)
    {
        if ((currtouched & _BV(i)) && !(lasttouched & _BV(i)))
        {
            sendToMicrobit("TTOUCHED:" + std::to_string(i));
        }
        if (!(currtouched & _BV(i)) && (lasttouched & _BV(i)))
        {
            sendToMicrobit("TRELEASED:" + std::to_string(i));
        }
    }
    
    lasttouched = currtouched; // Synchronized baseline status state allocation tracking safely
    return currtouched;
}

void touch_deal_with_message(messageParts message)
{
    if (strcmp(message.identifier, "TTHRSLD") == 0)
    {
        preferences.putUShort("tch_threshold", message.value1);
        preferences.putUShort("tch_release", message.value2);

        xSemaphoreTake(i2cSemaphore, portMAX_DELAY);
        cap.setThresholds(message.value1, message.value2);
        xSemaphoreGive(i2cSemaphore);
    }
}
