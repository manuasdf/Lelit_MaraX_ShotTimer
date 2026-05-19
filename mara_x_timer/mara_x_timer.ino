//Includes
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>

//Defines
#define SCREEN_WIDTH    128 // Width in px 
#define SCREEN_HEIGHT   32 // Height in px
#define OLED_RESET      -1
#define SCREEN_ADDRESS  0x3C // or 0x3D Check datasheet or Oled Display
#define BUFFER_SIZE     82 // Median buffer size

//Pins
int RX = 5; // PIN 4 Mara TX to Arduino RX D5
int TX = 6; // PIN 3 Mara RX to Arduino TX D6

//Internals
long lastMillis = 0;
int seconds = 0;
int lastTimer = 0;
long serialTimeout = 0;
char buffer[BUFFER_SIZE];
int index = 0;

//Mara Data
String maraData[7];
int currentBoilerTemperature = 0;
int currentSteamTemperature = 0;
int targetSteamTemperature = 0;
int pumpState = 0;
char mode = "";
String version;

//Instances
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
SoftwareSerial MaraXSerial(RX, TX);

void setup()
{
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  display.clearDisplay();
  display.display();
  Serial.begin(9600);
  MaraXSerial.begin(9600);
  memset(buffer, 0, BUFFER_SIZE);
  delay(500);
  infoScreen();
}

void infoScreen() 
{
  readState();

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0,0);
  if (mode == 43) { // "+"
    display.print(F("COFFEE MODE"));
  } else if (mode == 67) { // "C"
    display.print(F("STEAM MODE"));
  } else {
    display.print(F("NO MODE"));
  }

  display.setCursor(100,0);
  display.print(version);

  display.setCursor(0,16);
  display.print(targetSteamTemperature);
  display.print(F(" STEAM TARGET TEMP"));

  display.display();

  delay(5000);
}

void readState()
{
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

  while (MaraXSerial.available())
  {
    serialTimeout = millis();
    char rcv = MaraXSerial.read();
    if (rcv != '\n')
      buffer[index++] = rcv;
    else {
      index = 0;
      Serial.println(buffer);
      char* ptr = strtok(buffer, ",");
      int idx = 0;
      while (ptr != NULL)
      {
        maraData[idx++] = String(ptr);
        ptr = strtok(NULL, ",");
      }
    }
  }
  if (millis() - serialTimeout > 6000)
  {
    serialTimeout = millis();
  }

  // Store values
  mode = maraData[0].charAt(0); // only first character
  version = maraData[0].substring(1); // start from second character
  currentSteamTemperature = maraData[1].toInt();
  targetSteamTemperature = maraData[2].toInt();
  currentBoilerTemperature = maraData[3].toInt();
  pumpState = maraData[6].toInt();
}

void updateView()
{

  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(String(currentBoilerTemperature).length() == 3 ? 0 : 12,0);
  display.println(currentBoilerTemperature);

  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(String(currentSteamTemperature).length() == 3 ? 0 : 12,16);
  display.println(currentSteamTemperature); 

  // display.setTextColor(SSD1306_BLACK,SSD1306_WHITE);
  display.setTextSize(3);
  display.setCursor(86,0);
  display.println(seconds);
  // display.setTextColor(SSD1306_BLACK,SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(86,24);
  display.println(String(lastTimer) + " SEC");


  display.setTextSize(1);

  display.setCursor(38,0);
  display.println(F("BOILR"));

  display.setCursor(38,16);
  display.println(F("STEAM"));

  display.display();
}

void loop()
{
  readState();

  if (pumpState) {
    lastMillis = millis();

    while (pumpState) {
      if (millis() - lastMillis >= 1000) {
        lastMillis = millis();
        ++seconds;
        if (seconds > 99)
          seconds = 0;
      }

      updateView();

      readState();
    }

    lastTimer = seconds;
    seconds = 0;
  }
  
  updateView();
}