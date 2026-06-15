#include <Arduino.h>
#include "light.h"

SX1509 lights; // Create an SX1509 object to be used throughout

std::vector<LED> LEDs;

TaskHandle_t LightTask;

extern SemaphoreHandle_t i2cSemaphore;

// Local tracking enumerations and structures for non-blocking state machine execution
enum BreathePhase { PHASE_RISE, PHASE_ON, PHASE_FALL, PHASE_OFF };

struct LEDAnimationTrack {
    uint32_t lastUpdateTime = 0;
    uint32_t phaseStartTime = 0;
    BreathePhase phase = PHASE_RISE;
    bool blinkState = false;
    bool initialized = false;
    int lastWrittenIntensity = -1; // Caching variable to throttle redundant I2C bus commands
};

static LEDAnimationTrack animTracks[16];

// Forward declaration of internal thread-safe helper
static void writeLEDIntensityDirect(uint32_t pin, int intensity);

void light_setup()
{
    // Wait for the i2c semaphore flag to become available
    xSemaphoreTake(i2cSemaphore, portMAX_DELAY);

    if (!lights.begin(0x3E))
    {
        Serial.println("SX1509 for lighting not found");
        POST(4);
    }

    checkI2Cerrors("light setup init");
    xSemaphoreGive(i2cSemaphore);

    // Setup the lights to be initialized off
    for (int pin = 0; pin < 16; pin++)
    {
        LED l;
        l.pin = pin;
        l.state = pinState::off;
        l.taskHandle = NULL; // Maintained solely for signature struct alignment compatibility

        LEDs.push_back(l);

        xSemaphoreTake(i2cSemaphore, portMAX_DELAY);
        lights.pinMode(pin, ANALOG_OUTPUT); 
        lights.ledDriverInit(pin);
        lights.analogWrite(pin, LEDOFF);
        checkI2Cerrors("light pin init loop");
        xSemaphoreGive(i2cSemaphore);

        animTracks[pin].lastWrittenIntensity = LEDOFF;
    }

    xTaskCreatePinnedToCore(
        light_task,          
        "Light Task",        
        4096,                 // Clean optimized fixed stack size
        NULL,                
        light_task_Priority, 
        &LightTask, 
        1);      
}

void light_task(void *pvParameters)
{
    for (;;)
    {
        messageParts parts;

        // Use a non-blocking queue poll (0 tick wait) to execute animations simultaneously
        if (xQueueReceive(Light_Queue, &parts, 0) == pdTRUE)
        {
            std::string identifier = parts.identifier;
            uint32_t targetPin = parts.value1;

            if (targetPin < 16)
            {
                if (identifier.compare("LBLINK") == 0)
                {
                    LEDs[targetPin].OnTimeMS = parts.value2;
                    LEDs[targetPin].OffTimeMS = parts.value3;
                    LEDs[targetPin].state = blink;

                    animTracks[targetPin].lastUpdateTime = millis();
                    animTracks[targetPin].blinkState = true; 
                    writeLEDIntensityDirect(targetPin, LEDON); // Force immediate initialization state
                }
                else if (identifier.compare("LBREATHE") == 0)
                {
                    LEDs[targetPin].OnTimeMS = parts.value2;
                    LEDs[targetPin].OffTimeMS = parts.value3;
                    LEDs[targetPin].RiseTimeMS = parts.value4;
                    LEDs[targetPin].FallTimeMS = parts.value5;
                    LEDs[targetPin].state = breathe;

                    animTracks[targetPin].phase = PHASE_RISE;
                    animTracks[targetPin].phaseStartTime = millis();
                    animTracks[targetPin].initialized = true;
                }
                else if (identifier.compare("LLEDONOFF") == 0)
                {
                    TurnLEDOnOff(targetPin, parts.value2);
                }
                else if (identifier.compare("LLEDINTENSITY") == 0)
                {
                    TurnLEDOnWithIntensity(targetPin, parts.value2);
                }        
                else if (identifier.compare("LLEDALLOFF") == 0)
                {
                    for (uint32_t i = 0; i < 16; i++)
                    {
                        TurnLEDOnOff(i, 0);
                    }
                }
                else if (identifier.compare("LLEDALLON") == 0)
                {
                    for (uint32_t i = 0; i < 16; i++)
                    {
                        TurnLEDOnOff(i, 1);
                    }
                }
            }
        }

        // Asynchronous structural evaluation of active dynamic display tasks
        uint32_t now = millis();
        for (uint32_t pin = 0; pin < 16; pin++)
        {
            if (LEDs[pin].state == blink)
            {
                uint32_t elapsed = now - animTracks[pin].lastUpdateTime;
                if (animTracks[pin].blinkState) 
                {
                    if (elapsed >= LEDs[pin].OnTimeMS)
                    {
                        animTracks[pin].blinkState = false;
                        animTracks[pin].lastUpdateTime = now;
                        writeLEDIntensityDirect(pin, LEDOFF);
                    }
                }
                else 
                {
                    if (elapsed >= LEDs[pin].OffTimeMS)
                    {
                        animTracks[pin].blinkState = true;
                        animTracks[pin].lastUpdateTime = now;
                        writeLEDIntensityDirect(pin, LEDON);
                    }
                }
            }
            else if (LEDs[pin].state == breathe)
            {
                if (!animTracks[pin].initialized)
                {
                    animTracks[pin].phase = PHASE_RISE;
                    animTracks[pin].phaseStartTime = now;
                    animTracks[pin].initialized = true;
                }

                uint32_t elapsed = now - animTracks[pin].phaseStartTime;

                switch (animTracks[pin].phase)
                {
                    case PHASE_RISE:
                    {
                        // Defensively clear zero-bounds hazards to avoid calculations crashes
                        uint32_t duration = LEDs[pin].RiseTimeMS > 0 ? LEDs[pin].RiseTimeMS : 1;
                        if (elapsed >= duration)
                        {
                            animTracks[pin].phase = PHASE_ON;
                            animTracks[pin].phaseStartTime = now;
                            writeLEDIntensityDirect(pin, 0); // Fully illuminating point
                        }
                        else
                        {
                            int intensity = 255 - ((255 * elapsed) / duration);
                            writeLEDIntensityDirect(pin, intensity);
                        }
                        break;
                    }
                    case PHASE_ON:
                    {
                        if (elapsed >= LEDs[pin].OnTimeMS)
                        {
                            animTracks[pin].phase = PHASE_FALL;
                            animTracks[pin].phaseStartTime = now;
                            writeLEDIntensityDirect(pin, 0);
                        }
                        break;
                    }
                    case PHASE_FALL:
                    {
                        uint32_t duration = LEDs[pin].FallTimeMS > 0 ? LEDs[pin].FallTimeMS : 1;
                        if (elapsed >= duration)
                        {
                            animTracks[pin].phase = PHASE_OFF;
                            animTracks[pin].phaseStartTime = now;
                            writeLEDIntensityDirect(pin, 255); // Completely dark point
                        }
                        else
                        {
                            int intensity = (255 * elapsed) / duration;
                            writeLEDIntensityDirect(pin, intensity);
                        }
                        break;
                    }
                    case PHASE_OFF:
                    {
                        if (elapsed >= LEDs[pin].OffTimeMS)
                        {
                            animTracks[pin].phase = PHASE_RISE;
                            animTracks[pin].phaseStartTime = now;
                            writeLEDIntensityDirect(pin, 255);
                        }
                        break;
                    }
                }
            }
        }

        // Thread suspension yielding resource control smoothly (~100Hz updates)
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    vTaskDelete(NULL);
}

static void writeLEDIntensityDirect(uint32_t pin, int intensity)
{
    // Bounds limitation verification
    if (intensity < 0) intensity = 0;
    if (intensity > 255) intensity = 255;

    // Check cache: If intensity matches what is already deployed on the SX1509 chip, skip entirely
    if (animTracks[pin].lastWrittenIntensity == intensity)
    {
        return;
    }

    xSemaphoreTake(i2cSemaphore, portMAX_DELAY);
    lights.analogWrite(pin, intensity);
    checkI2Cerrors("light direct step write");
    xSemaphoreGive(i2cSemaphore);

    animTracks[pin].lastWrittenIntensity = intensity;
}

void TurnLEDOnWithIntensity(uint32_t pin, int intensity)
{
    if (pin >= 16) return;

    LEDs[pin].state = on;
    animTracks[pin].initialized = false; // Reset background execution loops tracker
    writeLEDIntensityDirect(pin, intensity);
}

void TurnLEDOnOff(uint32_t pin, int OnOff)
{
    if (pin >= 16) return;

    if (OnOff == 1)
    {
        LEDs[pin].state = on;
        animTracks[pin].initialized = false;
        writeLEDIntensityDirect(pin, LEDON);
    }
    else
    {
        LEDs[pin].state = pinState::off;
        animTracks[pin].initialized = false;
        writeLEDIntensityDirect(pin, LEDOFF);
    }
}

// Dead method maintained solely to secure compatibility signatures with outside project files
void stopCurrentTaskOnPin(uint32_t pin)
{
    // Deliberately empty: Execution safely handled on the centralized state-machine loop
}
