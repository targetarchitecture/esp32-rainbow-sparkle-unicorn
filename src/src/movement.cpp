#include <Arduino.h>
#include "movement.h"
#include "easing.h"

extern SemaphoreHandle_t i2cSemaphore;
Adafruit_PWMServoDriver PCA9685 = Adafruit_PWMServoDriver();
QueueHandle_t Movement_i2c_Queue;
TaskHandle_t MovementTask;
TaskHandle_t Movementi2cTask;

std::vector<servo> servos;

struct ServoActiveTrack {
    uint32_t startTime = 0;
    uint32_t durationMillis = 0;
    uint16_t fromMapped = 0;
    uint16_t toMapped = 0;
    uint16_t change = 0;
    uint16_t lastPwm = 0;
    bool isMoving = false;
};

static ServoActiveTrack moveTracks[16];

void movement_setup()
{
    xSemaphoreTake(i2cSemaphore, portMAX_DELAY);
    Wire.beginTransmission(64);
    if (Wire.endTransmission() != 0)
    {
        Serial.println("PCA9685 not found");
        POST(5);
    }

    PCA9685.reset();
    PCA9685.setOscillatorFrequency(27000000);
    PCA9685.setPWMFreq(50); 
    xSemaphoreGive(i2cSemaphore);

    for (uint8_t i = 0; i <= 15; i++)
    {
        servo S = {i, 2, 0, 0, 100, 505, LinearInOut, false, NULL};
        servos.push_back(S);
    }

    Movement_i2c_Queue = xQueueCreate(100, sizeof(servoPWM));

    xTaskCreatePinnedToCore(movement_task, "Movement Task", 4096, NULL, MovementTask_Priority, &MovementTask, 1);
    xTaskCreatePinnedToCore(movement_i2c_task, "Movement i2c Task", 3000, NULL, Movementi2cTask_Priority, &Movementi2cTask, 1);
}

void movement_task(void *pvParameters)
{
    for (;;)
    {
        messageParts parts;
        
        // Non-blocking query to handle continuous array steps update calculations safely
        if (xQueueReceive(Movement_Queue, &parts, 0) == pdTRUE)
        {
            std::string identifier = parts.identifier;
            uint8_t pin = parts.value1;

            if (pin < 16)
            {
                moveTracks[pin].isMoving = false; // Override previous motion instances smoothly

                if (identifier.compare("MANGLE") == 0)
                {
                    setServoAngle(pin, parts.value2, parts.value3, parts.value4);
                }
                else if (identifier.compare("MPWM") == 0)
                {
                    setServoPWM(pin, constrain(parts.value2, 0, 4096));
                }
                else if (identifier.compare("MLINEAR") == 0 || identifier.compare("MSMOOTH") == 0 || identifier.compare("MBOUNCY") == 0)
                {
                    easingCurves curve = LinearInOut;
                    if (identifier.compare("MSMOOTH") == 0) curve = QuadraticInOut;
                    if (identifier.compare("MBOUNCY") == 0) curve = BounceInOut;

                    servos[pin].minPulse = parts.value5;
                    servos[pin].maxPulse = parts.value6;
                    
                    uint16_t fromMap = (parts.value3 * (parts.value6 - parts.value5) / 180) + parts.value5;
                    uint16_t toMap = (parts.value2 * (parts.value6 - parts.value5) / 180) + parts.value5;

                    moveTracks[pin].startTime = millis();
                    moveTracks[pin].durationMillis = parts.value4 * 1000;
                    moveTracks[pin].fromMapped = fromMap;
                    moveTracks[pin].toMapped = toMap;
                    moveTracks[pin].change = abs(fromMap - toMap);
                    moveTracks[pin].lastPwm = fromMap;
                    moveTracks[pin].isMoving = true;
                    servos[pin].easingCurve = curve;

                    setServoPWM(pin, fromMap);
                }
            }
        }

        uint32_t now = millis();
        for (uint8_t i = 0; i < 16; i++)
        {
            if (moveTracks[i].isMoving)
            {
                uint32_t elapsed = now - moveTracks[i].startTime;
                if (elapsed >= moveTracks[i].durationMillis)
                {
                    moveTracks[i].isMoving = false;
                    setServoPWM(i, moveTracks[i].toMapped);
                }
                else
                {
                    uint16_t easedPos = 0;
                    uint16_t outPwm = 0;
                    switch (servos[i].easingCurve)
                    {
                        case QuadraticInOut: easedPos = QuarticEaseInOut(moveTracks[i].change, moveTracks[i].durationMillis, elapsed); break;
                        case BounceInOut:    easedPos = BounceEaseInOut(moveTracks[i].change, moveTracks[i].durationMillis, elapsed); break;
                        default:             easedPos = LinearEaseInOut(moveTracks[i].change, moveTracks[i].durationMillis, elapsed); break;
                    }

                    if (moveTracks[i].fromMapped > moveTracks[i].toMapped) outPwm = moveTracks[i].fromMapped - easedPos;
                    else outPwm = moveTracks[i].fromMapped + easedPos;

                    if (outPwm != moveTracks[i].lastPwm)
                    {
                        setServoPWM(i, outPwm);
                        moveTracks[i].lastPwm = outPwm;
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // Sync calculations frequency updates at steady 50Hz pacing outputs
    }
}

void movement_i2c_task(void *pvParameters)
{
    for (;;)
    {
        servoPWM pwm;
        if (xQueueReceive(Movement_i2c_Queue, &pwm, portMAX_DELAY))
        {
            xSemaphoreTake(i2cSemaphore, portMAX_DELAY);
            PCA9685.setPWM(pwm.pin, 0, pwm.pwm);
            xSemaphoreGive(i2cSemaphore);
        }
    }
}

void setServoPWM(const uint8_t pin, const uint16_t PWM)
{
    servoPWM toBeQueued = {pin, PWM};
    xQueueSend(Movement_i2c_Queue, &toBeQueued, portMAX_DELAY);
}

void setServoAngle(const uint8_t pin, const uint16_t angle, const uint16_t minPulse, const uint16_t maxPulse)
{
    uint16_t PWM = (angle * (maxPulse - minPulse) / 180) + minPulse;
    setServoPWM(pin, PWM);
}

void stopServo(const uint8_t pin) { if(pin < 16) moveTracks[pin].isMoving = false; }
void setServoEase(const uint8_t pin, easingCurves easingCurve, const uint16_t toDegree, const uint16_t fromDegree, const uint16_t duration, const uint16_t minPulse, const uint16_t maxPulse) {}
