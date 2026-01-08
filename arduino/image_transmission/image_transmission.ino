#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// TFT display pins
#define TFT_CS    4
#define TFT_DC    2
#define TFT_RST   3

// Image dimensions (must match Go program)
#define IMAGE_WIDTH  100
#define IMAGE_HEIGHT 100

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
uint32_t imageSize = 0;
uint32_t pixelsRecieved = 0;
uint16_t currentLine = 0;

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
        for (int i = 0; i < IMAGE_WIDTH; i++) {
          receiveImageData(i);
        }
        drawLine();
        break;
    }
  }
}

void receiveImageData(int iteration) {
  unsigned long start = millis();
  
  while(Serial.available() < 2) {
    if (millis() - start > 3000) {
      return; 
    }
  }

  for(int i=iteration * 2; i < (iteration * 2) + 2; i++) {
    lineBuffer[i] = Serial.read();
  }
}

void drawLine() {
  delay(100);
  
  uint16_t* colorBuffer = (uint16_t*)lineBuffer;
  
  for (uint16_t x = 0; x < IMAGE_WIDTH; x++) {
    uint16_t bufferIndex = x * 2;
    colorBuffer[x] = ((uint16_t)lineBuffer[bufferIndex] << 8) | lineBuffer[bufferIndex + 1];
  }
  tft. drawRGBBitmap(IMAGE_X, IMAGE_Y + currentLine, colorBuffer, IMAGE_WIDTH, 1);

  pixelsRecieved += IMAGE_WIDTH;
  currentLine++;

  // Wenn es fertig ist
  if ((pixelsRecieved * 2) >= imageSize) {
    // Reset Variables
    currentLine = 0;
    pixelsRecieved = 0;

    currentState = WAITING_FOR_HEADER;
    delay(10 * 1000);
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