#include <Arduino.h>
#include "touch.h"

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

Adafruit_MPR121 cap = Adafruit_MPR121();

// Keeps track of the last pins touched, so we know when buttons are 'released'
volatile uint16_t lasttouched = 0;

TaskHandle_t TouchTask;

void touch_setup()
{
    // obtain previous threshold limits
    uint8_t touchThreshold = preferences.getUShort("tch_threshold", 12U);
    uint8_t releaseThreshold = preferences.getUShort("tch_release", 6U);

    // wait for the i2c semaphore flag to become available
    xSemaphoreTake(i2cSemaphore, portMAX_DELAY);

    // Default address is 0x5A
    if (!cap.begin(0x5A, &Wire, touchThreshold, releaseThreshold))
    {
        Serial.println("MPR121 not found, check wiring?");

        POST(3);
    }

    // give back the i2c flag for the next task
    xSemaphoreGive(i2cSemaphore);

    // read once and set array as the baseline
    lasttouched = readAndSetTouchArray();

    xTaskCreatePinnedToCore(
        &touch_task,
        "Touch Task",
        2000, // uxTaskGetStackHighWaterMark = 1750
        NULL,
        touch_task_Priority,
        &TouchTask,
        1);
}

void touch_task(void *pvParameter)
{
    // UBaseType_t uxHighWaterMark;
    // uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    // Serial.print("touch_task uxTaskGetStackHighWaterMark:");
    // Serial.println(uxHighWaterMark);

    // uint32_t ulNotifiedValue = 0;

    for (;;)
    {
        delay(100);

        // read and set array returning the current touched
      readAndSetTouchArray();

    }

    vTaskDelete(NULL);
}

void touch_deal_with_message(messageParts message)
{
    std::string identifier = message.identifier;

    if (identifier.compare("TTHRSLD") == 0)
    {
        auto touchThreshold = message.value1;
        auto releaseThreshold = message.value2;

        // store these in the NVM
        preferences.putUShort("tch_threshold", touchThreshold);
        preferences.putUShort("tch_release", releaseThreshold);

        xSemaphoreTake(i2cSemaphore, portMAX_DELAY);

        checkI2Cerrors("Touch (touch_deal_with_message)");

        cap.setThresholds(touchThreshold, releaseThreshold);

        checkI2Cerrors("Touch (touch_deal_with_message)");

        // give back the i2c flag for the next task
        xSemaphoreGive(i2cSemaphore);
    }
}

// function to set the device and set the array
uint16_t readAndSetTouchArray()
{
    // wait for the i2c semaphore flag to become available
    xSemaphoreTake(i2cSemaphore, portMAX_DELAY);

    checkI2Cerrors("Touch (readAndSetTouchArray)");

    // Get the currently touched pads
    uint16_t currtouched = cap.touched();

    checkI2Cerrors("Touch (readAndSetTouchArray)");

    // give back the i2c flag for the next task
    xSemaphoreGive(i2cSemaphore);

    // always update the array for the touch state command
    for (uint8_t i = 0; i < 12; i++)
    {
        // it if *is* touched and *wasnt* touched before, alert!
        if ((currtouched & _BV(i)) && !(lasttouched & _BV(i)))
        {
            std::string touchStates = "TTOUCHED:" + std::to_string(i);

            sendToMicrobit(touchStates);
        }

        // if it *was* touched and now *isnt*, alert!
        if (!(currtouched & _BV(i)) && (lasttouched & _BV(i)))
        {
            std::string touchStates = "TRELEASED:" + std::to_string(i);

            sendToMicrobit(touchStates);
        }
    }
        // remember last touch
lasttouched = currtouched; 
    
return currtouched;
    
}
