#pragma once

#include "messaging.h"
#include "defines.h"

void DAC_setup();
void DAC_task(void *pvParameter);
void updateDACVoltage(const char* identifier, uint32_t rawValue);

extern void checkI2Cerrors(std::string area);
extern QueueHandle_t DAC_Queue;