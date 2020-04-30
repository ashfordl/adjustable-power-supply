/* 
 * Adjustable Power Supply
 * Luke Ashford 2016
 */

#include <SPI.h>
#include <Wire.h>
// #include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <TimerOne.h>

// Chip Select Pins
#define CSADC 9
#define CSDAC 10
#define ENABLE2675 7

// Rotary Encoders
#define VSETA A0
#define VSETB A1

// I2C LCD pins
#define I2C_ADDR    0x27
#define BACKLIGHT_PIN     3
#define En_pin  2
#define Rw_pin  1
#define Rs_pin  0
#define D4_pin  4
#define D5_pin  5
#define D6_pin  6
#define D7_pin  7
LiquidCrystal_I2C lcd(I2C_ADDR,En_pin,Rw_pin,Rs_pin,D4_pin,D5_pin,D6_pin,D7_pin);

// LEDs
#define SHUTDOWN_LED 3
#define CURLIM_LED 4

#define OUTRESET_BTN 2

volatile int targetMillivolts = 5000;

void setPinModes()
{
    pinMode(CSADC, OUTPUT);
    pinMode(CSDAC, OUTPUT);
    pinMode(ENABLE2675, OUTPUT);
    digitalWrite(CSADC, HIGH);
    digitalWrite(CSDAC, HIGH);
    digitalWrite(ENABLE2675, LOW);

    pinMode(VSETA, INPUT);
    pinMode(VSETB, INPUT);

    pinMode(SHUTDOWN_LED, OUTPUT);
    pinMode(CURLIM_LED, OUTPUT);
    digitalWrite(SHUTDOWN_LED, HIGH);
    digitalWrite(CURLIM_LED, LOW);

    pinMode(OUTRESET_BTN, INPUT_PULLUP);
}

void initLCD()
{
    lcd.begin(16,2);
    lcd.setBacklightPin(BACKLIGHT_PIN,POSITIVE);
    lcd.setBacklight(HIGH);
    lcd.home();
}

void initSPI()
{
    SPI.setClockDivider(SPI_CLOCK_DIV4); // slow the SPI bus down
    SPI.setBitOrder(MSBFIRST);  // Most significant bit arrives first
    SPI.setDataMode(SPI_MODE0);    // SPI 0,0 as per MCP4821/2 data sheet
    SPI.begin();
}

void initTimerOne()
{
    Timer1.initialize(5000);
    Timer1.attachInterrupt(inputISR);
}


boolean outputEnabled = 0;
void enableOutput()
{
    outputEnabled = 1;
  
    digitalWrite(ENABLE2675, HIGH);
    digitalWrite(SHUTDOWN_LED, LOW);

    writeDac(0, 1, generateChannel0Output(targetMillivolts));
    writeDac(1, 1, generateChannel1Output(targetMillivolts));
}

void disableOutput()
{
    outputEnabled = 0;
  
    digitalWrite(ENABLE2675, LOW);
    digitalWrite(SHUTDOWN_LED, HIGH);

    writeDac(0, 1, generateChannel0Output(1250));
    writeDac(1, 1, generateChannel1Output(1250));
}

/*
 * channel - 0, 1, 2 or 3
 * returns: int value 0 up to & including 4095
 */
int readAdc(int channel)
{
    byte b0 = 0;
    b0 |= 1 << 2; // Start bit
    b0 |= 1 << 1; // Single-ended

    byte b1 = 0;
    b1 |= (channel >= 2) << 7;
    b1 |= (channel % 2) << 6;

    int output = 0;

    digitalWrite(CSADC, LOW);
    SPI.transfer(b0);
    output = SPI.transfer(b1) << 8;
    output |= SPI.transfer(0);
    digitalWrite(CSADC, HIGH);

    return output & 4095;
}

/*
 * channel - 0 or 1; gainEnable - 1 to enable x2 gain; value - 0 up to & including 4095
 */
void writeDac(int channel, boolean gainEnable, int value)
{
    byte b = 0;
    b |= ((boolean)(channel)) << 7;
    b |= !gainEnable << 5;
    b |= 1 << 4;
    b |= ((value & 4095) >> 8);

    digitalWrite(CSDAC, LOW);
    SPI.transfer(b);
    SPI.transfer((byte)(value & 255));
    digitalWrite(CSDAC, HIGH);
}

/*
 * Generates the required input value for the 2675 reg in order to produce the desired target output
 * targetmV - 1250 up to & including 10 000
 * returns: int value 0 up to & including 4095
 */
int generateChannel0Output(int targetmV)
{
    float mv = (float)targetmV;
    return (int)(5421.0 - (348.0/797.0 * (mv + 2040.0)));
}

/*
 * Generates the required input value for the 317 reg in order to produce the desired target output
 * targetmV - 1250 up to & including 10 000
 * returns: int value 0 up to & including 4095
 */
int generateChannel1Output(int targetmV)
{
    float mv = (float)targetmV;
    return (int)(10.0 * ((mv - 1249.0) / 22.0));
}

/*
 * ISR handler for the output enable toggle and rotary encoders
 */
volatile byte LCDWriteTimer = 0;
volatile boolean needToWriteLCD = 0;
void inputISR()
{
    resetButtonISR();
    vSetEncISR();
    if (LCDWriteTimer++ == 0 && !needToWriteLCD)
    {
        needToWriteLCD = 1;
    }
}

/*
 * Interrupt Service Routine for the front-panel output enable toggle button
 */
volatile int resetButtonDepressedCount = 0;
void resetButtonISR()
{
    if (!digitalRead(OUTRESET_BTN))
    {
        resetButtonDepressedCount += 1;
    }
    else if (resetButtonDepressedCount > 0)
    {
        // Software debounce
        if (resetButtonDepressedCount > 10)
        {
            if (outputEnabled)
            {
                disableOutput();
            }
            else 
            {
                enableOutput();
            }
        }
        
        resetButtonDepressedCount = 0;
    }
}

/*
 * Interrupt Service Routine for the voltage-set rotary encoder
 */
int VSETA_last = HIGH;
int Vcount = targetMillivolts/100;
void vSetEncISR()
{
    int A = digitalRead(VSETA);
    // L->H on A
    if ((VSETA_last == LOW) && (A == HIGH))
    {
        if (digitalRead(VSETB) == LOW)
        {
            Vcount += 1;
            if (Vcount > 100)
            {
                Vcount = 100;
            }
        }
        else
        {
            Vcount -= 1;
            if (Vcount < 13)
            {
                Vcount = 13;
            }
        }

        targetMillivolts = Vcount*100;
    }

    VSETA_last = A;
}

void updateLCD()
{
    int ch0 = readAdc(0);
    int ch1 = readAdc(1);
    float c1 = ch1;
    c1 /= 0.376564;
    c1 += 0.5;
    int mvToWrite = targetMillivolts + (targetMillivolts - (int)c1);

    lcd.clear();
    lcd.home();
    lcd.print(targetMillivolts/100);
    lcd.print(" ");
    lcd.print(mvToWrite);
    lcd.print(" ");
    lcd.print(generateChannel1Output(mvToWrite));
    lcd.setCursor(0, 1);
    lcd.print(ch0);
    lcd.print(" ");
    lcd.print(ch1);
    lcd.print(" ");
    lcd.print((int)c1);
}

void setup() 
{
    setPinModes();
    initSPI();
    initLCD();
    initTimerOne();

    lcd.clear();
    lcd.home();
    lcd.print("Hello world");
}

void loop() 
{
    if (outputEnabled)
    {
        float c1 = (float)readAdc(1);
        c1 /= 0.376564;
        c1 += 0.5;
        int mvToWrite = targetMillivolts + (targetMillivolts - (int)c1);
      
        writeDac(0, 1, generateChannel0Output(mvToWrite));
        writeDac(1, 1, generateChannel1Output(mvToWrite));
    }

    if (needToWriteLCD)
    {
        updateLCD();
        needToWriteLCD = 0;
    }
}
