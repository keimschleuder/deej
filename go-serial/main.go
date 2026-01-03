package main

import (
	"bufio"
	"fmt"
	"io"
	"log"
	"os"
	"strconv"
	"strings"
	"time"

	"github.com/jacobsa/go-serial/serial"
)

func main() {
	options := serial.OpenOptions{
		PortName:        "COM9",
		BaudRate:        9600,
		DataBits:        8,
		StopBits:        1,
		MinimumReadSize: 1,
	}

	port, err := serial.Open(options)
	if err != nil {
		log.Fatalf("Failed to open port: %v", err)
	}
	defer port.Close()

	fmt.Println("Connected to Arduino on COM9")
	time.Sleep(2 * time.Second)

	// Start goroutine to continuously read from Arduino
	go readFromArduino(port)

	// Main goroutine handles user input
	sendToArduino(port)
}

// Continuously read from Arduino in background
func readFromArduino(port io.ReadWriteCloser) {
	reader := bufio.NewReader(port)

	for {
		line, err := reader.ReadString('\n')
		if err != nil {
			log.Printf("Error reading: %v", err)
			time.Sleep(100 * time.Millisecond)
			continue
		}

		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		fmt.Printf("\n[Arduino] %s\n", line)
		parseArduinoData(line)
		fmt.Print("Enter percentage (0-100) or 'q' to quit: ")
	}
}

// Handle user input and send to Arduino
func sendToArduino(port io.ReadWriteCloser) {
	reader := bufio.NewReader(os.Stdin)

	for {
		fmt.Print("Enter percentage (0-100) or 'q' to quit: ")
		input, err := reader.ReadString('\n')
		if err != nil {
			log.Printf("Error reading input: %v", err)
			continue
		}

		input = strings.TrimSpace(input)

		if input == "q" || input == "quit" {
			fmt.Println("Exiting...")
			return
		}

		percentage, err := strconv.Atoi(input)
		if err != nil {
			fmt.Println("Invalid input. Please enter a number.")
			continue
		}

		if percentage < 0 || percentage > 100 {
			fmt.Println("Percentage must be between 0 and 100")
			continue
		}

		// Send to Arduino
		message := fmt.Sprintf("%d\n", percentage)
		_, err = port.Write([]byte(message))
		if err != nil {
			log.Printf("Error sending: %v", err)
		} else {
			fmt.Printf("Sent: %d%%\n", percentage)
		}
	}
}

func parseArduinoData(data string) {
	parts := strings.Split(data, "|")

	for _, part := range parts {
		if len(part) < 4 {
			continue
		}

		switch part[0] {
		case 's':
			var sliderNum, value int
			fmt.Sscanf(part, "s%dv%d", &sliderNum, &value)
			fmt.Printf("  → Slider %d moved to %d%%\n", sliderNum, value)
		case 'b':
			var buttonNum, value int
			fmt.Sscanf(part, "b%dv%d", &buttonNum, &value)
			fmt.Printf("  → Button %d pressed: %v\n", buttonNum, value == 1)
		}
	}
}
