/* 
 * Power Supply Firmware
 * Luke Ashford 2020
 * 
 * Written for Atmel ATmega328 running Arduino bootloader
 * 
 * Library dependencies (source):
 * LiquidCrystal_I2C v1.1.2 (Arduino IDE Library Manager)
 * TimerOne v1.1.0  (Arduino IDE Library Manager)
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TimerOne.h>
#include <SPI.h>

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

/*
 * Writes the specified value to the DAC
 *
 * This design uses the MCP4822 DAC
 * VOUTA goes to the switching regulator LM2675/U1
 * VOUTB goes to the linear regulator LM317/U5 through opamp LMC6482/U4B
 *
 * INPUTS
 * channel - 0 or 1 corresponding to the DAC channel (0 = VOUTA, 1 = VOUTB)
 * gainEnable - true or false corresponding to whether to enable the optional 2x hardware gain
 * channelEnable - true or false corresponding to whether to enable this output channel
 * value - int from 0 to 4095 correspondin to the value to be written to the DAC
 */
void writeDac(int channel, boolean gainEnable, bool channelEnable, int value)
{
    byte b = 0;
    // Channel select (0 = output A, 1 = output B)
    b |= ((boolean)(channel)) << 7;
    // Output gain select (0 = 2x gain, 1 = 1x gain)
    b |= (!gainEnable) << 5;
    // Output shutdown control bit (0 = output disabled, 1 = output enabled)
    b |= (channelEnable) << 4;
    // 12-bit value, MSB first
    b |= ((value & 4095) >> 8);

    // Enable SPI communcation
    digitalWrite(CSDAC, LOW);
    // Transfer first byte
    SPI.transfer(b);
    // Transfer 8 LSB of value
    SPI.transfer((byte)(value & 255));
    // Disable SPI communication
    digitalWrite(CSDAC, HIGH);
}

/*
 * Reads the current value on the specified ADC channel
 *
 * This design uses an MCP3204 with external 4.096V reference
 *
 * INPUTS
 * channel - 0, 1, 2, 3 corresponding to the equivalent channel on the ADC
 *
 * OUTPUT
 * int value between 0 and 4095, the ADC's reading in millivolts 0 - 4.095V
 */
int readAdc(int channel)
{
    byte b0 = 0;
    // Start bit
    b0 |= 1 << 2;
    // Single-ended readings not pseudo-differential pairs
    b0 |= 1 << 1;

    byte b1 = 0;
    /* 
     * b1 looks like
     * ab000000
     * where ab is the binary number corresponding to the channel to read
     * (cf communication protocol detailed below)
     */
    b1 |= channel << 6;

    /* 
     * Communication protocol (MCP3204):
     *
     *      |---------------b0--------------|---------------------b1--------------------|
     * Bit# | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |  7   |  6   | 5 | 4 |  3  |  2  | 1  | 0  |
     * SEND | 0 | 0 | 0 | 0 | 0 | 1 | 1 | 0 | b1_7 | b1_6 | 0 | 0 |  0  |  0  | 0  | 0  |
     * READ | x | x | x | x | x | x | x | x |  x   |  x   | x | x | B11 | B10 | B9 | B8 |
     * 
     *      |-------------------b2------------------|
     * Bit# | 7  | 6  | 5  | 4  | 3  | 2  | 1  | 0  |
     * SEND | 0  | 0  | 0  | 0  | 0  | 0  | 0  | 0  |
     * READ | B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
     *
     * Bit#2 of b0 is the start bit
     * Bit#1 of b0 selects single-ended readings not differential pairs
     * Bit#7-6 of b1 select the channel
     * Bit#5 of b1 is when the ADC samples
     * Bit#4 of b1 is the null bit specified in the datasheet
     * B11-0 are the output bits, transmitted MSB first
     */


    int output = 0;

    // Enable SPI communication
    digitalWrite(CSADC, LOW);
    // Transfer first byte
    SPI.transfer(b0);
    // Transfer second byte to select channel, then read output and shift appropriately
    output = SPI.transfer(b1) << 8;
    // Read remainder of output
    output |= SPI.transfer(0);
    // Disable SPI communication
    digitalWrite(CSADC, HIGH);

    // AND with 4095 to kill any output from the null bit
    return output & 4095;
}

/*
 * Calculates and returns the required output on DAC1 to give the actual target
 * output at the terminals
 *
 * FORMULA
 * 4530 - ((target output) * 0.437)
 * This is hardcoded so as to repeat calculations in the MCU. See derivation below.
 *
 * DERIVATION
 * R3 = 1000 ohms
 * R5 = 3480 ohms
 * R7 = 7970 ohms
 *
 * We otherwise work below in millivolts and milliamps
 *
 * V_{FB} = 1210 mV
 *
 * The LM2675 tries to hold pin 4 (FEEDBACK) at 1210 mV, we use Kirchhoff's Current Law
 * at this pin
 * I_{R3} = 1.21 mA
 * I_{R7} = (LM2675 Output - V_{FB}) / R7 mA
 * I_{R5} = (V_{DAC1} - V_{FB}) / R5 mA
 *
 * To allow for the linear regulator and current sense resistor network, we aim for LM2675
 * output to be target output + 3250 mV
 *
 * KCL  => I_{R5} = I_{R3} - I_{R7}
 *      => V_{DAC} - 1210 = 3480 * ( 1.21 - output/7970 + 1210/7970 )
 * To a good approximation, rounding appropriately, we have that
 * V_{DAC} = 4530 - (target output * 0.437)
 *
 * INPUTS
 * target - integer quantity of millivolts to aim for at the terminals
 *
 * OUTPUT
 * int value to write to DAC1 to obtain the target output at the terminals
 */
int calculateDAC1Output(int target)
{
    return 4530 - (int)(target * (0.437f));
}

/*
 * Calculates and returns the required output on DAC2 to give the actual target
 * output at the terminals
 *
 * FORMULA
 * ((target output) - (linear dropout)) / (opamp gain U4B)
 *
 * The LM317 voltage reference is 1.25V and the op-amp gain is 1 + 100/82 = 2.21 (measured
 * in circuit)
 * 
 * INPUTS
 * target - integer quantity of millivolts to aim for at the terminals
 *
 * OUTPUT
 * int value to write to DAC2 to obtain the target output at the terminals
 */
int calculateDAC2Output(int target)
{
    return (int)((float)(target - 1250) / 2.21f);
}

/*
 * Returns the actual millivolt output at the terminals
 *
 * Reads the value of ADC_IN2 (in hardware) / ADC channel 1 (software) and adjusts to 
 * compensate for the resistor divider R34 & R35
 *
 * OUTPUT
 * int value corresponding to the actual millivolt output at the terminals
 */
int calculateADC2Input()
{
    int adc_voltage = readAdc(1);
    return (int)(adc_voltage * 2.65f);
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

    // Disable SPI chips
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

    // Init SPI
    SPI.setClockDivider(SPI_CLOCK_DIV4); // slow the SPI bus down
    SPI.setBitOrder(MSBFIRST);  // Most significant bit arrives first
    SPI.setDataMode(SPI_MODE0);    // SPI 0,0 as per MCP4821/2 data sheet
    SPI.begin();

    // 5000us = 5ms
    Timer1.initialize(5000);
    Timer1.attachInterrupt(inputISR);

    // TEST
    // Write 1V to output A of DAC
    digitalWrite(ENABLE2675, HIGH);
    writeDac(0, true, true, calculateDAC1Output(7500));
    // Write 3.3V to output A of DAC
    writeDac(1, true, true, calculateDAC2Output(7500));
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
        // updateLCD();
        vSetEncoderChanged = false;
    }

    if (iSetEncoderChanged)
    {
        // updateLCD();
        iSetEncoderChanged = false;
    }

    lcd.clear();
    lcd.setCursor(0, 0);
    // Read from ADC_IN1 = ADC's channel 0
    lcd.print(readAdc(0));
    lcd.setCursor(0, 1);
    // Read from ADC_IN2 = ADC's channel 1
    lcd.print(calculateADC2Input());


    delay(1000);
}
