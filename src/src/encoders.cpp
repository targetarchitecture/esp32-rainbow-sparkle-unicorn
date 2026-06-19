#include <Arduino.h>
#include "encoders.h"

ESP32Encoder encoder1;
ESP32Encoder encoder2;

// Volatile allocations mapping dropped completely; live lookups execute via hardware registers
void encoders_setup()
{
// Scoped enum reference resolves definition mapping errors
    ESP32Encoder::useInternalWeakPullResistors = puType::DOWN;

    encoder1.attachHalfQuad(ROTARY1CK, ROTARY1DT);
    encoder1.clearCount();
    encoder1.setCount(0);

    encoder2.attachHalfQuad(ROTARY2CK, ROTARY2DT);
    encoder2.clearCount();
    encoder2.setCount(0);
    
    // Background execution task completely extracted to maximize scheduler clock distribution efficiency
}
