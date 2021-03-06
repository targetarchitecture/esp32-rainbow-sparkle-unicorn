#include <Arduino.h>
#include <Wire.h>
#include "soc/rtc_wdt.h"
#include "Adafruit_MPR121.h"
#include "DFRobotDFPlayerMini.h"
#include "SN7 pins.h"

Adafruit_MPR121 touch = Adafruit_MPR121();
uint16_t lasttouched = 0;
uint16_t currtouched = 0;
DFRobotDFPlayerMini sound;

void touched();
void IRAM_ATTR buttonreleased();
void IRAM_ATTR buttonpressed();

void setup()
{
  Wire.begin(SDA, SCL); //I2C bus
  Serial.begin(115200); //ESP32 USB Port

  pinMode(TOUCH_INT, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(TOUCH_INT), buttonpressed, FALLING);
  //attachInterrupt(digitalPinToInterrupt(TOUCH_INT), buttonreleased, LOW);

  // Configure LED output
  pinMode(ONBOARDLED, OUTPUT);

  //Configure serial port pins and busy pin
  pinMode(DFPLAYER_BUSY, INPUT);
  Serial1.begin(9600, SERIAL_8N1, DFPLAYER_RX, DFPLAYER_TX);

  touch.begin(0x5A);

  if (!sound.begin(Serial1, true, true))
  {
    Serial.println("DFPlayer error");

    delay(5000);
    ESP.restart();
  }

  Serial.print("DFPlayer Busy:");
  Serial.println(digitalRead(DFPLAYER_BUSY));

  delay(30);
  sound.setTimeOut(750); //Set serial communication time out 750ms
  delay(30);

  sound.outputDevice(DFPLAYER_DEVICE_SD);

  delay(30);

  int fileCounts = sound.readFileCounts();

  if (fileCounts == -1)
  {
    Serial.println("DFPlayer no files found");

    delay(1000);
    ESP.restart();
  }
  else
  {
    Serial.print("Found ");
    Serial.print(fileCounts);
    Serial.println(" files");

    sound.volume(10); //30
    Serial.print("Volume: ");
    Serial.println(sound.readVolume());
  }
}

void loop()
{
  // touched();

  // delay(50);
}

void touched()
{
  rtc_wdt_feed();

  // Get the currently touched pads
  currtouched = touch.touched();

  rtc_wdt_feed();

  for (uint8_t pin = 0; pin < 12; pin++)
  {
    rtc_wdt_feed();

    // it if *is* touched and *wasnt* touched before, alert!
    if ((currtouched & _BV(pin)) && !(lasttouched & _BV(pin)))
    {
      Serial.print("Touched pin ");
      Serial.println(pin);

      //play the mp3
      sound.play(pin + 1);
      delay(30);
    }
    // if it *was* touched and now *isnt*, alert!
    if (!(currtouched & _BV(pin)) && (lasttouched & _BV(pin)))
    {
      Serial.print("Released pin ");
      Serial.println(pin);
    }
  }

  // reset our state
  lasttouched = currtouched;
}

// The function is placed in the RAM of the ESP32.
void IRAM_ATTR buttonpressed()
{

  Serial.println(digitalRead(TOUCH_INT));

  // Serial.println("buttonpressed");
  digitalWrite(ONBOARDLED, HIGH);
  touched();
}

void IRAM_ATTR buttonreleased()
{
  Serial.println("buttonreleased");
  digitalWrite(ONBOARDLED, LOW);
}