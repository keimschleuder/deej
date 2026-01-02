#include <TFT.h>
#include <SPI.h>

//PIN-Einstellungen
#define cs   17
#define dc   2
#define rst  3

const int NUM_SLIDERS = 1;
const int NUM_RELAIS = 2;
const int analogInputs[NUM_SLIDERS] = { A0 };
const int sliderOutputs[NUM_RELAIS] = { 9, 8 };
const int NUM_BUTTONS = 6;
const int buttonInputs[NUM_BUTTONS] = { 13, 12, 11, 10, 1, 0 };
const float noiseReduction = 1;

float percentSliderValues[NUM_SLIDERS];
float lastSliderValues[NUM_SLIDERS];
float secondLastSliderValues[NUM_SLIDERS];
int lastSliderActive;
bool buttonValues[NUM_BUTTONS];

const char* sliderNames[] = {"Master Volume", "Aktuelles Fenter", "Discord", "Musik", "Alles andere", "Mikrofon"};

TFT Screen = TFT(cs, dc, rst);
uint8_t red = 255;
uint8_t green = 255;
uint8_t blue = 255;

unsigned long lastAction = 0;

enum ScreenState {
  IDLE, 
  PERCENTAGE, 
  SPECIAL
};

ScreenState currentScreenState = IDLE;

void setup() {
  delay(1000);

  // Setup Screen
  Screen.begin();
  //Hintergrund: Schwarz
  Screen.background(0, 0, 0);
  //Textfarbe: Weiß
  Screen.stroke(blue, green, red);

  drawIdle();

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
}

void loop() {
  // Read button states
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (digitalRead(buttonInputs[i])) {
      // displayPercentage(50, i);
      
      // tmp
      switch (i)
      {
      case 0:
        sliderGoTo(50, 0);
        break;
      case 1:
        sliderGoTo(25, 0);
        break;
      case 2:
        sliderGoTo(75, 0);
        break;
      case 3:
        sliderGoTo(100, 0);
        break;
      case 4:
        sliderGoTo(0, 0);
        break;
      case 5:
        sliderGoTo(30, 0);
        break;
      default:
        break;
      }

      lastAction = millis();
    }
  }

  // Handle Sliders
  updateSliderValues();

  // Draw Idle Screen, when last Action is too far in the past
  if (millis() - lastAction >= 2000 && currentScreenState != IDLE) {
    drawIdle();
  }

  delay(100);
}

// Ausgelagert
// Slider Functions
void updateSliderValues() {
  // Sliders
  for (int i = 0; i < NUM_SLIDERS; i++) { // Prevent random jiggles, while also ensuring smooth movement
    int normalized = readSlider(i);

    if (normalized != lastSliderValues[i] && normalized != secondLastSliderValues[i]) {
      percentSliderValues[i] = normalized;
      secondLastSliderValues[i] = lastSliderValues[i];
      lastSliderValues[i] = normalized;
      // Screen updating
      if (currentScreenState == PERCENTAGE) {
        if (lastSliderActive == i) { updatePercentageSameSlider(normalized); }
        else { updatePercentage(normalized, i); }
      } else { displayPercentage(normalized, i); }
      lastAction = millis();
      lastSliderActive = i;
    } else {
      percentSliderValues[i] = 0;
    }
  }
}

void sliderGoTo(uint8_t aim, uint8_t slider) {
  if (aim != 100) {
    for (int i = 0; i < 3; i++) {
      int curr = readSlider(slider);
      while (isEqual(curr, aim)) {
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
  displayPercentage(aim, slider);
  lastSliderValues[slider] = curr;
  secondLastSliderValues[slider] = aim;
}

// Display Functions
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

  currentScreenState = IDLE;
}

// Helper Functions
bool isEqual(uint8_t curr, uint8_t aim) {
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
