# Notes

This file contains notes about my process and thoughts during the project. 

# Materials

* Board
* Display
* Voltage regulator

# Power supply

Initially I wanted the coffee machine to power the arduino board. That turned out to be more difficult than expected. 

Pins 3 and 4 of the Mara X board are used for the serial communication. Pin 2 seemed to be ground and pin 1 seemed to supply 12V current. But the current drops  (to about 1.7V) as soon as the tiniest load is applied. 

**Pins:**
* 1 - VCC (12V)
* 2 - GND
* 3 - Receive (RX)
* 4 - Transmit (TX)

My next idea was to user a 9V block battery or a 18650 cell with a button and a self-latching circuit that turns itself off when unused for some time. 

Here is an example:  
https://forum.arduino.cc/t/vorstellung-und-erste-fragen-zur-selbstabschaltung-transistorschaltung/362576/16

In practice, it wasn't a good solution. 

Lastly, I've attached the coffee machine, the coffe grinder, and USB-power supply for the ardunio board on the same extension cord with switch. I consider this topic closed. 

# Buffer

At 9600 baud, each character arrives every ~1.04 ms (10 bits per char: 1 start, 8 data, 1 stop). Buffering and processing must keep pace to avoid overflow.

Buffer size should be 2–4× the max expected message length to handle bursts. For example, if messages are ≤100 chars, use a 200–400-byte buffer. The Mara X Board is sending 28 characters including the newline and 26 without. The buffer size should be at least 52 bytes and with an upper ceiling of 112 bytes. The median is at 82 bytes.

```Arduino
#define BUFFER_SIZE 256
char buffer[BUFFER_SIZE];
int buffer_index = 0;

void setup() {
  Serial.begin(9600);
}

void loop() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n') {
      buffer[buffer_index] = '\0'; // Null-terminate
      process_line(buffer);
      buffer_index = 0;
    } else if (buffer_index < BUFFER_SIZE - 1) {
      buffer[buffer_index++] = c;
    }
    // Else: drop data if buffer full (or implement overflow handling)
  }
}

void process_line(char* line) {
  Serial.print("Received: ");
  Serial.println(line);
}
```

# Modes

## 1 -> Steam preference

"In this mode, the machine works to provide steam availability 
during the brewing process. In this scenario, the temperature 
profle of the brewing process is less stable and precise over the 
use of the product. Nonetheless, the machine has higher steam 
performances. The HX Boiler works more intensively to balance the thermal 
fuctuation inside the boiler during the brewing process.

The heating element is highly active:
a.  During coffee brewing to maintain stable the thermal brewing profle
b.  After coffee brewing to generate enough steam
Best scenario of use: small coffce – fat white"

The value in this mode my coffee machine gave me was "C", which is 67 in the ASCII-table.

## 0 -> Coffee preference

"In this Xmode, the software turns off the heating element during 
coffee brewing, on the one hand, reducing the risk of overheating 
(typical problem of HX machines) and on the other hand, 
increasing the thermal stability in the HX."

The machine needs a restart to recognise the mode. "[...] if the led of the power button blinks, this means that the Xmode 
coffee is active."

The value in this mode my coffee machine gave me was "+", which is 43 in the ASCII-table.

# Mara X Data

My machine runs version 1.12. 

```Arduino
/*
    Example Data: C1.06,116,124,093,0840,1,0\n every ~400-500ms
    Length: 26
    [Pos] [Data] [Describtion]
    0)      C     Coffee Mode (C) or SteamMode (V)
    -        1.06  Software Version
    1)      116   current steam temperature (Celsisus)
    2)      124   target steam temperature (Celsisus)
    3)      093   current hx temperature (Celsisus)
    4)      0840  countdown for 'boost-mode'
    5)      1     heating element on or off
    6)      0     pump on or off
  */
```

# Architecture 

## Scheduler from Arduino IDE

```Arduino
#include <Scheduler.h>

void task1() {
  Serial.println("Task 1");
  delay(1000);  // Scheduler handles this non-blockingly
}

void task2() {
  Serial.println("Task 2");
  delay(500);
}

void setup() {
  Serial.begin(9600);
  Scheduler.startLoop(task1);
  Scheduler.startLoop(task2);
}

void loop() {
  yield();  // Required for Scheduler
}
```

## State Machine

```Arduino
enum SystemState { IDLE, TASK1_ACTIVE, TASK2_ACTIVE };
SystemState state = IDLE;
unsigned long task1Timer = 0;

void loop() {
  unsigned long currentMillis = millis();

  switch (state) {
    case IDLE:
      if (currentMillis % 2000 < 50) {  // Every 2 seconds
        state = TASK1_ACTIVE;
        task1Timer = currentMillis;
      }
      break;
    case TASK1_ACTIVE:
      if (currentMillis - task1Timer >= 1000) {
        Serial.println("Task 1 done");
        state = TASK2_ACTIVE;
      }
      break;
    case TASK2_ACTIVE:
      Serial.println("Task 2 running");
      state = IDLE;
      break;
  }
}
```



# Resources

* https://www.circuits-diy.com/12v-to-9v-converter-circuit-using-lm7809-regulator-ic/
* https://www.ebay.de/itm/375340664282
