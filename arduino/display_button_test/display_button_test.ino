#include <TFT.h>
#include <SPI.h>

//PIN-Einstellungen
#define cs   17
#define dc   2
#define rst  3

const int NUM_BUTTONS = 6;
const int buttonInputs[NUM_BUTTONS] = { 13, 12, 11, 10, 1, 0 };

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

void displayPercentage(uint8_t percentage, uint8_t slider) {
  // Clear Screen
  Screen.fillScreen(Screen.Color565(0, 0, 0));

  // Textfarbe weiss
  Screen.stroke(255, 255, 255);
  // Rechteck-Rahmen
  for(int i = 0; i < 3; i++) {
    Screen.drawRect(5 + i, 5 + i, 150 - (2 * i), 118 - (2 * i), Screen.Color565(255, 255, 255));
  }
  // Prozentzahl anzeigen
  Screen.textSize(4);
  
  // Alte Zahl löschen (schwarzes Rechteck über den Bereich)
  Screen.fill(0, 0, 0);
  Screen.noStroke();
  Screen.rect(20, 40, 110, 45);
  
  // Neue Zahl schreiben
  Screen.stroke(255, 255, 255);
  char buffer[4];
  sprintf(buffer, "%02d", percentage);
  Screen.text(buffer, 30, 55);
  Screen.text("%", 102, 55);

  // Label schreiben
  Screen.textSize(1);
  if(slider < 6) {
    Screen.text(sliderNames[slider], 30, 40);
  }

  currentScreenState = PERCENTAGE;
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

void setup() {
  delay(1000);

  // Setup Screen
  Screen.begin();
  //Hintergrund: Schwarz
  Screen.background(0, 0, 0);
  //Textfarbe: Weiß
  Screen.stroke(blue, green, red);

  drawIdle();

  // Setup Buttons
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonInputs[i], INPUT);
  }
}

void loop() {
  // Read button states
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (digitalRead(buttonInputs[i])) {
      displayPercentage(50, i);
      lastAction = millis();
    }
  }

  // Draw Idle Screen, when last Action is too far in the past
  if (millis() - lastAction >= 2000 && currentScreenState != IDLE) {
    drawIdle();
  }

  delay(100);
}
