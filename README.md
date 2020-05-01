# Power Supply

This repository provides firmware for a custom-built bench power supply which I designed. The firmware targets an ATMega328P running the Arduino bootloader, and using Arduino libraries. 

## Features
Many of these are a work-in-progress
- 1.3 - 10V output, configurable at 0.1V intervals
- Adjustable current limit 0 - 1A, 0.01A intervals
- Toggle-able output

## Deployment
The Arduino IDE is required. First install the required libraries (the code should fail to compile if this has not been done) from the Arduino IDE's library manager. Then upload the code to the ATMega328P to be used. If this chip is in-circuit, then an ISP programmer such as a USBTinyISP may be used. In this instance the board should be powered separately; that is, the jumper on the USBTinyISP should be disconnected.