const int NUM_SLIDERS = 6;
const int analogInputs[NUM_SLIDERS] = { A0, A1, A3, A6, A7, A8 };
const int NUM_BUTTONS = 6;
const int buttonInputs[NUM_BUTTONS] = { 0, 1, 2, 3, 5, 7 };
const float noiseReduction = 0.02

float percentSliderValues[NUM_SLIDERS];
float lastSliderValues[NUM_SLIDERS];
int buttonValues[NUM_BUTTONS];

void setup() {
  for (int i = 0; i < NUM_SLIDERS; i++) {
    pinMode(analogInputs[i], INPUT);
  }

  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonInputs[i], INPUT_PULLUP);
  }

  Serial.begin(9600);
}

void loop() {
  updateSliderValues();
  sendSliderValues(); // Actually send data (all the time)
//   printSliderValues(); // For debug
  lastSliderValues = percentSliderValues;
  delay(10);
}

void updateSliderValues() {
  // Sliders
  percentSliverValues = [];
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
    buttonValues[i] = digitalRead(buttonInputs[i]);
  }
}

void sendSliderValues() {
  String builtString = String("");

  /* slightly overengineering this stuff. */
  bool sliderChanged = false;
  for (int i = 0; i < NUM_SLIDERS; i++) {
    builtString += "s";
    if (percentSliderValues[i] != 0) {
      if (sliderChanged) {
        builtString += "|";
      }
      sliderChanged = true;

      builtString += String((int)i);
      builtString += "x";
      builtString += String((float)percentSliderValues[i]);
    }
  }
  if (!sliderChanged) {
    builtString += "-1x-1";
  }

  if (NUM_BUTTONS > 0) {
    builtString += String("|");
  }

  for (int i = 0; i < NUM_BUTTONS; i++) {
    builtString += "b";
    builtString += String((int)buttonValues[i]);

    if (i < NUM_BUTTONS - 1) {
      builtString += String("|");
    }
  }

  Serial.println(builtString);
}

bool hasChanged(int new, int old) {
  if (abs(old - new) >= noiseReduction) {
    return true;
  }

  if ((almostEqual(new, 1.0) && old != 1.0) || almostEqual(new, 0.0) && old != 0.0) {
    return true
  }

  return false;
}

bool almostEqual(float a, float b) {
  return abs(a - b) > 0.01
}

float normalizeValue(float v) {
  float result = floor(v * 100) / 100.0;  // Auf 2 Nachkommastellen kürzen: 0.99856 -> 0.99
  return result;
}

void printSliderValues() {
  for (int i = 0; i < NUM_SLIDERS; i++) {
    String printedString = String("Slider #") + String(i + 1) + String(": ") + String(analogSliderValues[i]) + String(" mV");
    Serial.write(printedString.c_str());

    if (i < NUM_SLIDERS - 1) {
      Serial.write(" | ");
    } else {
      Serial.write("\n");
    }
  }
}
