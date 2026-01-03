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

// Serial
enum SerialState {
  RECIEVE_PERCENTAGE, 
  RECIEVE_IMAGE, 
  SEND_PERCENTAGE, 
  IDLE_SERIAL
};

SerialState serialState = IDLE_SERIAL;

void setup() {
  delay(1000);

  // Setup Screen
  Screen.begin();
  //Hintergrund: Schwarz
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
  switch (serialState)
  {
  case RECIEVE_PERCENTAGE:
    recieveValues();
    break;
  case RECIEVE_IMAGE:
    recieveImage();
    break;
  case SEND_PERCENTAGE:
    sendValues();
    serialState = IDLE_SERIAL;
    break;
  case IDLE_SERIAL:
    bool changed = readValues();
    if (changed) {
      // TODO Request a change in serial state to SEND_PERCENTAGE and then continue
    }
    // TODO Check for RECIEVE requests
    break;
  default:
    break;
  }

  // Draw Idle Screen, when last Action is too far in the past
  if (millis() - lastAction >= 2000 && currentScreenState != IDLE_SCREEN) {
    drawIdle();
  }
}

bool readValues() {
  bool changed = false;
  // Buttons
  for (int i = 0; i < NUM_BUTTONS; i++) { 
    buttonValues[i] = digitalRead(buttonInputs[i]);
    if (buttonValues[i]) { changed = true; }    
  }

  // Sliders
  changed = changed || updateSliderValues();

  return changed;
}

// Serial
void recieveValues() {
  if (Serial.available() > 0) {
    // Read the incoming percentage value
    int percentage = Serial.parseInt();

    // Clear any remaining characters in the buffer (like newline)
    while (Serial.available() > 0) {
      Serial.read();
    }

    // Skip invalid values
    if ( 0 <= percentage && percentage <= 100 ) {
      sliderGoTo(percentage, 0);
    }
  }
}

void sendValues() {
  String builtString = String("");
  bool firstValue = true;

  // Sliders
  for (int i = 0; i < NUM_SLIDERS; i++)
  {
    if (sliderActive[i])
    {
      if (!firstValue) { builtString += String("|"); }
      
      builtString += "s";
      builtString += String(i);
      builtString += "v";
      builtString += String((int)percentSliderValues[i]);

      firstValue = false;
    } 
  }
  
  // Buttons
  for (int i = 0; i < NUM_BUTTONS; i++)
  {
    if (buttonValues[i])
    {
      if (!firstValue) { builtString += String("|"); }

      builtString += "b";
      builtString += String(i);
      builtString += "v";
      builtString += String((bool)buttonValues[i]);

      firstValue = false;
    }
    
  }

  Serial.println(builtString);
}

void recieveImage() {
  // TODO
}

// Slider
bool updateSliderValues() {
  bool changed = false;
  // Sliders
  for (int i = 0; i < NUM_SLIDERS; i++) { // Prevent random jiggles, while also ensuring smooth movement
    int normalized = readSlider(i);

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
    } else {
      percentSliderValues[i] = 0;
    }
  }

  return changed;
}

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
}

bool isUnequal(uint8_t curr, uint8_t aim) {
  if (aim == 100) { return curr != 100; }
  else if (aim == 0) { return curr != 0; }  
  else { return curr != aim && curr != aim + 1 && curr != aim - 1; }
}

int normalizeValue(float v) {
  return round(v * 100);  // Auf 2 Nachkommastellen kürzen: 0.99856 -> 0.99 
}

int readSlider(uint8_t slider) {
  int analogValue = analogRead(analogInputs[slider]);

  // Convert to percentages + Noise cancelling
  float dirtyFloat = analogValue / 1023.0;
  return normalizeValue(dirtyFloat);  // Two decimal digits
}

void steer(uint8_t slider, bool dir) {
  // True = towards 100 %
  bool outputs[NUM_RELAIS];
  switch (slider)
  {
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

// Display Functions
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

  // TODO: Is playing?
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
