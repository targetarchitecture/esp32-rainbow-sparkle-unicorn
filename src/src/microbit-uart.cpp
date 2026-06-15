#include <Arduino.h>
#include <cstring>
#include "microbit-uart.h"
#include "messaging.h"

TaskHandle_t MicrobitRXTask;
TaskHandle_t MicrobitTXTask;
QueueHandle_t Microbit_Receive_Queue;
QueueHandle_t Microbit_Transmit_Queue;

#define SHOW_SERIAL 1

void microbit_setup()
{
    Microbit_Transmit_Queue = xQueueCreate(50, MAXBBCMESSAGELENGTH);

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    uart_param_config(UART_NUM_2, &uart_config);
    uart_set_pin(BBC_UART_NUM, ESP_TX_MICROBIT_RX, ESP_RX_MICROBIT_TX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(BBC_UART_NUM, RX_BUF_SIZE, TX_BUF_SIZE, 50, &Microbit_Receive_Queue, 0);

    xTaskCreate(microbit_receive_task, "Microbit RX Task", 4096, NULL, BBC_RX_Priority, &MicrobitRXTask);
    xTaskCreatePinnedToCore(microbit_transmit_task, "Microbit TX Task", 2048, NULL, BBC_TX_Priority, &MicrobitTXTask, 1);
}

void microbit_receive_task(void *pvParameters)
{
    uart_event_t uart_event;
    uint8_t *received_buffer = (uint8_t *)malloc(RX_BUF_SIZE);
    std::string accumulation_buffer = "";

    while (true)
    {
        if (xQueueReceive(Microbit_Receive_Queue, &uart_event, portMAX_DELAY))
        {
            if (uart_event.type == UART_DATA)
            {
                int len = uart_read_bytes(UART_NUM_2, received_buffer, uart_event.size, portMAX_DELAY);
                for (int i = 0; i < len; i++)
                {
                    char c = (char)received_buffer[i];
                    if (c == '\n')
                    {
                        if (!accumulation_buffer.empty())
                        {
#if SHOW_SERIAL
                            Serial << "RX Frame Complete: " << accumulation_buffer.c_str() << endl;
#endif
                            dealWithMessage(accumulation_buffer);
                            accumulation_buffer.clear();
                        }
                    }
                    else if (c != '\r')
                    {
                        accumulation_buffer += c;
                    }
                }
            }
            else if (uart_event.type == UART_FIFO_OVF)
            {
                ESP_LOGE("UART", "FIFO Overflow Detected");
                uart_flush_input(UART_NUM_2);
                xQueueReset(Microbit_Receive_Queue);
                accumulation_buffer.clear();
            }
        }
    }
    free(received_buffer);
    vTaskDelete(NULL);
}

void sendToMicrobit(std::string msg)
{
    char queuedMsg[MAXBBCMESSAGELENGTH];
    strncpy(queuedMsg, msg.c_str(), sizeof(queuedMsg) - 1);
    queuedMsg[sizeof(queuedMsg) - 1] = '\0';

    xQueueSend(Microbit_Transmit_Queue, &queuedMsg, portMAX_DELAY);
}

void microbit_transmit_task(void *pvParameters)
{
    for (;;)
    {
        char msg[MAXBBCMESSAGELENGTH + 2] = {0}; // Extra space for newline append boundary

        if (xQueueReceive(Microbit_Transmit_Queue, &msg, portMAX_DELAY))
        {
            size_t currentLen = strlen(msg);
            if (currentLen < MAXBBCMESSAGELENGTH)
            {
                msg[currentLen] = '\n';
                msg[currentLen + 1] = '\0';
            }
#if SHOW_SERIAL
            Serial << "TX Frame: " << msg;
#endif
            uart_write_bytes(BBC_UART_NUM, msg, strlen(msg));
            vTaskDelay(pdMS_TO_TICKS(5)); // Rate limiting spacing guard
        }
    }
    vTaskDelete(NULL);
}
