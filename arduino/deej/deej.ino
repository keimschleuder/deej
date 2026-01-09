#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// TFT display pins
#define TFT_CS    4
#define TFT_DC    2
#define TFT_RST   3

// Image dimensions (must match Go program)
#define IMAGE_WIDTH  100
#define IMAGE_HEIGHT 100

// Position to display image
#define IMAGE_X 30
#define IMAGE_Y 0

// Create display object
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

const long imageRequestInterval = 10000;

// Buffer for one line of RGB565 data (100 pixels * 2 bytes)
uint8_t lineBuffer[IMAGE_WIDTH * 2];

// Communication state
enum State {
  IDLE,
  WAITING_FOR_HEADER,
  READING_SIZE,
  RECEIVING_IMAGE,
  RECEIVING_DATA
};

enum ScreenState {
  IDLE_SCREEN, 
  PERCENTAGE, 
  SPECIAL
};

State currentIMGState = IDLE;
ScreenState currentScreenState = IDLE_SCREEN;
uint32_t imageSize = 0;
uint32_t pixelsReceived = 0;
uint16_t currentLine = 0;
bool imageOnScreen = false;
unsigned long lastImageRequest = 0;
unsigned long lastAction = 0;

// Sliders
const int NUM_SLIDERS = 1;
const int NUM_RELAIS = 2;
const int analogInputs[NUM_SLIDERS] = { A0 };
const int sliderOutputs[NUM_RELAIS] = { 9, 8 };

uint8_t percentSliderValues[NUM_SLIDERS];
uint8_t lastSliderValues[NUM_SLIDERS];
uint8_t secondLastSliderValues[NUM_SLIDERS];
bool sliderActive[NUM_SLIDERS];
int lastSliderActive;

const char* sliderNames[] = {"Master Volume", "Aktuelles Fenter", "Discord", "Musik", "Alles andere", "Mikrofon"};

// Buttons
const int NUM_BUTTONS = 6;
const int buttonInputs[NUM_BUTTONS] = { 13, 12, 11, 10, 1, 0 };
bool buttonValues[NUM_BUTTONS];
bool lastButtonValues[NUM_BUTTONS];

// Serial communication
String serialBuffer = "";
unsigned long lastSerialSend = 0;
const unsigned long SERIAL_SEND_INTERVAL = 25; // Minimum ms between sends

void setup() {
  // Slider Setup
  for (int i = 0; i < NUM_SLIDERS; i++) {
    pinMode(analogInputs[i], INPUT);
  }

  for (int i = 0; i < NUM_RELAIS; i++) {
    pinMode(sliderOutputs[i], OUTPUT);
  }

  // Setup Buttons
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonInputs[i], INPUT);
    lastButtonValues[i] = false;
  }
  
  // Initialize TFT display
  tft.initR(INITR_BLACKTAB);
  
  // Set rotation to landscape mode
  tft.setRotation(1);
  tft.setSPISpeed(8000000);
  
  // Clear screen
  tft.fillScreen(ST77XX_BLACK);
  
  Serial.begin(115200);
  // Display startup message
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 0);
  tft.println("Waiting for USB...");
  
  // Wait for USB serial connection
  while (! Serial) {
    delay(10);
  }
  
  delay(3000);
  
  tft.fillScreen(ST77XX_BLACK);
  Serial.println("Arduino ready");
}

void loop() {
  // Check for incoming serial commands
  handleIncomingSerial();
  
  // Read local hardware (sliders and buttons)
  bool changed = readValues();
  
  // Send updates if something changed (with rate limiting)
  if (changed && (millis() - lastSerialSend >= SERIAL_SEND_INTERVAL)) {
    sendValues();
    lastSerialSend = millis();
  }
  
  // Draw Idle Screen when last Action is too far in the past
  if (millis() - lastAction >= 2000) {
    handleIMGSend();
    lastSerialSend = millis();
  }
}

// ===== SERIAL COMMUNICATION =====

void handleIncomingSerial() {
  while (Serial.available() > 0) {
    serialBuffer = Serial.readStringUntil('\n');

    // Process complete line
    if (serialBuffer.length() > 0) {
      processSerialCommand(serialBuffer);
      serialBuffer = "";
    }
  }
}

void processSerialCommand(String command) {
  command.trim();
  
  if (command.length() == 0) {
    return;
  }
  
  // Command format: CMD:PARAM1:PARAM2
  // Examples:
  //   SET:0:75    - Set slider 0 to 75%
  //   PING        - Respond with PONG
  
  int firstColon = command.indexOf(':');
  String cmd = command.substring(0, firstColon);
  
  if (cmd == "SET") {
    // SET:slider:percentage
    int secondColon = command.indexOf(':', firstColon + 1);
    int slider = command.substring(firstColon + 1, secondColon).toInt();
    int percentage = command.substring(secondColon + 1).toInt();
    
    if (slider >= 0 && slider < NUM_SLIDERS && percentage >= 0 && percentage <= 100) {
      sliderGoTo(percentage, slider);
      Serial.print("OK:SET:");
      Serial.print(slider);
      Serial.print(":");
      Serial.println(percentage);
    } else {
      Serial.println("ERROR:INVALID_PARAMS");
    }
  } else if (cmd == "PING") {
    Serial.println("PONG");
  } else if (cmd == "IMG") {
    currentIMGState = READING_SIZE;
    handleIMGSend();
  } else {
    Serial.print("ERROR:UNKNOWN_CMD:");
    Serial.println(cmd);
  }
}

void sendValues() {
  String builtString = "";
  bool firstValue = true;

  // Sliders
  for (int i = 0; i < NUM_SLIDERS; i++) {
    if (sliderActive[i]) {
      if (!firstValue) { builtString += "|"; }
      
      builtString += "s";
      builtString += String(i);
      builtString += "v";
      builtString += String((int)percentSliderValues[i]);

      firstValue = false;
      sliderActive[i] = false; // Reset after sending
    } 
  }
  
  // Buttons
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (buttonValues[i]) {
      if (!firstValue) { builtString += "|"; }

      builtString += "b";
      builtString += String(i);
      builtString += "v";
      builtString += String((int)buttonValues[i]);

      firstValue = false;
    }
  }

  if (builtString.length() > 0) {
    Serial.println(builtString);
  }
}

// ===== HARDWARE READING =====

bool readValues() {
  bool changed = false;
  
  // Read Buttons
  for (int i = 0; i < NUM_BUTTONS; i++) { 
    bool currentState = digitalRead(buttonInputs[i]);
    
    // Only trigger on button press (transition from LOW to HIGH)
    if (currentState && !lastButtonValues[i]) {
      buttonValues[i] = true;
      changed = true;
      lastAction = millis();
    } else {
      buttonValues[i] = false;
    }
    
    lastButtonValues[i] = currentState;
  }

  // Read Sliders
  changed = changed || updateSliderValues();

  return changed;
}

bool updateSliderValues() {
  bool changed = false;
  
  for (int i = 0; i < NUM_SLIDERS; i++) {
    int normalized = readSlider(i);

    // Prevent random jiggles, while ensuring smooth movement
    if (normalized != lastSliderValues[i] && normalized != secondLastSliderValues[i]) {
      percentSliderValues[i] = normalized;
      secondLastSliderValues[i] = lastSliderValues[i];
      lastSliderValues[i] = normalized;
      
      // Screen updating
      delegateDisplay(normalized, i);
      lastAction = millis();
      lastSliderActive = i;
      sliderActive[i] = true;
      changed = true;
    }
  }

  return changed;
}

int readSlider(uint8_t slider) {
  int analogValue = analogRead(analogInputs[slider]);
  float dirtyFloat = analogValue / 1023.0;
  return normalizeValue(dirtyFloat);
}

int normalizeValue(float v) {
  return round(v * 100);
}

// ===== SLIDER MOTOR CONTROL =====

void sliderGoTo(uint8_t aim, uint8_t slider) {
  if (aim != 100) {
    for (int i = 0; i < 3; i++) {
      int curr = readSlider(slider);
      while (isUnequal(curr, aim)) {
        if (curr < aim) {
          steer(slider, true);
        } else {
          steer(slider, false);
        }
        curr = readSlider(slider);
      }
      haltSliders();
      delay(100);
    }
  } else {
    steer(slider, true);
    delay(250);
    digitalWrite(sliderOutputs[0], false);
    delay(50);
    haltSliders();
  }
  
  int curr = readSlider(slider);
  delegateDisplay(aim, slider);
  lastSliderValues[slider] = curr;
  secondLastSliderValues[slider] = aim;
  lastAction = millis();
}

bool isUnequal(uint8_t curr, uint8_t aim) {
  if (aim == 100) { return curr != 100; }
  else if (aim == 0) { return curr != 0; }  
  else { return curr != aim && curr != aim + 1 && curr != aim - 1; }
}

void steer(uint8_t slider, bool dir) {
  // True = towards 100 %
  bool outputs[NUM_RELAIS];
  
  switch (slider) {
    case 0:
    outputs[0] = true;
    outputs[1] = dir;
    break;
  case 1:
    outputs[0] = true;
    outputs[1] = dir;
    break;
  case 2:
    outputs[0] = true;
    outputs[1] = dir;
    break;
  case 3:
    outputs[0] = true;
    outputs[1] = dir;
    break;
  case 4:
    outputs[0] = true;
    outputs[1] = dir;
    break;
  case 5:
      outputs[0] = true;
      outputs[1] = dir;
      break;
    default:
      outputs[0] = false;
      outputs[1] = false;
      break;
  }
  
  for (int i = 0; i < NUM_RELAIS; i++) {
    digitalWrite(sliderOutputs[i], outputs[i]);
  }
}

void haltSliders() {
  for (int i = 0; i < NUM_RELAIS; i++) {
    digitalWrite(sliderOutputs[i], false);
  }
}

// Display functions

void delegateDisplay(uint8_t percentage, uint8_t slider) {
  if (currentScreenState == PERCENTAGE) {
    if (lastSliderActive == slider) { updatePercentageSameSlider(percentage); }
    else { updatePercentage(percentage, slider); }
  } else { displayPercentage(percentage, slider); }
  imageOnScreen = false;
}

void displayPercentage(uint8_t percentage, uint8_t slider) {
  // Clear Screen
  tft.fillScreen(ST77XX_BLACK);
  delay(100);

  // Textfarbe weiss
  tft.setTextColor(ST77XX_WHITE);
  
  // Rechteck-Rahmen
  for(int i = 0; i < 3; i++) {
    tft.drawRect(5 + i, 5 + i, 150 - (2 * i), 118 - (2 * i), ST77XX_WHITE);
  }

  updatePercentage(percentage, slider);
  currentScreenState = PERCENTAGE;
}

void updatePercentage(uint8_t percentage, uint8_t slider) {
  // Textfarbe weiss
  tft.setTextColor(ST77XX_WHITE);
  
  // Alte Zahl löschen (schwarzes Rechteck über den Bereich)
  tft.fillRect(20, 40, 110, 45, ST77XX_BLACK);
 
  updatePercentageSameSlider(percentage);

  // Label schreiben
  tft.setTextSize(1);
  if(slider < 6) {
    tft.setCursor(30, 40);
    tft.print(sliderNames[slider]);
  }
}

void updatePercentageSameSlider(uint8_t percentage) {
  // Prozentzahl anzeigen
  tft.setTextSize(4);
  
  // Alte Zahl löschen (schwarzes Rechteck über den Bereich)
  tft.fillRect(30, 55, 70, 28, ST77XX_BLACK);
  
  // Neue Zahl schreiben
  tft.setTextColor(ST77XX_WHITE);
  char buffer[4];
  sprintf(buffer, "%02d", percentage);
  tft.setCursor(30, 55);
  tft.print(buffer);
  tft.setCursor(102, 55);
  tft.print("%");
}

void handleIMGSend() {
  if (currentIMGState == IDLE) {
    unsigned long now = millis();    
    unsigned long diff = now - lastImageRequest;  

    if (diff >= imageRequestInterval) {
      requestImage();
    }

    currentScreenState = IDLE_SCREEN;
  }

  // For other states, wait for serial data
  while (Serial.available() > 0) {
    switch (currentIMGState) {
      case WAITING_FOR_HEADER:
        handleHeader();
        break;
        
      case READING_SIZE:
        handleSize();
        break;
        
      case RECEIVING_IMAGE:
        for (int i = 0; i < IMAGE_WIDTH; i++) {
          receiveImageData(i);
        }
        drawLine();
        break;

      case RECEIVING_DATA:
        handleData();
        break;
    }
  }
}

void handleData() {
  String title = Serial.readStringUntil('\t');
  title.trim();
  String artist = Serial.readStringUntil('\n');
  artist.trim();

  delay(100);
  tft.fillRect(0, IMAGE_Y + IMAGE_HEIGHT, tft.width(), 28, ST77XX_BLACK);

  uint8_t titleSize = 2;
  if (title.length() > 10) {
    titleSize = 1;
  }
  uint8_t start_x_title = 0;
  if (title.length() <= 20) {
    start_x_title = IMAGE_X;
  }

  uint8_t start_x_artist = IMAGE_X;
  if (artist.length() > 20) {
    start_x_artist = 0;
  }

  tft.setCursor(start_x_title, IMAGE_Y + IMAGE_HEIGHT + 2);
  tft.setTextSize(titleSize);
  tft.setTextColor(ST77XX_WHITE);
  tft.println(title);
  tft.setCursor(start_x_artist, IMAGE_Y + IMAGE_HEIGHT + 20);
  tft.setTextSize(1);
  tft.setTextColor(46582);
  tft.println(artist);

  currentIMGState = IDLE;
  imageOnScreen = true;
}

void receiveImageData(int iteration) {
  unsigned long start = millis();
  
  while(Serial.available() < 2) {
    if (millis() - start > 3000) {
      return; 
    }
  }

  for(int i=iteration * 2; i < (iteration * 2) + 2; i++) {
    lineBuffer[i] = Serial.read();
  }
}

void drawLine() {
  delay(100);
  
  uint16_t* colorBuffer = (uint16_t*)lineBuffer;
  
  for (uint16_t x = 0; x < IMAGE_WIDTH; x++) {
    uint16_t bufferIndex = x * 2;
    colorBuffer[x] = ((uint16_t)lineBuffer[bufferIndex] << 8) | lineBuffer[bufferIndex + 1];
  }
  tft.drawRGBBitmap(IMAGE_X, IMAGE_Y + currentLine, colorBuffer, IMAGE_WIDTH, 1);

  pixelsReceived += IMAGE_WIDTH;
  currentLine++;

  // Wenn es fertig ist
  if ((pixelsReceived * 2) >= imageSize) {
    // Reset Variables
    currentLine = 0;
    pixelsReceived = 0;

    currentIMGState = RECEIVING_DATA;
  }
}

void handleHeader() {
  String input = Serial.readStringUntil('\n');
  input.trim();

  if (input == "IMG") {
    currentIMGState = READING_SIZE;
    delay(100);
    tft.fillRect(0, IMAGE_Y, IMAGE_WIDTH, IMAGE_HEIGHT, ST77XX_BLACK);
    tft.fillRect(IMAGE_X + IMAGE_WIDTH, IMAGE_Y, IMAGE_WIDTH, IMAGE_HEIGHT, ST77XX_BLACK);
  } else {
    currentIMGState = IDLE;
  }
}

void handleSize() {
  unsigned long start = millis();
  
  while(Serial.available() < 4) {
    if (millis() - start > 3000) { // Timeout nach 3 Sekunden
      tft.println("\nTIMEOUT!");
      tft.print("Available: ");
      tft.println(Serial.available());
      return; 
    }
  }

  uint8_t bytes[4];
  for(int i=0; i<4; i++) {
    bytes[i] = Serial.read();
  }

  imageSize = ((uint32_t)bytes[0] << 24) |
              ((uint32_t)bytes[1] << 16) |
              ((uint32_t)bytes[2] << 8) |
              ((uint32_t)bytes[3]);
  
  if (imageSize > 0 && imageSize < 50000) {
    currentIMGState = RECEIVING_IMAGE;
  } else {
     tft.println("Size invalid");
  }
}

void requestImage() {
  if (imageOnScreen) {
    Serial.print("REQ\n");
  } else {
    Serial.print("REQ:NEW\n");
  }
  lastImageRequest = millis();
  currentIMGState = WAITING_FOR_HEADER;
}
