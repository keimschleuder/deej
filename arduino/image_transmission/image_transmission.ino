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
uint8_t headerBuffer[4];
uint8_t headerIndex = 0;
uint32_t imageSize = 0;
uint32_t bytesReceived = 0;
uint16_t currentLine = 0;
uint16_t lineBufferIndex = 0;

void setup() {
  Serial.begin(115200);
  
  // Initialize TFT display
  tft.initR(INITR_BLACKTAB);
  
  // Set rotation to landscape mode
  tft. setRotation(1);
  
  // Clear screen
  tft. fillScreen(ST77XX_BLACK);
  
  // Display startup message
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 50);
  tft.println("Waiting for USB...");
  
  // Wait for USB serial connection
  while (! Serial) {
    delay(10);
  }
  
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(10, 50);
  tft.println("USB connected!");
  delay(3000);
  
  tft.fillScreen(ST77XX_BLACK);
  requestImage();
}

void loop() {
  
  while (Serial.available() > 0) {
    uint8_t inByte = Serial.read();
    
    delay(500);

    switch (currentState) {
      case WAITING_FOR_HEADER:
        handleHeader(inByte);
        break;
        
      case READING_SIZE:
        handleSize(inByte);
        break;
        
      case RECEIVING_IMAGE: 
        handleImageData(inByte);
        break;
    }
  }
}

void handleHeader(uint8_t inByte) {
  headerBuffer[headerIndex++] = inByte;
  
  if (headerIndex >= 4) {
    if (headerBuffer[0] == 'I' && headerBuffer[1] == 'M' &&
        headerBuffer[2] == 'G' && headerBuffer[3] == '\n') {
      currentState = READING_SIZE;
      headerIndex = 0;
    } else {
      headerBuffer[0] = headerBuffer[1];
      headerBuffer[1] = headerBuffer[2];
      headerBuffer[2] = headerBuffer[3];
      headerIndex = 3;
    }
  }
}

void handleSize(uint8_t inByte) {
  headerBuffer[headerIndex++] = inByte;
  
  if (headerIndex >= 4) {
    imageSize = ((uint32_t)headerBuffer[0] << 24) |
                ((uint32_t)headerBuffer[1] << 16) |
                ((uint32_t)headerBuffer[2] << 8) |
                ((uint32_t)headerBuffer[3]);
    
    bytesReceived = 0;
    currentLine = 0;
    lineBufferIndex = 0;
    currentState = RECEIVING_IMAGE;
    headerIndex = 0;
    
    tft.fillRect(IMAGE_X, IMAGE_Y, IMAGE_WIDTH, IMAGE_HEIGHT, ST77XX_BLUE);
  }
}

void handleImageData(uint8_t inByte) {
  lineBuffer[lineBufferIndex] = inByte;
  lineBufferIndex++;  
  bytesReceived++;
  
  if (lineBufferIndex >= IMAGE_WIDTH * 2) {
    drawLine(currentLine);
    currentLine++;
    lineBufferIndex = 0;
  }
  
  if (bytesReceived >= imageSize) {
    currentState = WAITING_FOR_HEADER;
    delay(300 * 1000);
    requestImage();
  }
}

void drawLine(uint16_t y) {
  for (uint16_t x = 0; x < IMAGE_WIDTH; x++) {
    uint16_t bufferIndex = x * 2;
    uint16_t color = ((uint16_t)lineBuffer[bufferIndex] << 8) | lineBuffer[bufferIndex + 1];
    tft.drawPixel(IMAGE_X + x, IMAGE_Y + y, color);
  }
}

void requestImage() {
  Serial.print("REQ\n");
}