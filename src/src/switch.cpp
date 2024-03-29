#include <Arduino.h>
#include "switch.h"
#include <SparkFunSX1509.h> // Include SX1509 library

SX1509 switches; // Create an SX1509 object to be used throughout

TaskHandle_t SwitchTask;

std::string previousSwitchStates;

volatile byte switchArray[16] = {};

void switch_setup()
{
    //wait for the i2c semaphore flag to become available
    xSemaphoreTake(i2cSemaphore, portMAX_DELAY);

    if (!switches.begin(0x3F))
    {
        Serial.println("SX1509 for switching not found");

        POST(2);
    }

    //this is the maximum acros all pins
    switches.debounceTime(64);

    for (size_t i = 0; i < 16; i++)
    {
        switches.pinMode(i, INPUT_PULLUP);
        switches.debouncePin(i);
    }

    checkI2Cerrors("switch");

    //give back the i2c flag for the next task
    xSemaphoreGive(i2cSemaphore);

    xTaskCreatePinnedToCore(
        switch_task,          /* Task function. */
        "Switch Task",        /* name of task. */
        3000,                 /* Stack size of task (uxTaskGetStackHighWaterMark: 8204)   */
        NULL,                 /* parameter of the task */
        switch_task_Priority, /* priority of the task */
        &SwitchTask, 1);      /* Task handle to keep track of created task */
}

void switch_task(void *pvParameters)
{
    /* Inspect our own high water mark on entering the task. */
    // UBaseType_t uxHighWaterMark;
    // uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    // Serial.print("switch_task uxTaskGetStackHighWaterMark:");
    // Serial.println(uxHighWaterMark);

    previousSwitchStates = readAndSetSwitchArray();

    // BaseType_t xResult;

    for (;;)
    {
        // put a delay so it isn't overwhelming
        delay(100);

        std::string SwitchStates = readAndSetSwitchArray();

        //only bother sending a touch update command if the touch changed
        if (SwitchStates.compare(previousSwitchStates) != 0)
        {
            std::string msg = "SUPDATE:";
            msg.append(SwitchStates);

            sendToMicrobit(msg);
        }

        //remember last switch states
        previousSwitchStates = SwitchStates;
    }

    vTaskDelete(NULL);
}

//function to set the device and set the array
std::string readAndSetSwitchArray()
{
    //wait for the i2c semaphore flag to become available
    xSemaphoreTake(i2cSemaphore, portMAX_DELAY);

    checkI2Cerrors("switch (switch_task start)");

    //quickly read all of the pins and save the state
    for (size_t i = 0; i < 16; i++)
    {
        switchArray[i] = switches.digitalRead(i);
    }

    checkI2Cerrors("switch (switch_task end)");

    //give back the i2c flag for the next task
    xSemaphoreGive(i2cSemaphore);

    //build the switch states string
    std::string swithStates;

    for (size_t i = 0; i < 16; i++)
    {
        if (switchArray[i] == LOW)
        {
            swithStates.append("L");
        }
        else
        {
            swithStates.append("H");
        }
    }

    return swithStates;
}
