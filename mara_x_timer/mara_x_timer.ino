//Includes
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>

//Defines
#define SCREEN_WIDTH    128 // Width in px 
#define SCREEN_HEIGHT   32 // Height in px
#define OLED_RESET      -1
#define SCREEN_ADDRESS  0x3C // or 0x3D Check datasheet or Oled Display
#define BUFFER_SIZE     84 // Three times packet size (incl. \n)
#define DEBUG           false // Get serial output for diagnostics

//Pins
int RX = 5; // PIN 4 Mara TX to Arduino RX D5
int TX = 6; // PIN 3 Mara RX to Arduino TX D6

//Internals
unsigned long lastTimerMillis = 0;
unsigned int seconds = 0;
unsigned int lastTimer = 0;
unsigned int shotsProduced = 0;
unsigned long serialTimeout = 0;
unsigned long infoScreenTimeout = 0;
char buffer[BUFFER_SIZE];
int index = 0;

//Mara Data
String maraData[7];
unsigned int currentBoilerTemperature = 0;
unsigned int currentSteamTemperature = 0;
unsigned int targetSteamTemperature = 0;
int pumpState = 0;
int boilerState = 0;
unsigned char mode = NULL;
String version;

//Error states
const unsigned long PACKET_TIMEOUT = 2000;
const unsigned long DISPLAY_INTERVAL = 100;

bool stateValid = false;
bool rxOverflow = false;

unsigned long lastPacketMillis = 0;
unsigned long lastDisplayMillis = 0;
unsigned long lastDebugMillis = 0;

unsigned long malformedPackets = 0;
unsigned long overflowPackets = 0;
unsigned long staleEvents = 0;

//Instances
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
SoftwareSerial MaraXSerial(RX, TX);

void setup()
{
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  display.clearDisplay();
  display.display();
  delay(1000); // Wait for Mara X
  Serial.begin(9600);
  MaraXSerial.begin(9600);
  memset(buffer, 0, BUFFER_SIZE);
  infoScreenTimeout = millis();
  lastDisplayMillis = millis();
}

void invalidateState()
{
  stateValid = false;

  // Safe values when communication is lost.
  pumpState = 0;
  boilerState = 0;

  currentBoilerTemperature = 0;
  currentSteamTemperature = 0;
  targetSteamTemperature = 0;

  mode = 0;
  version = "";

  seconds = 0;
}

bool isCommunicationStale()
{
  return !stateValid ||
    millis() - lastPacketMillis >= PACKET_TIMEOUT;
}

void checkForStaleState()
{
  if (stateValid &&
    millis() - lastPacketMillis >= PACKET_TIMEOUT) {
    staleEvents++;
    invalidateState();
  }
}

bool isUnsignedNumber(const char *text)
{
  if (text == NULL || *text == '\0') {
    return false;
  }

  while (*text) {
    if (*text < '0' || *text > '9') {
      return false;
    }

    text++;
  }

  return true;
}

bool readState()
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

  bool validPacketReceived = false;
  //bool packetReceived = false;

  while (MaraXSerial.available())
  {
    char rcv = MaraXSerial.read();

    if (rcv == '\r') {
      continue;
    }

    // After an overflow, discard everything until the next line.
    if (rxOverflow) {
      if (rcv == '\n') {
        rxOverflow = false;
        index = 0;
        overflowPackets++;
      }

      continue;
    }

    if (rcv != '\n') {
      if (index < BUFFER_SIZE - 1) { // Prevent buffer overflow
        buffer[index++] = rcv;
      } else {
        // Packet was too long. Discard it.
        index = 0;
        rxOverflow = true;
      }

      continue;
    }

    // Empty line.
    if (index == 0) {
      continue;
    }

    buffer[index] = '\0';
    index = 0;

    char *fields[7];
    int fieldCount = 0;

    char *ptr = strtok(buffer, ",");

    while (ptr != NULL && fieldCount < 7)
    {
      fields[fieldCount++] = ptr;
      ptr = strtok(NULL, ",");
    }

    // Reject malformed packets.
    if (fieldCount != 7 || ptr != NULL) {
      malformedPackets++;
      continue;
    }

    // Fields 1, 2, 3, 4, 5, and 6 must be numeric.
    bool fieldsValid = true;

    for (int i = 1; i < 7; i++) {
      if (!isUnsignedNumber(fields[i])) {
        fieldsValid = false;
        break;
      }
    }

    if (!fieldsValid) {
      malformedPackets++;
      continue;
    }

    // Range checks.
    int newPumpState = atoi(fields[6]);
    int newBoilerState = atoi(fields[5]);

    if ((newPumpState != 0 && newPumpState != 1) ||
      (newBoilerState != 0 && newBoilerState != 1)) {
      malformedPackets++;
      continue;
    }

    // if (maraData[0].length() < 2) {
    //   continue;
    // }

    // After tests have past, modify live state
    mode = fields[0][0];
    version = String(fields[0] + 1);

    currentSteamTemperature = atoi(fields[1]);
    targetSteamTemperature = atoi(fields[2]);
    currentBoilerTemperature = atoi(fields[3]);

    boilerState = newBoilerState;
    pumpState = newPumpState;

    stateValid = true;
    lastPacketMillis = millis();
    validPacketReceived = true;
  }

  return validPacketReceived;
}

void updateView()
{
  unsigned long now = millis();

  if (now - lastDisplayMillis < DISPLAY_INTERVAL) {
    return;
  }

  lastDisplayMillis = now;

  display.clearDisplay();

  if (stateValid) {
    displayTemplate_04();
  } else {
    displayErrorScreen();
  }

  display.display();
}

void displayErrorScreen() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(F("MARA SERIAL ERROR"));

  display.setCursor(0, 12);
  display.print(F("Bad: "));
  display.print(malformedPackets);

  display.setCursor(0, 24);
  display.print(F("Lost: "));
  display.print(staleEvents);

  display.setCursor(70, 24);
  display.print(F("OVF:"));
  display.print(overflowPackets);
}

void printDiagnostics()
{
  if (!DEBUG && millis() - lastDebugMillis < 5000) {
    return;
  }

  lastDebugMillis = millis();

  Serial.print(F("valid="));
  Serial.print(stateValid);

  Serial.print(F(" age="));
  if (stateValid) {
    Serial.print(millis() - lastPacketMillis);
  } else {
    Serial.print(F("invalid"));
  }

  Serial.print(F(" malformed="));
  Serial.print(malformedPackets);

  Serial.print(F(" overflow="));
  Serial.print(overflowPackets);

  Serial.print(F(" stale="));
  Serial.println(staleEvents);
}

void displayInfoScreen()
{
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
}

void displayTemplate_01() {
  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(String(currentBoilerTemperature).length() == 3 ? 0 : 12,0);
  display.println(currentBoilerTemperature);

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(String(currentSteamTemperature).length() == 3 ? 0 : 12,16);
  display.println(currentSteamTemperature); 

  display.setTextSize(1);

  display.setCursor(38,0);
  display.println(F("BOILR"));

  display.setCursor(38,16);
  display.println(F("STEAM"));

  display.setTextSize(3);
  display.setCursor(86,0);
  display.println(seconds);
  display.setTextSize(1);
  display.setCursor(86,24);
  display.println(String(lastTimer) + " SEC");
}

void displayTemplate_02() {
  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(String(currentBoilerTemperature).length() == 3 ? 0 : 12,0);
  display.println(currentBoilerTemperature);

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(String(currentSteamTemperature).length() == 3 ? 0 : 12,16);
  display.print(currentSteamTemperature); 
  display.print("/");
  display.print(targetSteamTemperature); 

  display.setTextSize(1);

  display.setCursor(38,0);
  display.print(F("BOILER"));

  if (boilerState) {
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.setCursor(38,8);
    display.print(F("  on  "));
    display.setTextColor(SSD1306_WHITE);
  } else {
    display.setCursor(38,8);
    display.print(F(" off"));
  }

  display.setTextSize(3);
  display.setCursor(86,0);
  display.println(seconds);
  display.setTextSize(1);
  display.setCursor(86,24);
  display.println(String(lastTimer) + " SEC");
}

void displayTemplate_03() {
  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(String(seconds).length() == 2 ? 12 : 24,0);
  display.print(seconds);
  display.print("/");
  display.print(lastTimer); 

  display.setCursor(String(currentSteamTemperature).length() == 3 ? 0 : 12,16);
  display.print(currentSteamTemperature); 
  display.print("/");
  display.print(targetSteamTemperature); 

  display.setTextSize(1);
  display.setCursor(110,0);
  display.println(F("BLR"));
  display.setCursor(110,16);
  display.println(currentBoilerTemperature);
  display.setCursor(110,8);

  if (boilerState) {
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.print(F("ON"));
    display.setTextColor(SSD1306_WHITE);
  } else {
    display.print(F("OFF"));
  }

  display.setCursor(110,24);
  if (mode == 43) { // "+"
    display.print(F("CFF"));
  } else if (mode == 67) { // "C"
    display.print(F("STM"));
  } else {
    display.print(F("NoM"));
  }
}

void displayTemplate_04() {
  display.setCursor(0, 0);
  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(String(seconds).length() == 2 ? 12 : 24, 0);
  display.print(seconds);
  display.print("/");
  display.print(lastTimer); 

  display.setCursor(String(currentSteamTemperature).length() == 3 ? 0 : 12, 16);
  display.print(currentSteamTemperature); 
  display.print("/");
  display.print(targetSteamTemperature); 

  display.setCursor(100,0);
  display.println(shotsProduced);

  display.setCursor(currentBoilerTemperature >= 100 ? 90 : 100, 16);
  display.println(currentBoilerTemperature);
}

void loop()
{
  readState();
  checkForStaleState();
  printDiagnostics();

  if (stateValid && pumpState) {
    if (millis() - lastTimerMillis >= 1000) {
      lastTimerMillis = millis();

      if (seconds < 99) {
        seconds++;
      } else {
        seconds = 0;
      }
    }
  } else {
    if (seconds > 9) {
      shotsProduced++;
      lastTimer = seconds;
    }

    seconds = 0;
  }

  updateView();
}
