#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// TFT display pins
#define TFT_CS    4
#define TFT_DC    2
#define TFT_RST   3

// Display dimensions in landscape mode
#define DISPLAY_WIDTH  160
#define DISPLAY_HEIGHT 128

// Image dimensions (must match Go program)
#define IMAGE_WIDTH  25
#define IMAGE_HEIGHT 25

// Position to display image
#define IMAGE_X 30
#define IMAGE_Y 0

// Create display object
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Buffer for one line of RGB565 data (100 pixels * 2 bytes)
uint8_t lineBuffer[IMAGE_WIDTH * 2];

// Communication state
enum State {
  WAITING_FOR_HEADER,
  READING_SIZE,
  RECEIVING_IMAGE
};

State currentState = WAITING_FOR_HEADER;
uint8_t headerIndex = 0;
uint32_t imageSize = 0;
uint32_t pixelsRecieved = 0;
uint16_t currentLine = 0;
uint16_t currentColumn = 0;
bool firstImgByte = true;

void setup() {
  Serial.begin(115200);
  
  // Initialize TFT display
  tft.initR(INITR_BLACKTAB);
  
  // Set rotation to landscape mode
  tft. setRotation(1);
  tft.setSPISpeed(8000000);
  
  // Clear screen
  tft. fillScreen(ST77XX_BLACK);
  
  // Display startup message
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 0);
  tft.println("Waiting for USB...");
  
  // Wait for USB serial connection
  while (! Serial) {
    delay(10);
  }
  
  tft.setCursor(10, 10);
  tft.println("USB connected!");
  delay(3000);
  
  tft.fillScreen(ST77XX_BLACK);
  requestImage();
}

void loop() {
  while (Serial.available() > 0) {
    switch (currentState) {
      case WAITING_FOR_HEADER:
        handleHeader();
        break;
        
      case READING_SIZE:
        handleSize();
        break;
        
      case RECEIVING_IMAGE:
        handleImageData();
        break;
    }
  }
}

void handleImageData() {
  unsigned long start = millis();
  
  while(Serial.available() < 2) {
    if (millis() - start > 3000) { // Timeout nach 3 Sekunden
      tft.println("\nTIMEOUT!");
      tft.print("Available: ");
      tft.println(Serial.available());
      return; 
    }
  }

  uint8_t imageData[2];
  for(int i=0; i<2; i++) {
    imageData[i] = Serial.read();
  }

  delay(100);
  
  uint16_t color = ((uint16_t)imageData[0] << 8) | imageData[1];
  tft.drawPixel(currentColumn + IMAGE_X, currentLine + IMAGE_Y, color);

  currentColumn++;
  pixelsRecieved++;
  if (currentColumn >= IMAGE_WIDTH) {
    currentColumn = 0;
    currentLine++;
  }
  if ((pixelsRecieved * 2) >= imageSize) {
    // Reset Variables
    currentColumn = 0;
    currentLine = 0;
    pixelsRecieved = 0;

    currentState = WAITING_FOR_HEADER;
    // TFT Logging
    tft.setCursor(IMAGE_X, IMAGE_Y + IMAGE_HEIGHT + 5);
    tft.println("Completed");
    delay(10 * 1000);
    tft.fillRect(IMAGE_X, IMAGE_Y + IMAGE_HEIGHT + 5, 100, 10, ST77XX_BLACK);
    delay(500);

    // Request new image
    requestImage();
  }
}

// Working and tested - No delays
void handleHeader() {
  String input = Serial.readStringUntil('\n');

  if (input == 'IMG') {
    currentState = READING_SIZE;
  }
}

void handleSize() {
  unsigned long start = millis();
  
  while(Serial.available() < 4) {
    if (millis() - start > 3000) { // Timeout nach 3 Sekunden
      tft.println("\nTIMEOUT!");
      tft.print("Available: ");
      tft.println(Serial.available());
      return; 
    }
  }

  uint8_t bytes[4];
  for(int i=0; i<4; i++) {
    bytes[i] = Serial.read();
  }

  imageSize = ((uint32_t)bytes[0] << 24) |
              ((uint32_t)bytes[1] << 16) |
              ((uint32_t)bytes[2] << 8) |
              ((uint32_t)bytes[3]);
  
  if (imageSize > 0 && imageSize < 50000) {
    currentState = RECEIVING_IMAGE;
  } else {
     tft.println("Size invalid");
  }
}

void requestImage() {
  Serial.print("REQ\n");
}