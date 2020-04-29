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
// Requires the LiquidCrystal_I2C library available from the Arduino IDE package manager
#include <LiquidCrystal_I2C.h>

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
  
  // Print a message to the LCD.
  lcd.setCursor(0,0);
  lcd.print("abcdefghijklmnop");
  lcd.setCursor(0,1);
  lcd.print("ABCdefghijklmnop");
}

void loop()
{
  // Testing the LCD
  
  delay(1000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("1000");
  delay(1000);
  lcd.clear();
}
