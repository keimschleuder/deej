const int NUM_SLIDERS = 6;
const int analogInputs[NUM_SLIDERS] = {A0, A1, A3, A6, A7, A8};
const int NUM_BUTTONS = 6;
const int buttonInputs[NUM_BUTTONS] = {0, 1, 2, 3, 5, 7};
const float noiseReduction = 0.02

int analogSliderValues[NUM_SLIDERS];
float percentSliderValues[NUM_SLIDERS];
int lastSliderValues[NUM_SLIDERS];
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
  delay(10);
  lastSliderValues = analogSliderValues;
}

void updateSliderValues() {
  for (int i = 0; i < NUM_SLIDERS; i++) {
    analogSliderValues[i] = analogRead(analogInputs[i]);
  }

  // Convert to percentages + Noise cancelling
  percentSliverValues = []
  for (int i = 0; i < NUM_SLIDERS; i++) {
    float dirtyFloat = analogSliderValues[i] / 1023.0
    float normalized = normalizeValue(dirtyFloat) // Two decimal digits

    if (hasChanged(normalized, lastSliderValues[i])) {
      percentSliderValues[i] = normalized
    }
  }
  // Screen updating

  for (int i = 0; i < NUM_BUTTONS; i++) {
    buttonValues[i] = digitalRead(buttonInputs[i]);
  }
}

void sendSliderValues() {
  String builtString = String("");

  for (int i = 0; i < NUM_SLIDERS; i++) {
    builtString += "s";
    builtString += String((int)analogSliderValues[i]);

    if (i < NUM_SLIDERS - 1) {
      builtString += String("|");
    }
  }

  if(NUM_BUTTONS > 0){
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
  return abs(a - b) > 0.00001
}

float normalizeValue(float v) {
  float result = floor(v * 100) / 100.0;
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
