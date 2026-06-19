#include <Arduino.h>
#include "switch.h"
#include <SparkFunSX1509.h>

SX1509 switches;
TaskHandle_t SwitchTask;
std::string previousSwitchStates = "HHHHHHHHHHHHHHHH";
volatile byte switchArray[16] = {};

void switch_setup()
{
    xSemaphoreTake(i2cSemaphore, portMAX_DELAY);
    if (!switches.begin(0x3F))
    {
        Serial.println("SX1509 for switching not found");
        POST(2);
    }
    switches.debounceTime(64);
    for (size_t i = 0; i < 16; i++)
    {
        switches.pinMode(i, INPUT_PULLUP);
        switches.debouncePin(i);
    }
    xSemaphoreGive(i2cSemaphore);

    previousSwitchStates = readAndSetSwitchArray();
    xTaskCreatePinnedToCore(switch_task, "Switch Task", 3000, NULL, switch_task_Priority, &SwitchTask, 1);
}

void switch_task(void *pvParameters)
{
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        std::string SwitchStates = readAndSetSwitchArray();

        if (SwitchStates.compare(previousSwitchStates) != 0)
        {
            sendToMicrobit("SUPDATE:" + SwitchStates);
        }
        previousSwitchStates = SwitchStates;
    }
}

std::string readAndSetSwitchArray()
{
    std::string switchStates = "";
    switchStates.reserve(16);

    xSemaphoreTake(i2cSemaphore, portMAX_DELAY);
    for (size_t i = 0; i < 16; i++)
    {
        int val = switches.digitalRead(i);
        switchArray[i] = val;
        switchStates.append(val == LOW ? "L" : "H");
    }
    xSemaphoreGive(i2cSemaphore);

    return switchStates;
}
