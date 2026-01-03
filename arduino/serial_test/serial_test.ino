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

// Sliders
const int NUM_SLIDERS = 1;
const int NUM_RELAIS = 2;
const int analogInputs[NUM_SLIDERS] = { A0 };
const int sliderOutputs[NUM_RELAIS] = { 9, 8 };

float percentSliderValues[NUM_SLIDERS];
float lastSliderValues[NUM_SLIDERS];
float secondLastSliderValues[NUM_SLIDERS];
int lastSliderActive;

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

  // Serial setup
  Serial.begin(9600);

  Serial.println("Waiting for serial");

  // Wait for serial connection
  while (!Serial) {
    ; // Wait for serial port to connect
  }
  
  Serial.println("Arduino ready to receive data");
}

void loop() {
  recieveValues();
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

// Slider
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
  displayPercentage(aim);
  lastSliderValues[slider] = curr;
  secondLastSliderValues[slider] = aim;
}

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

// Display 
void displayPercentage(uint8_t percentage) {
  // Clear Screen
  Screen.fillScreen(Screen.Color565(0, 0, 0));

  // Textfarbe weiss
  Screen.stroke(255, 255, 255);
  // Rechteck-Rahmen
  for(int i = 0; i < 3; i++) {
    Screen.drawRect(5 + i, 5 + i, 150 - (2 * i), 118 - (2 * i), Screen.Color565(255, 255, 255));
  }

  Screen.stroke(255, 255, 255);
  
  // Alte Zahl löschen (schwarzes Rechteck über den Bereich)
  Screen.fill(0, 0, 0);
  Screen.noStroke();
  Screen.rect(20, 40, 110, 45);

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
