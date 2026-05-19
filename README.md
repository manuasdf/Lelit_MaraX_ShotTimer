# Mara X Shot Timer
An arduino based shot timer for the Lelit Mara X prosumer espressor machine. 

## Existing projects

1. https://github.com/SaibotFlow/marax-monitor (The pictures about the wiring are very helpful)
1. https://github.com/DiSanzes/Mara-X2-ShotTimer (forked the first)
1. https://github.com/snafuz/MaraXDisplay (forked the second)
      
## Objective 1

The project runs on most Arduino boards or ATmega chips. 

Reasoning: Most existing projects use ESP32 or better with WiFi capabilities which none of the projects need nor use. ATmegas that support two serial I/Os at the same time, get the job done. 

## Objective 2

The project uses, only when necessary, currently maintained dependencies. 

Reasoning: For the longest time, most of the listed projects were using a deprecated library. The aim is to handcode as much as possible to make the project run on smaller hardware as well. Without any dependencies will most likely not be possible since libraries for the display and the software serial are needed. 

## Pseudocode

```
  // read state
  // if state is pump==true
    // save time-stamp
    // while pump==true
      // read time stamp
      // refresh display
      // read state
    // update last time
  // read state
  // update display
```

## Compatibility

Adafruit SSD1306 for the display: https://github.com/adafruit/Adafruit_SSD1306  
Comptaible with most ATmega and ESP* but not ATtiny85.

## Hardware

For this project I used:
* Ardunio Nano
* Monochrome OLEDs display based on SSD1306 driver, 128 x 32 pixel, using I2C for communication
