#include <Arduino.h>
#include "ADC.h"

void ADC_setup()
{
    // Configure target pins as analog inputs
    pinMode(ADC1, INPUT);
    pinMode(ADC2, INPUT);
    
    // ESP32 ADC default resolution is 12-bit (0-4095). 
    // If your micro:bit blocks expect 10-bit values (0-1023), uncomment the line below:
    // analogReadResolution(10);
}

/**
 * @brief Optional helper function to filter analog read noise via multi-sample averaging.
 * Replace raw analogRead(ADC1) in messaging.cpp with this if sliders bounce.
 */
uint16_t getSmoothedAnalogRead(uint8_t pin)
{
    uint32_t accumulator = 0;
    const uint8_t samples = 8;
    
    for (uint8_t i = 0; i < samples; i++)
    {
        accumulator += analogRead(pin);
    }
    
    return static_cast<uint16_t>(accumulator / samples);
}
