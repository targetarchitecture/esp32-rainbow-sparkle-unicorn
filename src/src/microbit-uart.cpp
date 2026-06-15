#include <Arduino.h>
#include "microbit-uart.h"

TaskHandle_t MicrobitRXTask;
TaskHandle_t MicrobitTXTask;

QueueHandle_t Microbit_Receive_Queue;
QueueHandle_t Microbit_Transmit_Queue;

#define SHOW_SERIAL 1

void microbit_setup()
{
    Microbit_Transmit_Queue = xQueueCreate(50, MAXBBCMESSAGELENGTH);

    /* Configure parameters of an UART driver, communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    uart_param_config(UART_NUM_2, &uart_config);

    // Set UART pins to the BBC Microbit (UART2)
    uart_set_pin(BBC_UART_NUM, ESP_TX_MICROBIT_RX, ESP_RX_MICROBIT_TX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // Install UART driver, and get the queue (buffer size = 1024)
    uart_driver_install(BBC_UART_NUM, RX_BUF_SIZE, TX_BUF_SIZE, 50, &Microbit_Receive_Queue, 0);

    // apparently pattern detection not actually needed! (2.8.22)
    // uart_enable_pattern_det_intr(UART_NUM_2, PATTERN_FROM_MICROBIT, PATTERN_LEN, 0, 0,0);

    // The one below doesn't seem super reliable
    // uart_enable_pattern_det_baud_intr(UART_NUM_2, PATTERN_FROM_MICROBIT, PATTERN_LEN, 0, 0, 0);

    // uart_pattern_queue_reset(UART_NUM_2, 50); //used to be 20

    // Create a task to handler UART event from ISR
    xTaskCreate(microbit_receive_task, "Microbit RX Task", 4096, NULL, BBC_RX_Priority, &MicrobitRXTask);

    // Create a task to handle send UART data to Microbit
    xTaskCreatePinnedToCore(microbit_transmit_task, "Microbit TX Task", 2048, NULL, BBC_TX_Priority, &MicrobitTXTask, 1);
}

void microbit_receive_task(void *pvParameters)
{
    // UBaseType_t uxHighWaterMark;
    // uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    // Serial.print("microbit_receive_task uxTaskGetStackHighWaterMark:");
    // Serial.println(uxHighWaterMark);

    uart_event_t uart_event;
    uint8_t *received_buffer = (uint8_t *)malloc(RX_BUF_SIZE);
    std::string accumulation_buffer = "";

    std::string receivedMsg;

    size_t datalen;

    while (true)
    {
        if (xQueueReceive(Microbit_Receive_Queue, &uart_event, portMAX_DELAY))
        {
            switch (uart_event.type)
            {
            case UART_DATA:
            {

                int len = uart_read_bytes(UART_NUM_2, received_buffer, uart_event.size, portMAX_DELAY);
                for (int i = 0; i < len; i++)
                {
                    char c = (char)received_buffer[i];
                    if (c == '\n')
                    {
                        if (!accumulation_buffer.empty())
                        {
                            dealWithMessage(accumulation_buffer);
                            accumulation_buffer.clear();
                        }
                    }
                    else if (c != '\r') // Strip carriage returns if present
                    {
                        accumulation_buffer += c;
                    }
                }
            }
            break;
            case UART_BREAK:
            {
                ESP_LOGI(TAG, "UART_BREAK");
            }
            break;
            case UART_BUFFER_FULL:
            {
                ESP_LOGI(TAG, "UART_BUFFER_FULL");
            }
            break;
            case UART_FIFO_OVF:
            {
                ESP_LOGI(TAG, "UART_FIFO_OVF");
                uart_flush_input(UART_NUM_2);
                xQueueReset(Microbit_Receive_Queue);
            }
            break;
            case UART_FRAME_ERR:
            {
                ESP_LOGI(TAG, "UART_FRAME_ERR");
            }
            break;
            case UART_PARITY_ERR:
            {
                ESP_LOGI(TAG, "UART_PARITY_ERR");
            }
            break;
            case UART_DATA_BREAK:
            {
                ESP_LOGI(TAG, "UART_DATA_BREAK");
            }
            break;
            case UART_PATTERN_DET:
            {
                // Serial << " UART_PATTERN_DET uart_event.size:" << uart_event.size << endl;

                ESP_LOGI(TAG, "UART_PATTERN_DET");
                uart_get_buffered_data_len(BBC_UART_NUM, &datalen);
                int pos = uart_pattern_pop_pos(BBC_UART_NUM);
                ESP_LOGI(TAG, "Detected %d pos %d", datalen, pos);
                uart_read_bytes(BBC_UART_NUM, received_buffer, datalen - PATTERN_LEN, pdMS_TO_TICKS(100));
                uint8_t pat[PATTERN_LEN + 1];
                memset(pat, 0, sizeof(pat));
                uart_read_bytes(BBC_UART_NUM, pat, PATTERN_LEN, pdMS_TO_TICKS(100));
                // printf("data: %.*s === pattern: %s\n", datalen - PATTERN_LEN, received_buffer, pat);

                // added this extra place to catch UART messages
                receivedMsg.clear();

                for (size_t i = 0; i < uart_event.size; i++)
                {
                    char c = received_buffer[i]; // receive byte as a character

                    receivedMsg += c;
                }

#if SHOW_SERIAL
                Serial << "RX (UART_PATTERN_DET):" << receivedMsg.c_str() << endl;
#endif

                dealWithMessage(receivedMsg);
            }
            break;
            default:
            {
            }
            break;
            }
        }
    }

    free(received_buffer);
    vTaskDelete(NULL);
}

void sendToMicrobit(std::string msg)
{
    // the queue needs to work with a copy
    //char queuedMsg[MAXBBCMESSAGELENGTH];
    //strcpy(queuedMsg, msg.c_str());

//15.6.26 - gemini changed it to this
    strncpy(queuedMsg, msg.c_str(), sizeof(queuedMsg) - 1);
     queuedMsg[sizeof(queuedMsg) - 1] = '\0';

    xQueueSend(Microbit_Transmit_Queue, &queuedMsg, portMAX_DELAY);
}

void microbit_transmit_task(void *pvParameters)
{
    for (;;)
    {
        char msg[MAXBBCMESSAGELENGTH + 1] = {0};

        if (xQueueReceive(Microbit_Transmit_Queue, &msg, portMAX_DELAY))
        {
#if SHOW_SERIAL
            Serial << "TX:" << msg << endl;
#endif

            // append # to the end so the microbit knows the end of the line
            strcat(msg, "\n");

            // int bytes =
            uart_write_bytes(BBC_UART_NUM, msg, strlen(msg));

            // Added on 1/12/20 to try to stop flooding
            delay(5);
        }
    }

    vTaskDelete(NULL);
}
