#pragma once

#include "defines.h"
#include <ESP32Encoder.h>

void encoders_setup();
void encoders_task(void *pvParameter);

// Expose instances globally to messaging.cpp
extern ESP32Encoder encoder1;
extern ESP32Encoder encoder2;