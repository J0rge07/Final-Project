# Final Project - CSCI 330

## Group Name
The Interruptors

## Overview
This project is a multi-device alarm system using RP2350 microcontrollers. The system detects motion using an IMU sensor and triggers a distributed response across multiple devices using Bluetooth communication.

When motion is detected, the system records a timestamp using a real-time clock (RTC) and sends an alarm message to a secondary device. The receiving device activates visual and audio alerts including LEDs and a speaker system.

## Features
- IMU-based motion detection using interrupts
- Real-time clock (RTC) timestamp logging
- Bluetooth communication between devices
- LED alert system (NeoPixel)
- Audio alarm output (speaker/buzzer)

## System Design
1. Sensor device detects motion via IMU interrupt
2. RTC records exact time of event
3. Message is sent over Bluetooth
4. Receiver device processes message:
   - activates LEDs
   - triggers audio alarm
   - displays/logs timestamp

## Technologies Used
- RP2350 microcontrollers
- C++ (PlatformIO)
- Bluetooth communication
- RTC module
- IMU sensor
- NeoPixel LEDs
- I2S audio output
