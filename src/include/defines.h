#pragma once

// #define BOARDNAME "SN10"
// #include "pin layouts\SN10 pins.h"

#define BOARDNAME "SN9"
#include "pin layouts\SN9 pins.h"

//RTOS task priorities (highest is 24) - getting these priorities wrong can make the differance from 1260ms to 347ms
#define MQTT_task_Priority 2
#define MQTT_client_task_Priority 6

#define switch_task_Priority 4
#define touch_task_Priority 4

#define MovementTask_Priority 1
#define Movementi2cTask_Priority 5
#define ServoEasingTask_Priority 3

#define LEDBlinkingTask_Priority 5
#define LEDBreathingTask_Priority 5

#define DAC_task_Priority 1
#define encoders_task_Priority 1
#define light_task_Priority 1
#define sound_task_Priority 1
#define sound_busy_task_Priority 1

#define BBC_RX_Priority 10
#define BBC_TX_Priority 10 // used to be 2

//UART variables
#define BBC_UART_NUM UART_NUM_2

#define UARTMESSAGELENGTH 128
#define MAXBBCMESSAGELENGTH 128
 
#define PATTERN_LEN 1
#define PATTERN_FROM_MICROBIT 0x0D //Carriage Return

#define RX_BUF_SIZE 1024
#define TX_BUF_SIZE 1024  
