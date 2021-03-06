#include <Arduino.h>
#include "DAC.h"

TaskHandle_t DACTask;
//QueueHandle_t DAC_Queue;

void DAC_setup()
{
  //DAC_Queue = xQueueCreate(50, sizeof(RXfromBBCmessage));

    xTaskCreatePinnedToCore(
        DAC_task,          /* Task function. */
        "DAC Task",        /* name of task. */
        3500,              /* Stack size of task (uxTaskGetStackHighWaterMark = 3204) */
        NULL,              /* parameter of the task */
        DAC_task_Priority, /* priority of the task */
        &DACTask, 1);      /* Task handle to keep track of created task */
}

void DAC_task(void *pvParameters)
{
    // UBaseType_t uxHighWaterMark;
    // uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    // Serial.print("DAC_task uxTaskGetStackHighWaterMark:");
    // Serial.println(uxHighWaterMark);

    messageParts parts;
    int8_t DACvalue;

    for (;;)
    {
        char msg[MAXESP32MESSAGELENGTH] = {0};

        //wait for new DAC commands in the queue
        xQueueReceive(DAC_Queue, &msg, portMAX_DELAY);

        //Serial.printf("DAC_Queue: %s\n", msg);

        messageParts parts = processQueueMessage(msg, "DAC");
 
        DACvalue = constrain(atoi(parts.value1), 0, 254);

        //Serial.printf("DAC action: %s @ %iV\n", parts.identifier, DACvalue);.

        if (strncmp(parts.identifier, "X1",2) == 0)
        {
            dacWrite(DAC1, DACvalue);
        }
        else if (strncmp(parts.identifier, "X2",2) == 0)
        {
            dacWrite(DAC2, DACvalue);
        }
    }

    vTaskDelete(NULL);
}