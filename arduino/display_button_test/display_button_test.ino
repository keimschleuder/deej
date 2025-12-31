#include <TFT.h>
#include <SPI.h>

//PIN-Einstellungen
#define cs   17
#define dc   2
#define rst  3

const int NUM_BUTTONS = 6;
const int buttonInputs[NUM_BUTTONS] = { 13, 12, 11, 10, 1, 0 };

const char* sliderNames[] = {"Master Volume", "Aktuelles Fenter", "Discord", "Musik", "Alles andere", "Mikrofon"};

TFT TFTscreen = TFT(cs, dc, rst);
uint8_t red = 255;
uint8_t green = 255;
uint8_t blue = 255;

void displayPercentage(uint8_t percentage, uint8_t slider) {
  // Textfarbe weiss
  TFTscreen.stroke(255, 255, 255);
  // Rechteck-Rahmen
  for(int i = 0; i < 3; i++) {
    TFTscreen.drawRect(5 + i, 5 + i, 150 - (2 * i), 118 - (2 * i), TFTscreen.Color565(255, 255, 255));
  }
  // Prozentzahl anzeigen
  TFTscreen.textSize(4);
  
  // Alte Zahl löschen (schwarzes Rechteck über den Bereich)
  TFTscreen.fill(0, 0, 0);
  TFTscreen.noStroke();
  TFTscreen.rect(20, 40, 110, 45);
  
  // Neue Zahl schreiben
  TFTscreen.stroke(255, 255, 255);
  char buffer[4];
  sprintf(buffer, "%02d", percentage);
  TFTscreen.text(buffer, 30, 55);
  TFTscreen.text("%", 102, 55);

  // Label schreiben
  TFTscreen.textSize(1);
  if(slider < 6) {
    TFTscreen.text(sliderNames[slider], 30, 40);
  }
}

void setup() {
  delay(1000);

  // Setup Screen
  TFTscreen.begin();
  //Hintergrund: Schwarz
  TFTscreen.background(0, 0, 0);
  //Textfarbe: Weiß
  TFTscreen.stroke(blue, green, red);

  // Setup Buttons
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonInputs[i], INPUT);
  }
}

void loop() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (digitalRead(buttonInputs[i])) {
      displayPercentage(50, i);
    }
  }
  delay(100);
}
