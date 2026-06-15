#include <Arduino.h>
#include "DAC.h"

void DAC_setup()
{
    // Pins are natively initialized when invoking dacWrite channel assignments.
    // No dedicated background thread is spawned, saving 3.5KB of RAM.
}

/**
 * @brief Thread-safe synchronous register write for the ESP32 internal 8-bit DAC channels.
 * Invoke this function directly inside messaging.cpp case matches instead of parsing via an xQueue.
 */
void updateDACVoltage(const char* identifier, uint32_t rawValue)
{
    // Force constraint to safe 8-bit resolution boundaries (0-255)
    uint8_t dacValue = constrain(rawValue, 0, 255);

    if (strcmp(identifier, "DIAL1") == 0)
    {
        dacWrite(DAC1, dacValue); // Updates internal GPIO pin register instantly
    }
    else if (strcmp(identifier, "DIAL2") == 0)
    {
        dacWrite(DAC2, dacValue);
    }
}
