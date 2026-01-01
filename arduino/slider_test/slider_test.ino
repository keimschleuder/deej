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
  int aim = 25;
  int slider = 0;
  sliderGoTo(aim, slider);
  delay(1000);
}

void updateSliderValues() {
  // Sliders
  for (int i = 0; i < NUM_SLIDERS; i++) {
    int analogValue = analogRead(analogInputs[i]);

    // Convert to percentages + Noise cancelling
    float dirtyFloat = analogValue / 1023.0;
    float normalized = normalizeValue(dirtyFloat);  // Two decimal digits
    // Serial.println(dirtyFloat);

    if (normalized != lastSliderValues[i]) {
      percentSliderValues[i] = normalized;
      lastSliderValues[i] = normalized;
      // Screen updating
      displayPercentage(normalized, i);
      lastAction = millis();
    } else {
      percentSliderValues[i] = 0;
    }
  }
}

void sliderGoTo(uint8_t aim, uint8_t slider) {
  int curr = readSlider(slider);
  while (curr != aim && curr != aim + 1 && curr != aim - 1) {
    if (curr < aim) {
      steer(slider, true);
    } else {
      steer(slider, false);
    }
    curr = readSlider(slider);
    Serial.println(curr);
  }
  haltSliders();
}

void displayPercentage(uint8_t percentage, uint8_t slider) {
  Serial.println(percentage);
}

// Helper Functions
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
    digitalWrite(sliderOutputs[i], 0);
  }
}
