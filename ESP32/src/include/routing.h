#pragma once

#include "messageParts.h"
#include "defines.h"
#include "SN7 pins.h"

void routing_task(void *pvParameters);
void routing_setup();
                                   
extern void encoders_deal_with_message(char msg[MAXESP32MESSAGELENGTH]);
extern void ADC_deal_with_message(char msg[MAXESP32MESSAGELENGTH]);
extern void touch_deal_with_message(char msg[MAXESP32MESSAGELENGTH]);
extern void switch_deal_with_message(char msg[MAXESP32MESSAGELENGTH]);
extern void MQTT_deal_with_message(char msg[MAXESP32MESSAGELENGTH]);
extern void checkI2Cerrors(const char *area);
extern void sendToMicrobit(char msg[MAXBBCMESSAGELENGTH]);

extern QueueHandle_t DAC_Queue;
extern QueueHandle_t Sound_Queue;
extern QueueHandle_t Microbit_Receive_Queue;
extern QueueHandle_t Light_Queue;
extern QueueHandle_t Movement_Queue;
extern QueueHandle_t MQTT_Queue;
