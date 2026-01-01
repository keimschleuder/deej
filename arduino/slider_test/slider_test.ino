const int NUM_SLIDERS = 1;
const int NUM_RELAIS = 2;
const int analogInputs[NUM_SLIDERS] = { A0 };
const int sliderOutputs[NUM_RELAIS] = { 9, 8 };
const float noiseReduction = 0.02;

float percentSliderValues[NUM_SLIDERS];
float lastSliderValues[NUM_SLIDERS];

unsigned long lastAction = 0;

enum ScreenState {
  IDLE, 
  PERCENTAGE, 
  SPECIAL
};

ScreenState currentScreenState = IDLE;

void setup() {
  delay(1000);

  // Slider Setup
  for (int i = 0; i < NUM_SLIDERS; i++) {
    pinMode(analogInputs[i], INPUT);
  }

  for (int i = 0; i < NUM_RELAIS; i++) {
    pinMode(sliderOutputs[i], OUTPUT);
  }

  Serial.begin(9600);
}

void loop() {
  if (millis() - lastAction >= 2000 && currentScreenState != IDLE) {
    Serial.println("Idle");
  }

  // updateSliderValues();
  digitalWrite(sliderOutputs[0], 1);
  delay(1000);
  digitalWrite(sliderOutputs[1], 1);
  delay(1000);
  digitalWrite(sliderOutputs[1], 0);
  delay(1000);
  digitalWrite(sliderOutputs[0], 0);
  delay(1000);
}

void updateSliderValues() {
  // Sliders
  for (int i = 0; i < NUM_SLIDERS; i++) {
    int analogValue = analogRead(analogInputs[i]);

    // Convert to percentages + Noise cancelling
    float dirtyFloat = analogValue / 1023.0;
    float normalized = normalizeValue(dirtyFloat);  // Two decimal digits
    Serial.println(analogValue);

    /* if (hasChanged(normalized, lastSliderValues[i])) {
      percentSliderValues[i] = normalized;
      // Screen updating
      displayPercentage(normalized, i);
      lastAction = millis();
    } else {
      percentSliderValues[i] = 0;
    } */
  }
}

void displayPercentage(float percentage, uint8_t slider) {
  Serial.println(percentage, slider);
}

// Helper Functions
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
