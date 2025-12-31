#include <TFT.h>
#include <SPI.h>
 
//PIN-Einstellungen
#define cs   4
#define dc   2
#define rst  3

TFT TFTscreen = TFT(cs, dc, rst);
uint8_t red = 255;
uint8_t green = 255;
uint8_t blue = 255;

void setup() {
  delay(1000);
  TFTscreen.begin();
  //Hintergrund: Schwarz
  TFTscreen.background(0, 0, 0);
  //Textfarbe: Weiß
  TFTscreen.stroke(blue, green, red);
}

void displayPercentage(uint8_t percentage) {
  // Textfarbe weiss
  TFTscreen.stroke(255, 255, 255);
  // Rechteck-Rahmen
  for(int i = 0; i < 3; i++) {
    TFTscreen.drawRect(5 + i, 5 + i, 150 - (2 * i), 118 - (2 * i), 255);
  }
  // Prozentzahl anzeigen
  TFTscreen.textSize(4);
  
  // Alte Zahl löschen (schwarzes Rechteck über den Bereich)
  TFTscreen.fill(0, 0, 0);
  TFTscreen.noStroke();
  TFTscreen.rect(20, 45, 78, 35);
  
  // Neue Zahl schreiben
  TFTscreen.stroke(255, 255, 255);
  char buffer[4];
  sprintf(buffer, "%02d", percentage);
  TFTscreen.text(buffer, 30, 50);
  TFTscreen.text("%", 100, 50);
}

void loop() {
  displayPercentage(10);
  delay(1000);
  displayPercentage(0);
  delay(1000);
  displayPercentage(100);
  delay(1000);
  displayPercentage(50);
  delay(1000);
}
