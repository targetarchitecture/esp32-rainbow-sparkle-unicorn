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
    xSemaphoreTake(i2cSemaphore, portMAX_DELAY);
    // Execute single 16-bit transaction reading all pins instantly
    uint16_t pinDataWord = switches.readWord(0x12); 
    xSemaphoreGive(i2cSemaphore);

    std::string switchStates = "";
    for (size_t i = 0; i < 16; i++)
    {
        bool isLow = (pinDataWord & (1 << i)) == 0;
        switchArray[i] = isLow ? LOW : HIGH;
        switchStates.append(isLow ? "L" : "H");
    }
    return switchStates;
}
