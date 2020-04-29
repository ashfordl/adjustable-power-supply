/* 
 * Power Supply Firmware
 * Luke Ashford 2020
 * 
 * Written for Atmel ATmega328 running Arduino bootloader
 * 
 * Library dependencies:
 * LiquidCrystal_I2C (Arduino Package Manager)
 * 
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TimerOne.h>


// Set the LCD address
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Chip select pins
#define CSADC 9
#define CSDAC 10
#define ENABLE2675 7

// Rotary encoder pins
#define VSETA A0
#define VSETB A1

// LED pins
#define SHUTDOWN_LED 3
#define CURLIM_LED 4

// Output enable pin
#define OUTRESET_BTN 2

// Volatile variables used from within ISRs
volatile bool outputEnabled = false;

// Flags to handle application status changes
volatile bool outputEnabledChanged = false;

/*
 * Interrupt Service Routine for the front-panel output enable toggle button
 * 
 * resetButtonDepressedCount is a supporting variable for this function only
 */
volatile int resetButtonDepressedCount = 0;
void resetButtonISR()
{
  // Active low due to (external) pull-up resistor R2
  if (!digitalRead(OUTRESET_BTN))
  {
    resetButtonDepressedCount += 1;
  }
  else if (resetButtonDepressedCount > 0)
  {
    // Software debounce (essentially only polls once every 15ms)
    if (resetButtonDepressedCount > 3)
    {
      outputEnabled = !outputEnabled;
      outputEnabledChanged = true;
    }
    
    resetButtonDepressedCount = 0;
  }
}



/*
 * Master input service routine handling inputs from rotary encoders and the button
 */
void inputISR()
{
  resetButtonISR();
}

void setup()
{
  // Init LCD
  lcd.init();
  lcd.backlight();


  // Set pin modes and initial pin outputs
  pinMode(CSADC, OUTPUT);
  pinMode(CSDAC, OUTPUT);
  pinMode(ENABLE2675, OUTPUT);

  digitalWrite(CSADC, HIGH);
  digitalWrite(CSDAC, HIGH);
  // Begin with the LM2675 disabled
  digitalWrite(ENABLE2675, LOW);

  pinMode(VSETA, INPUT);
  pinMode(VSETB, INPUT);

  pinMode(SHUTDOWN_LED, OUTPUT);
  pinMode(CURLIM_LED, OUTPUT);
  digitalWrite(SHUTDOWN_LED, HIGH);
  digitalWrite(CURLIM_LED, LOW);

  pinMode(OUTRESET_BTN, INPUT_PULLUP);

  // 5000us = 5ms
  Timer1.initialize(5000);
  Timer1.attachInterrupt(inputISR);

}

void loop()
{
  if (outputEnabledChanged)
  {
    digitalWrite(SHUTDOWN_LED, !outputEnabled);
    outputEnabledChanged = false;
  }
}
