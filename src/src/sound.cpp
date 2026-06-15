#include <Arduino.h>
#include "sound.h"

/*
Mode 1: Files in root directory: Played in create date order, file names do not matter
Mode 2: Files in /01…/99 directories: Uses THREE digit file names (001.mp3)
Mode 3: Files in /mp3 directory: Uses FOUR digit files (0001.mp3) names played in that order (this is what I needed)
*/

DFRobotDFPlayerMini sound;
TaskHandle_t SoundTask;

// Non-blocking communication delay threshold parameter
const uint32_t COMMAND_GUARD_PERIOD_MS = 50; 
static uint32_t lastCommandTimestamp = 0;

static void enforceNonBlockingGuard()
{
    uint32_t now = millis();
    uint32_t elapsed = now - lastCommandTimestamp;
    if (elapsed < COMMAND_GUARD_PERIOD_MS)
    {
        vTaskDelay(pdMS_TO_TICKS(COMMAND_GUARD_PERIOD_MS - elapsed));
    }
    lastCommandTimestamp = millis();
}

void sound_setup()
{
    pinMode(DFPLAYER_BUSY, INPUT); // Audio playing = LOW, Audio idle = HIGH

    xTaskCreatePinnedToCore(
        sound_task,          
        "Sound Task",        
        4096,                // Clean stable stack ceiling
        NULL,                
        sound_task_Priority, 
        &SoundTask, 
        1);      
}

void sound_task(void *pvParameters)
{
    Serial1.begin(9600, SERIAL_8N1, DFPLAYER_RX, DFPLAYER_TX);
    
    // Initialize DFPlayer interface setup parameters safely
    sound.begin(Serial1, true, true);
    sound.setTimeOut(750); 
    sound.outputDevice(DFPLAYER_DEVICE_SD);

    int previousBusyState = 1;

    for (;;)
    {
        messageParts parts;

        // Use short non-blocking check time limit to service status flags concurrently
        if (xQueueReceive(Sound_Queue, &parts, pdMS_TO_TICKS(30)) == pdTRUE)
        {
            std::string identifier = parts.identifier;

            if (identifier.compare("SVOL") == 0)
            {
                uint32_t volume = constrain(parts.value1, 0, 30);
                enforceNonBlockingGuard();
                sound.volume(volume);
            }
            else if (identifier.compare("SFILECOUNT") == 0)
            {
                enforceNonBlockingGuard();
                int fileCount = sound.readFileCounts();
                if (fileCount >= 0)
                {
                    sendToMicrobit("FILECOUNT:" + std::to_string(fileCount));
                }
            }
            else if (identifier.compare("SPLAY") == 0)
            {
                uint32_t folderNum = constrain(parts.value1, 1, 99);
                uint32_t trackNum = constrain(parts.value2, 1, 999);
                enforceNonBlockingGuard();
                sound.playFolder(folderNum, trackNum);
            }
            else if (identifier.compare("SPAUSE") == 0)
            {
                enforceNonBlockingGuard();
                sound.pause();
            }
            else if (identifier.compare("SRESUME") == 0)
            {
                enforceNonBlockingGuard();
                sound.start();
            }
            else if (identifier.compare("SSTOP") == 0)
            {
                enforceNonBlockingGuard();
                sound.stop();
            }
        }

        // Integrated High-Speed Busy Pin Monitoring Layer (Destroys legacy background task thread)
        int currentBusyState = digitalRead(DFPLAYER_BUSY);
        if (currentBusyState != previousBusyState)
        {
            // Low transition signal means unit commenced audio playback
            if (currentBusyState == LOW)
            {
                sendToMicrobit("SBUSY:1");
            }
            else // High logic transition signals file read completion tracking loop
            {
                sendToMicrobit("SBUSY:0");
            }
            previousBusyState = currentBusyState;
        }
    }

    vTaskDelete(NULL);
}
