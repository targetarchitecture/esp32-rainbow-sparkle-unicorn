#include <Arduino.h>
#include "movement.h"

extern SemaphoreHandle_t i2cSemaphore;

Adafruit_PWMServoDriver PCA9685 = Adafruit_PWMServoDriver();

QueueHandle_t Movement_i2c_Queue;

TaskHandle_t MovementTask;
TaskHandle_t Movementi2cTask;

std::vector<servo> servos;

void movement_setup()
{
    xSemaphoreTake(i2cSemaphore, portMAX_DELAY);

    // see if it's actually connected
    Wire.beginTransmission(64);
    auto error = Wire.endTransmission();

    if (error != 0)
    {
        Serial.println("SX1509 for switching not found");

        POST(5);
    }

    // don't need to call pwm.begin as this reinitialise the i2c
    // PCA9685.begin
    PCA9685.reset();
    PCA9685.setOscillatorFrequency(27000000);
    PCA9685.setPWMFreq(50); // Analog servos run at ~50 Hz updates

    checkI2Cerrors("movement");

    xSemaphoreGive(i2cSemaphore);

    delay(10);

    // set up struct array for the servos
    for (uint8_t i = 0; i <= 15; i++)
    {
        servo S;

        S.pin = i;
        S.duration = 2;
        S.toDegree = 0;
        S.fromDegree = 0;
        S.minPulse = 100;
        S.maxPulse = 505;
        S.easingCurve = LinearInOut;
        S.interuptEasing = false;
        S.taskHandle = NULL;

        servos.push_back(S);
    }

    // create a queue to hold servo PWM values (allows us to kill the servo easing processes at anytime)
    Movement_i2c_Queue = xQueueCreate(100, sizeof(servoPWM));

    // this task is to recieve the servo messages
    xTaskCreatePinnedToCore(
        movement_task,         /* Task function. */
        "Movement Task",       /* name of task. */
        3000,                  /* Stack size of task (2780) */
        NULL,                  /* parameter of the task */
        MovementTask_Priority, /* priority of the task */
        &MovementTask,         /* Task handle to keep track of created task */
        1);                    /* core */

    // this task is to perform pwms for the servos
    xTaskCreatePinnedToCore(
        movement_i2c_task,        /* Task function. */
        "Movement i2c Task",      /* name of task. */
        3000,                     /* Stack size of task (2700) */
        NULL,                     /* parameter of the task */
        Movementi2cTask_Priority, /* priority of the task */
        &Movementi2cTask,         /* Task handle to keep track of created task */
        1);                       /* core */
}

void movement_task(void *pvParameters)
{
    /* Inspect our own high water mark on entering the task. */
    // UBaseType_t uxHighWaterMark;
    // uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    // Serial.print("movement_task uxTaskGetStackHighWaterMark:");
    // Serial.println(uxHighWaterMark);

    for (;;)
    {
        messageParts parts;

        // wait for new movement command in the queue
        xQueueReceive(Movement_Queue, &parts, portMAX_DELAY);

        std::string identifier = parts.identifier;

        // OK OK - What ever your doing stop the servo task first
        uint8_t stopPin = parts.value1;
        stopServo(stopPin);

        if (identifier.compare("MSTOP") == 0)
        {
            // not much to do now as we always stop the servos

            // Stop servo
            uint8_t pin = parts.value1;
            stopServo(pin);
        }
        else if (identifier.compare("MANGLE") == 0)
        {
            // Set servo to angle
            uint8_t pin = parts.value1;
            int16_t angle = parts.value2;
            int16_t minPulse = parts.value3;
            int16_t maxPulse = parts.value4;

            // Serial << "setServoAngle(" << pin << "," << angle << "," << minPulse << "," << maxPulse << ")" << endl;

            setServoAngle(pin, angle, minPulse, maxPulse);
        }
        else if (identifier.compare("MLINEAR") == 0)
        {
            // Set servo to angle.
            uint8_t pin = parts.value1;
            int16_t toDegree = parts.value2;
            int16_t fromDegree = parts.value3;
            int16_t duration = parts.value4;
            int16_t minPulse = parts.value5;
            int16_t maxPulse = parts.value6;

            // Serial << "motion: " << identifier.c_str() << endl;

            setServoEase(pin, LinearInOut, toDegree, fromDegree, duration, minPulse, maxPulse);
        }
        else if (identifier.compare("MSMOOTH") == 0)
        {
            // Set servo to angle
            uint8_t pin = parts.value1;
            uint16_t toDegree = parts.value2;
            uint16_t fromDegree = parts.value3;
            uint16_t duration = parts.value4;
            uint16_t minPulse = parts.value5;
            uint16_t maxPulse = parts.value6;

            setServoEase(pin, QuadraticInOut, toDegree, fromDegree, duration, minPulse, maxPulse);
        }
        else if (identifier.compare("MBOUNCY") == 0)
        {
            // Serial.print("BounceInOut on pin ");

            // Set servo to angle
            uint8_t pin = parts.value1;
            uint16_t toDegree = parts.value2;
            uint16_t fromDegree = parts.value3;
            uint16_t duration = parts.value4;
            uint16_t minPulse = parts.value5;
            uint16_t maxPulse = parts.value6;

            setServoEase(pin, BounceInOut, toDegree, fromDegree, duration, minPulse, maxPulse);
        }

        else if (identifier.compare("MPWM") == 0)
        {
            // Set servo to PWM
            uint8_t pin = parts.value1;
            uint16_t PWM = constrain(parts.value2, 0, 4096);

            setServoPWM(pin, PWM);
        }
    }

    vTaskDelete(NULL);
}

void movement_i2c_task(void *pvParameters)
{
    /* Inspect our own high water mark on entering the task. */
    // UBaseType_t uxHighWaterMark;
    // uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    // Serial.print("movement_i2c_task uxTaskGetStackHighWaterMark:");
    // Serial.println(uxHighWaterMark);

    for (;;)
    {
        servoPWM pwm;

        // wait for the next PWM command to be queued - timing is one each loop making it simpler
        xQueueReceive(Movement_i2c_Queue, &pwm, portMAX_DELAY);

        // wait for the i2c semaphore flag to become available
        xSemaphoreTake(i2cSemaphore, portMAX_DELAY);

        // configure the PWM duty cycle
        PCA9685.setPWM(pwm.pin, 0, pwm.pwm);

        checkI2Cerrors("movement (movement_i2c_task)");

        xSemaphoreGive(i2cSemaphore);

        delay(1);
    }

    vTaskDelete(NULL);
}

double mapAngles(const uint16_t x, const uint16_t in_min, const uint16_t in_max, const uint16_t out_min, const uint16_t out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void setServoEase(const uint8_t pin, easingCurves easingCurve, const uint16_t toDegree, const uint16_t fromDegree, const uint16_t duration, const uint16_t minPulse, const uint16_t maxPulse)
{
    // set variables
    servos[pin].duration = duration;
    servos[pin].toDegree = toDegree;
    servos[pin].fromDegree = fromDegree;
    servos[pin].minPulse = minPulse;
    servos[pin].maxPulse = maxPulse;
    servos[pin].easingCurve = easingCurve;
    servos[pin].interuptEasing = false;

    // Serial << "pin:" << pin << endl;
    // Serial << "duration:" << duration << endl;
    // Serial << "toDegree:" << toDegree << endl;
    // Serial << "fromDegree:" << fromDegree << endl;
    // Serial << "minPulse:" << minPulse << endl;
    // Serial << "maxPulse:" << maxPulse << endl;
    // Serial << "easingCurve:" << easingCurve << endl;

    // Create a local char array buffer (16 bytes matches the default FreeRTOS limit)
    char taskName[16];
    
    // Safely write a formatted string ("Servo_0", "Servo_1", etc.) into the buffer
    snprintf(taskName, sizeof(taskName), "Servo Task %d", pin);

    xTaskCreatePinnedToCore(
        &ServoEasingTaskV3,
        taskName,
        2000, // 10000,
        NULL,
        ServoEasingTask_Priority,
        &servos[pin].taskHandle,
        1);

    delay(10);

    xTaskNotify(servos[pin].taskHandle, pin, eSetValueWithOverwrite);

    delay(10);
}

void setServoPWM(const uint8_t pin, const uint16_t PWM)
{
    // Serial << "setServoPWM on pin " << pin << " PWM  " << PWM << endl;

    servoPWM toBeQueued;
    toBeQueued.pin = pin;
    toBeQueued.pwm = PWM;

    xQueueSend(Movement_i2c_Queue, &toBeQueued, portMAX_DELAY);
}

void setServoAngle(const uint8_t pin, const uint16_t angle, const uint16_t minPulse, const uint16_t maxPulse)
{
    // Serial.println("setServoAngle");

    servos[pin].interuptEasing = false;
    servos[pin].minPulse = minPulse;
    servos[pin].maxPulse = maxPulse;

    uint16_t PWM = mapAngles(angle, 0, 180, servos[pin].minPulse, servos[pin].maxPulse);

    // add to the queue
    setServoPWM(pin, PWM);
}

void stopServo(const uint8_t pin)
{
    // Serial.printf("STOPPING SERVO PIN: %i\n", pin);
    // u_long startTime = millis();

    servos[pin].interuptEasing = true;

    // not super efficent - but easier, just loop and check
    // As much as this looks poor - it works and the loop can execute between 1-4 times before the task handle is NULL (0-400ms)
    while (true)
    {
        if (servos[pin].taskHandle == NULL)
        {
            break;
        }

        // just adding some delay to give the task enough time to react - the task already has a 50ms delay in it
        delay(50);

        // Serial.print("#");
    }

    servos[pin].interuptEasing = false;

    // give the task delete command some time to complete
    delay(10);

    // Serial.print("pin: ");
    // Serial.print(pin);
    // Serial.print(" servo stop completed in: ");
    // Serial.print(millis() - startTime);
    // Serial.print(" @ ");
    // Serial.println(millis());

    // Serial.printf("\nSTOPPING SERVO PIN: %i COMPLETED\n", pin);
}

void ServoEasingTaskV3(void *pvParameter)
{
    // UBaseType_t uxHighWaterMark;
    // uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    // Serial.print("ServoEasingTaskV2 uxTaskGetStackHighWaterMark:");
    // Serial.println(uxHighWaterMark);

    uint32_t pin = 0;
    BaseType_t xResult = xTaskNotifyWait(0X00, 0x00, &pin, portMAX_DELAY);

    uint16_t durationMillis = servos[pin].duration * 1000;

    uint16_t toDegree = servos[pin].toDegree;
    uint16_t fromDegree = servos[pin].fromDegree;

    uint16_t minPulse = servos[pin].minPulse;
    uint16_t maxPulse = servos[pin].maxPulse;
    uint16_t easingCurve = servos[pin].easingCurve;

    uint16_t fromDegreeMapped = mapAngles(fromDegree, 0, 180, minPulse, maxPulse);
    uint16_t toDegreeMapped = mapAngles(toDegree, 0, 180, minPulse, maxPulse);

    uint16_t _change = abs(fromDegreeMapped - toDegreeMapped);

    uint16_t easedPosition = 0;
    uint16_t PWM = fromDegreeMapped;
    uint16_t previousPWM = PWM;

    // if (pin == 14 )
    // {
    //     //Serial << "ServoEasing starting pin:" << pin << " durationMillis:" << durationMillis << "ms, change:" << _change << endl;
    //     Serial << "ServoEasing starting pin:" << pin << " fromDegreeMapped:" << fromDegreeMapped << " toDegreeMapped:" << toDegreeMapped << " change:" << _change << endl;
    // }

    // set starting position (from degree PWM mapped)
    setServoPWM(pin, fromDegreeMapped);

    u_long startTime = millis();
    u_long endTime = startTime + durationMillis;

    while (millis() <= endTime)
    {
        u_long currentTime = millis() - startTime;

        switch (easingCurve)
        {
        case QuadraticInOut:
            easedPosition = QuarticEaseInOut(_change, durationMillis, currentTime);
            break;
        case BounceInOut:
            easedPosition = BounceEaseInOut(_change, durationMillis, currentTime);
            break;
        case LinearInOut:
            easedPosition = LinearEaseInOut(_change, durationMillis, currentTime);
            break;
        default:
            easedPosition = LinearEaseInOut(_change, durationMillis, currentTime);
        }

        // work out the PWM based on the direction of travel
        if (fromDegreeMapped > toDegreeMapped)
        {
            // PWM = easedPosition + minPulse;
            PWM = fromDegreeMapped - easedPosition;
        }
        else
        {
            // PWM = maxPulse - easedPosition;
            PWM = fromDegreeMapped + easedPosition;
        }

        // if (pin == 14 )
        // {
        //     Serial << "currentTime:" << currentTime << " endTime:" << endTime << " PWM:" << PWM << " easedPosition: " << easedPosition << " durationMillis:" << durationMillis << endl;
        // }

        // only send if a differance to reduce i2c traffic
        if (PWM != previousPWM)
        {
            // add to the queue
            setServoPWM(pin, PWM);
        }

        // remember the previous PWM
        previousPWM = PWM;

        // take a very defined break (50 Hz) = 20 milliseconds between events
        // if nearly at the end of travel just reduce the delay to try to get the timing more accurate
        if (endTime - millis() < 20)
        {
            delay(20);
        }
        else
        {
            delay(10);
        }

        // check for the message to interupt early
        if (servos[pin].interuptEasing == true)
        {
            // jump out of the for loop
            break;
        }
    }

    // set starting position (to degree PWM mapped)
    PWM = toDegreeMapped;
    setServoPWM(pin, PWM);

    // Serial << "ServoEasing pin:" << pin << " loop completed in:" << millis() - startTime << "ms. Final PWM:" << PWM << endl;

    /* The task is going to be deleted. Set the handle to NULL. */
    servos[pin].taskHandle = NULL;

    // delete task
    vTaskDelete(NULL);
}
