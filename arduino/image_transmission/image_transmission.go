package main

import (
	"bytes"
	"fmt"
	"image"
	_ "image/jpeg"
	_ "image/png"
	"io"
	"log"
	"os"
	"time"

	"github.com/jacobsa/go-serial/serial"
	"github.com/nfnt/resize"
)

const (
	SERIAL_PORT = "COM9"
	BAUD_RATE   = 115200

	TARGET_WIDTH  = 100
	TARGET_HEIGHT = 100

	imagePath = `C:\Users\nikla\Documents\GitHub\deej\arduino\image_transmission\image.jpg`
)

func main() {
	log.Println("Image Server - Waiting for Arduino requests")
	log.Println("============================================")

	// Configure serial port
	options := serial.OpenOptions{
		PortName:        SERIAL_PORT,
		BaudRate:        BAUD_RATE,
		DataBits:        8,
		StopBits:        1,
		MinimumReadSize: 1,
	}

	// Open serial port
	port, err := serial.Open(options)
	if err != nil {
		log.Fatalf("Failed to open port: %v", err)
	}
	defer port.Close()

	// Wait for Arduino to reset
	time.Sleep(time.Second)
	log.Printf("Connected to Arduino on %s at %d baud\n", SERIAL_PORT, BAUD_RATE)

	// Flush any initial data
	buf := make([]byte, 1024)
	for {
		n, err := port.Read(buf)
		if err != nil || n == 0 {
			break
		}
		time.Sleep(10 * time.Millisecond)
	}

	log.Println("Listening for image requests...")

	// Listen for requests from Arduino
	requestBuffer := make([]byte, 4)
	requestIndex := 0

	for {
		n, err := port.Read(buf)
		if err != nil {
			log.Printf("Read error: %v", err)
			time.Sleep(100 * time.Millisecond)
			continue
		}

		for i := 0; i < n; i++ {
			requestBuffer[requestIndex] = buf[i]
			requestIndex++

			if requestIndex >= 4 {
				// Check for request command "REQ\n"
				if requestBuffer[0] == 'R' && requestBuffer[1] == 'E' &&
					requestBuffer[2] == 'Q' && requestBuffer[3] == '\n' {
					log.Println("\n=== Image requested by Arduino ===")
					err := sendImage(port)
					if err != nil {
						log.Printf("Error sending image: %v", err)
					} else {
						log.Println("Image sent successfully!")
					}
				}
				requestIndex = 0
			}
		}
	}
}

func sendImage(port io.ReadWriteCloser) error {
	// Read the image file
	log.Printf("Reading image from:  %s", imagePath)
	imageData, err := os.ReadFile(imagePath)
	if err != nil {
		return fmt.Errorf("failed to read image file: %v", err)
	}

	// Decode image
	img, format, err := image.Decode(bytes.NewReader(imageData))
	if err != nil {
		return fmt.Errorf("failed to decode image: %v", err)
	}
	log.Printf("Decoded %s image:  %dx%d", format, img.Bounds().Dx(), img.Bounds().Dy())

	// Resize image
	log.Printf("Resizing to %dx%d.. .", TARGET_WIDTH, TARGET_HEIGHT)
	resized := resize.Resize(TARGET_WIDTH, TARGET_HEIGHT, img, resize.Lanczos3)

	// Convert to RGB565 data
	rgb565Data := imageToRGB565(resized)
	log.Printf("RGB565 data size:  %d bytes", len(rgb565Data))

	// Send header: "IMG\n"
	header := []byte{'I', 'M', 'G', '\n'}
	log.Println("Sending header...")
	_, err = port.Write(header)
	if err != nil {
		return fmt.Errorf("failed to write header: %v", err)
	}

	time.Sleep(100 * time.Millisecond)

	// Send size as 4 bytes
	size := uint32(len(rgb565Data))
	sizeBytes := []byte{byte(size >> 24), byte(size >> 16), byte(size >> 8), byte(size)}
	log.Println("Sending size...")

	n, err := port.Write(sizeBytes)
	log.Printf("Sent %d size bytes:  %v", n, sizeBytes)
	if err != nil {
		return fmt.Errorf("failed to write header: %v", err)
	}

	time.Sleep(50 * time.Millisecond)

	// TODO: Send Image

	return nil
}

func imageToRGB565(img image.Image) []byte {
	bounds := img.Bounds()
	width := bounds.Dx()
	height := bounds.Dy()

	// 2 bytes per pixel for RGB565
	rgb565Data := make([]byte, width*height*2)
	index := 0

	for y := 0; y < height; y++ {
		for x := 0; x < width; x++ {
			r, g, b, _ := img.At(x, y).RGBA()

			// Convert from 16-bit to 8-bit
			r8 := uint16(r >> 8)
			g8 := uint16(g >> 8)
			b8 := uint16(b >> 8)

			// Convert RGB888 to RGB565
			// RGB565:  RRRRRGGGGGGBBBBB
			rgb565 := uint16((b8&0xF8)<<8) | uint16((g8&0xFC)<<3) | uint16(r8>>3)

			// Send as big-endian (high byte first)
			rgb565Data[index] = uint8(rgb565 >> 8)
			rgb565Data[index+1] = uint8(rgb565 & 0xFF)
			index += 2
		}
	}

	return rgb565Data
}
