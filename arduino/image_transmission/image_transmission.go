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
	time.Sleep(2 * time.Second)
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
				// Check for request command "REQ\x00"
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
	log.Printf("Reading image from: %s", imagePath)
	imageData, err := os.ReadFile(imagePath)
	if err != nil {
		return fmt.Errorf("failed to read image file: %v", err)
	}

	// Decode image
	img, format, err := image.Decode(bytes.NewReader(imageData))
	if err != nil {
		return fmt.Errorf("failed to decode image: %v", err)
	}
	log.Printf("Decoded %s image: %dx%d", format, img.Bounds().Dx(), img.Bounds().Dy())

	// Resize image
	log.Printf("Resizing to %dx%d...", TARGET_WIDTH, TARGET_HEIGHT)
	resized := resize.Resize(TARGET_WIDTH, TARGET_HEIGHT, img, resize.Lanczos3)

	// Convert to raw RGB data
	rgbData := imageToRGB(resized)
	log.Printf("RGB data size: %d bytes", len(rgbData))

	// Send header: "IMG\n"
	header := []byte{'I', 'M', 'G', '\n'}
	size := uint32(len(rgbData))
	header = append(header, byte(size>>24), byte(size>>16), byte(size>>8), byte(size))

	log.Println("Sending header...")
	_, err = port.Write(header)
	if err != nil {
		return fmt.Errorf("failed to write header: %v", err)
	}

	time.Sleep(50 * time.Millisecond)

	// Send RGB data in chunks (one line at a time for smooth display)
	bytesPerLine := TARGET_WIDTH * 3
	totalLines := TARGET_HEIGHT
	log.Printf("Sending %d lines of %d bytes each...", totalLines, bytesPerLine)

	for line := 0; line < totalLines; line++ {
		start := line * bytesPerLine
		end := start + bytesPerLine

		_, err = port.Write(rgbData[start:end])
		if err != nil {
			return fmt.Errorf("failed to write line %d: %v", line, err)
		}

		// Log progress every 8 lines
		if (line+1)%8 == 0 {
			progress := ((line + 1) * 100) / totalLines
			log.Printf("Progress: %d%% (%d/%d lines)", progress, line+1, totalLines)
		}

		// Small delay between lines
		time.Sleep(5 * time.Millisecond)
	}

	log.Println("All data sent!")
	return nil
}

func imageToRGB(img image.Image) []byte {
	bounds := img.Bounds()
	width := bounds.Dx()
	height := bounds.Dy()

	rgbData := make([]byte, width*height*3)
	index := 0

	for y := 0; y < height; y++ {
		for x := 0; x < width; x++ {
			r, g, b, _ := img.At(x, y).RGBA()
			// Convert from 16-bit to 8-bit
			rgbData[index] = uint8(r >> 8)
			rgbData[index+1] = uint8(g >> 8)
			rgbData[index+2] = uint8(b >> 8)
			index += 3
		}
	}

	return rgbData
}
