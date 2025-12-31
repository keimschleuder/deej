#include <TFT.h>
#include <SPI.h>

// Display Constants
#define cs   17
#define dc   2
#define rst  3

const char* sliderNames[] = {"Master Volume", "Aktuelles Fenter", "Discord", "Musik", "Alles andere", "Mikrofon"};

TFT TFTscreen = TFT(cs, dc, rst);
uint8_t red = 255;
uint8_t green = 255;
uint8_t blue = 255;

// Slider and Button Constants
const int NUM_SLIDERS = 6;
const int analogInputs[NUM_SLIDERS] = { A0, A1, A3, A3, A4, A5 };
const int sliderOutputs[NUM_SLIDERS] = { 9, 8, 7, 6, 5, 4 };
const int NUM_BUTTONS = 6;
const int buttonInputs[NUM_BUTTONS] = { 13, 12, 11, 10, 1, 0 };
const float noiseReduction = 0.02;

float percentSliderValues[NUM_SLIDERS];
float lastSliderValues[NUM_SLIDERS];
bool buttonValues[NUM_BUTTONS];

void setup() {
  delay(1000);

  // Display Setup
  TFTscreen.begin();
  TFTscreen.background(0, 0, 0);
  TFTscreen.stroke(blue, green, red);

  // Slider and Button Setup
  for (int i = 0; i < NUM_SLIDERS; i++) {
    pinMode(analogInputs[i], INPUT);
  }

  for (int i = 0; i < NUM_SLIDERS; i++) {
    pinMode(sliderOutputs[i], OUTPUT);
  }

  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonInputs[i], INPUT);
  }

  // Serial Setup
  Serial.begin(9600);
}

void loop() {
  updateSliderValues();
  sendSliderValues(); // Actually send data (all the time)
//   printSliderValues(); // For debug
  for (int i = 0; i < NUM_SLIDERS; i++) {
    lastSliderValues[i] = percentSliderValues[i];
  }
  delay(10);
}

void updateSliderValues() {
  // Sliders
  for (int i = 0; i < NUM_SLIDERS; i++) {
    int analogValue = analogRead(analogInputs[i]);

    // Convert to percentages + Noise cancelling
    float dirtyFloat = analogValue / 1023.0;
    float normalized = normalizeValue(dirtyFloat);  // Two decimal digits

    if (hasChanged(normalized, lastSliderValues[i])) {
      percentSliderValues[i] = normalized;
    } else {
      percentSliderValues[i] = 0;
    }
  }
  // Screen updating

  // Buttons
  for (int i = 0; i < NUM_BUTTONS; i++) {
    buttonValues[i] = (digitalRead(buttonInputs[i]) == HIGH);
  }
}

void sendSliderValues() {
  String builtString = String("");

  /* slightly overengineering this stuff. */
  bool changed = false;
  for (int i = 0; i < NUM_SLIDERS; i++) {
    if (percentSliderValues[i] != 0) {
      if (changed) {
        builtString += "|";
      } else {
        changed = true;
      }

      builtString += "s";
      builtString += String((int)i);
      builtString += "x";
      builtString += String((float)percentSliderValues[i]);
      /*Format: "s{Slider Index}x{Slider Vaule (0.0 - 1.0 as %)}*/
    }
  }
  if (!changed) {
    builtString += "s-1x-1";
  }

  if (NUM_BUTTONS > 0) {
    builtString += String("|");
  }

  changed = false;
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (buttonValues[i]) {
      if (changed) {
        builtString += "|";
      } else {
        changed = true;
      }
      builtString += "b";
      builtString += String((int)i);
      /*Returns every pressedes Button as: b{index}*/
    }
  }
  if (!changed) {
    builtString += "b-1";
  }

  /*Default is: s-1x-1|b-1
  Whenever a value chnages or a button is pressed the builtString changed as documented above*/

  Serial.println(builtString);
}

bool hasChanged(int updated, int old) {
  if (abs(old - updated) >= noiseReduction) {
    return true;
  }

  if ((almostEqual(updated, 1.0) && old != 1.0) || almostEqual(updated, 0.0) && old != 0.0) {
    return true;
  }

  return false;
}

bool almostEqual(float a, float b) {
  return (abs(a - b) > noiseReduction);
}

float normalizeValue(float v) {
  float result = floor(v * 100) / 100.0;  // Auf 2 Nachkommastellen kürzen: 0.99856 -> 0.99
  return result;
}

void printSliderValues() {
  for (int i = 0; i < NUM_SLIDERS; i++) {
    String printedString = String("Slider #") + String(i + 1) + String(": ") + String(percentSliderValues[i]) + String(" mV");
    Serial.write(printedString.c_str());

    if (i < NUM_SLIDERS - 1) {
      Serial.write(" | ");
    } else {
      Serial.write("\n");
    }
  }
}
