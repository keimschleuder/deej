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

void debugPrint(const char* msg) {
  tft.fillRect(0, 110, 160, 18, ST77XX_BLACK);
  tft.setCursor(0, 110);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(1);
  tft.print(msg);
}

void debugPrintNum(const char* msg, uint32_t num) {
  tft. fillRect(0, 110, 160, 18, ST77XX_BLACK);
  tft. setCursor(0, 110);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(1);
  tft. print(msg);
  tft.print(num);
}

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
  // debugPrint("Loop");
  if (Serial.available() > 0) {
    // debugPrint("Serial Started");
    uint8_t inByte = Serial.read();
    // debugPrint("Daten gelesen");
    
    // Zeige empfangenes Byte an
    tft.fillRect(0, 110, 160, 18, ST77XX_BLACK);
    tft.setCursor(0, 110);
    tft.setTextColor(ST77XX_GREEN);
    tft.setTextSize(1);
    tft.print("Byte:  0x");
    tft.print(inByte, HEX);
    tft.print(" '");
    tft.print((char)inByte);
    tft.print("'");
    
    delay(500);  // Halbe Sekunde warten, damit du es lesen kannst

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
  debugPrint("Receiving Header");
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
    debugPrint("Receiving...");
  }
}

void handleImageData(uint8_t inByte) {
  lineBufferIndex++;
  lineBuffer[lineBufferIndex] = inByte;
  bytesReceived++;
  
  if (lineBufferIndex >= IMAGE_WIDTH * 2) {
    debugPrint("Drawing Line");
    drawLine(currentLine);
    
    // Debug: Show first line's pixel values
    if (currentLine == 0) {
      tft.fillRect(0, 100, 160, 10, ST77XX_BLACK);
      tft.setCursor(0, 100);
      tft.setTextColor(ST77XX_GREEN);
      tft.setTextSize(1);
      
      // Show first few bytes received
      tft.print("B:");
      for (int i = 0; i < 8; i++) {
        tft.print(lineBuffer[i], HEX);
        tft.print(" ");
      }
    }
    
    // Debug: Show first pixel color on line 1
    if (currentLine == 1) {
      uint16_t firstColor = ((uint16_t)lineBuffer[0] << 8) | lineBuffer[1];
      debugPrintNum("Color0:  0x", firstColor);
      
      // Also draw a test rectangle with this color
      tft.fillRect(0, 0, 20, 20, firstColor);
    }
    
    currentLine++;
    lineBufferIndex = 0;
  }
  
  if (bytesReceived >= imageSize) {
    debugPrint("Done!");
    currentState = WAITING_FOR_HEADER;
    
    delay(5000);
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
  debugPrint("Sending REQ...");
  Serial.print("REQ\n");
}