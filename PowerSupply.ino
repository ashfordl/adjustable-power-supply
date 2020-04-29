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
#define ISETA A2
#define ISETB A3

// LED pins
#define SHUTDOWN_LED 3
#define CURLIM_LED 4

// Output enable pin
#define OUTRESET_BTN 2

// Volatile variables used from within ISRs
volatile bool outputEnabled = false;
volatile int vSetEncoderValue = 0;
volatile int iSetEncoderValue = 0;

// Flags to handle application status changes
volatile bool outputEnabledChanged = false;
volatile bool vSetEncoderChanged = false;
volatile bool iSetEncoderChanged = false;

/*
 * Interrupt service routine for the front-panel output enable toggle button
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
 * Interrupt service routine for the voltage-set rotary encoder
 * 
 * VSETA_last is a supporting variable used in this function only
 */
volatile int VSETA_last = HIGH;
void vSetEncoderISR()
{
  int A = digitalRead(VSETA);
  // if detected rising edge on A
  if ((VSETA_last == LOW) && (A == HIGH))
  {
    // VSETB == low is turning clockwise
    if (digitalRead(VSETB) == LOW)
    {
        vSetEncoderValue += 1;
    }
    // Turning counter clockwise
    else
    {
        vSetEncoderValue -= 1;
    }

    vSetEncoderChanged = true;;
  }

  VSETA_last = A;
}

/*
 * Interrupt service routine for the current limit-set rotary encoder
 * 
 * ISETA_last is a supporting variable used in this function only
 */
volatile int ISETA_last = HIGH;
void iSetEncoderISR()
{
  int A = digitalRead(ISETA);
  // if detected rising edge on A
  if ((ISETA_last == LOW) && (A == HIGH))
  {
    // ISETB == low is turning clockwise
    if (digitalRead(ISETB) == LOW)
    {
        iSetEncoderValue += 1;
    }
    // Turning counter clockwise
    else
    {
        iSetEncoderValue -= 1;
    }

    iSetEncoderChanged = true;;
  }

  ISETA_last = A;
}

/*
 * Master input service routine handling inputs from rotary encoders and the button
 */
void inputISR()
{
  resetButtonISR();
  vSetEncoderISR();
  iSetEncoderISR();
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
  pinMode(ISETA, INPUT);
  pinMode(ISETB, INPUT);

  pinMode(SHUTDOWN_LED, OUTPUT);
  pinMode(CURLIM_LED, OUTPUT);
  digitalWrite(SHUTDOWN_LED, HIGH);
  digitalWrite(CURLIM_LED, LOW);

  pinMode(OUTRESET_BTN, INPUT_PULLUP);

  // 5000us = 5ms
  Timer1.initialize(5000);
  Timer1.attachInterrupt(inputISR);

}

void updateLCD()
{
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(vSetEncoderValue);
  
  lcd.setCursor(0,1);
  lcd.print(iSetEncoderValue);
}

void loop()
{
  if (outputEnabledChanged)
  {
    digitalWrite(SHUTDOWN_LED, !outputEnabled);
    outputEnabledChanged = false;
  }

  if (vSetEncoderChanged)
  {
    updateLCD();
    vSetEncoderChanged = false;
  }

  if (iSetEncoderChanged)
  {
    updateLCD();
    iSetEncoderChanged = false;
  }
}
