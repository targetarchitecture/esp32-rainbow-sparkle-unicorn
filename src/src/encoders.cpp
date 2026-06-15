#include <Arduino.h>
#include "encoders.h"

ESP32Encoder encoder1;
ESP32Encoder encoder2;

volatile int32_t encoder1Count = 0;
volatile int32_t encoder2Count = 0;

void encoders_setup()
{
    // Enable the weak pull up/down resistors
    ESP32Encoder::useInternalWeakPullResistors = DOWN; // UP;

    // Attach pins for use as encoder pins
    encoder1.attachHalfQuad(ROTARY1CK, ROTARY1DT);
    encoder1.clearCount();
    encoder1.setCount(0);

    // Attach pins for use as encoder pins
    encoder2.attachHalfQuad(ROTARY2CK, ROTARY2DT);
    encoder2.clearCount();
    encoder2.setCount(0);
}
