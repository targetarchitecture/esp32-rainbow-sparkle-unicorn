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
    xSemaphoreTake(i2cSemaphore, portMAX_DELAY);
    
    // Read all 16 pins in one single I2C bus transaction
    uint16_t pinDataWord = switches.readWord(0x12); // 0x12 is REG_DATA_B/A base

    xSemaphoreGive(i2cSemaphore);

    std::string switchStates = "";
    for (size_t i = 0; i < 16; i++)
    {
        // Extract bit status
        bool isLow = (pinDataWord & (1 << i)) == 0;
        switchArray[i] = isLow ? LOW : HIGH;
        switchStates.append(isLow ? "L" : "H");
    }

    return switchStates;
}
