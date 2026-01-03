// Screen
#include <TFT.h>
#include <SPI.h>

//PIN-Einstellungen
#define cs   17
#define dc   2
#define rst  3

TFT Screen = TFT(cs, dc, rst);
uint8_t red = 255;
uint8_t green = 255;
uint8_t blue = 255;

unsigned long lastAction = 0;

enum ScreenState {
  IDLE_SCREEN, 
  PERCENTAGE, 
  SPECIAL
};

ScreenState currentScreenState = IDLE_SCREEN;

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
const unsigned long SERIAL_SEND_INTERVAL = 100; // Minimum ms between sends

void setup() {
  delay(1000);

  // Setup Screen
  Screen.begin();
  Screen.background(0, 0, 0);
  //Textfarbe: Weiß
  Screen.stroke(blue, green, red);

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

  // Serial setup
  Serial.begin(9600);
  
  Serial.println("Waiting for serial");

  // Wait for serial connection
  while (!Serial) {
    ; // Wait for serial port to connect
  }
  
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
  if (millis() - lastAction >= 2000 && currentScreenState != IDLE_SCREEN) {
    drawIdle();
  }
}

// ===== SERIAL COMMUNICATION =====

void handleIncomingSerial() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      // Process complete line
      if (serialBuffer.length() > 0) {
        processSerialCommand(serialBuffer);
        serialBuffer = "";
      }
    } else {
      serialBuffer += c;
    }
  }
}

void processSerialCommand(String command) {
  command.trim();
  
  if (command.length() == 0) {
    return;
  }
  
  // Check if it's a simple percentage value (backward compatibility)
  if (command.toInt() >= 0 && command.toInt() <= 100) {
    int percentage = command.toInt();
    sliderGoTo(percentage, 0);
    Serial.print("OK:SET:");
    Serial.println(percentage);
    return;
  }
  
  // Command format: CMD:PARAM1:PARAM2
  // Examples:
  //   SET:0:75    - Set slider 0 to 75%
  //   GET         - Request current status
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
  }
  else if (cmd == "GET") {
    // Request current status
    sendValues();
  }
  else if (cmd == "PING") {
    Serial.println("PONG");
  }
  else {
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

// ===== DISPLAY FUNCTIONS =====

void delegateDisplay(uint8_t percentage, uint8_t slider) {
  if (currentScreenState == PERCENTAGE) {
    if (lastSliderActive == slider) { updatePercentageSameSlider(percentage); }
    else { updatePercentage(percentage, slider); }
  } else { displayPercentage(percentage, slider); }
}

void displayPercentage(uint8_t percentage, uint8_t slider) {
  // Clear Screen
  Screen.fillScreen(Screen.Color565(0, 0, 0));

  // Textfarbe weiss
  Screen.stroke(255, 255, 255);
  
  // Rechteck-Rahmen
  for(int i = 0; i < 3; i++) {
    Screen.drawRect(5 + i, 5 + i, 150 - (2 * i), 118 - (2 * i), Screen.Color565(255, 255, 255));
  }

  updatePercentage(percentage, slider);
  currentScreenState = PERCENTAGE;
}

void updatePercentage(uint8_t percentage, uint8_t slider) {
  // Textfarbe weiss
  Screen.stroke(255, 255, 255);
  
  // Alte Zahl löschen (schwarzes Rechteck über den Bereich)
  Screen.fill(0, 0, 0);
  Screen.noStroke();
  Screen.rect(20, 40, 110, 45);
 
  updatePercentageSameSlider(percentage);

  // Label schreiben
  Screen.textSize(1);
  if(slider < 6) {
    Screen.text(sliderNames[slider], 30, 40);
  }
}

void updatePercentageSameSlider(uint8_t percentage) {
  // Textfarbe weiss
  Screen.stroke(255, 255, 255);
  
  // Prozentzahl anzeigen
  Screen.textSize(4);
  
  // Alte Zahl löschen (schwarzes Rechteck über den Bereich)
  Screen.fill(0, 0, 0);
  Screen.noStroke();
  Screen.rect(30, 55, 70, 28);
  
  // Neue Zahl schreiben
  Screen.stroke(255, 255, 255);
  char buffer[4];
  sprintf(buffer, "%02d", percentage);
  Screen.text(buffer, 30, 55);
  Screen.text("%", 102, 55);
}

void drawIdle() {
  // Clear Screen
  Screen.fillScreen(Screen.Color565(0, 0, 0));

  // TODO: Coverart
  Screen.fillRect(30, 0, 100, 100, Screen.Color565(0, 0, 255));

  // Media controls
  uint16_t color = Screen.Color565(255, 255, 255);
  uint8_t yTop = 107;
  uint8_t yMiddle = 113;
  uint8_t yBottom = 119;

  // Last Track
  Screen.fillTriangle(30, yMiddle, 36, yTop, 36, yBottom, color);
  Screen.fillTriangle(35, yMiddle, 41, yTop, 41, yBottom, color);

  // Play/Pause
  Screen.fillTriangle(82, yMiddle, 76, yTop, 76, yBottom, color);
  Screen.fillRect(83, yTop, 2, 13, color);

  // Next Track
  Screen.fillTriangle(125, yMiddle, 119, yTop, 119, yBottom, color);
  Screen.fillTriangle(130, yMiddle, 124, yTop, 124, yBottom, color);

  currentScreenState = IDLE_SCREEN;
}