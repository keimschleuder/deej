package main

import (
	"bufio"
	"flag"
	"fmt"
	"io"
	"log"
	"os"
	"strconv"
	"strings"
	"time"

	"github.com/go-ole/go-ole"
	"github.com/itchyny/volume-go"
	"github.com/jacobsa/go-serial/serial"
	"github.com/micmonay/keybd_event"
	"github.com/moutend/go-wca/pkg/wca"
	"github.com/spf13/viper"
)

type ArduinoMessage struct {
	Timestamp    time.Time
	SliderValues map[int]int
	ButtonStates map[int]bool
}

const (
	configName             = "config"
	configType             = "yaml"
	configPath             = "."
	configKeySliderMapping = "slider_mapping"
	configKeyButtonMapping = "button_mapping"
	configKeyCOMPort       = "com_port"
	configKeyBaudRate      = "baud_rate"
	defaultCOMPort         = "COM9"
	defaultBaudRate        = 9600
)

var kb keybd_event.KeyBonding
var userConfig *viper.Viper
var sliderMapping map[string]int // maps slider names (like "master") to slider numbers
var verbose bool                 // verbose mode flag

func main() {
	verboseFlag := flag.Bool("verbose", false, "Enable verbose output (shows all messages)")
	flag.Parse()
	verbose = *verboseFlag

	// Initialize COM for Windows Audio
	ole.CoInitializeEx(0, ole.COINIT_APARTMENTTHREADED)
	defer ole.CoUninitialize()

	// Initialize configuration
	var err error
	userConfig, err = initializeConfig()
	if err != nil {
		log.Fatalf("Failed to initialize config: %v", err)
	}

	// Build slider mapping (reverse lookup: name -> number)
	sliderMapping = buildSliderMapping()

	// Initialize keyboard
	kb, err = keybd_event.NewKeyBonding()
	if err != nil {
		log.Fatalf("Failed to initialize keyboard: %v", err)
	}

	// Get config values
	comPort := userConfig.GetString(configKeyCOMPort)
	baudRate := userConfig.GetUint(configKeyBaudRate)

	// Configure serial port
	options := serial.OpenOptions{
		PortName:        comPort,
		BaudRate:        baudRate,
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

	time.Sleep(2 * time.Second)
	fmt.Printf("Connected to Arduino on %s at %d baud\n", comPort, baudRate)

	// Channel for Arduino messages
	msgChan := make(chan ArduinoMessage, 10)

	// Start reading from Arduino in background
	go readFromArduino(port, msgChan)

	// Start processing received messages (always run to drain channel)
	go processMessages(msgChan)

	// Main loop: handle user input
	handleUserInput(port)
}

// initializeConfig creates and configures a viper instance for the config file
func initializeConfig() (*viper.Viper, error) {
	config := viper.New()
	config.SetConfigName(configName)
	config.SetConfigType(configType)
	config.AddConfigPath(configPath)

	// Set defaults
	config.SetDefault(configKeySliderMapping, map[int]string{})
	config.SetDefault(configKeyButtonMapping, map[int]int{})
	config.SetDefault(configKeyCOMPort, defaultCOMPort)
	config.SetDefault(configKeyBaudRate, defaultBaudRate)

	// Read config file
	if err := config.ReadInConfig(); err != nil {
		return nil, fmt.Errorf("failed to read config file: %w", err)
	}

	if verbose {
		fmt.Printf("Loaded config from: %s\n", config.ConfigFileUsed())
	}
	return config, nil
}

// buildSliderMapping creates a reverse lookup map from slider names to numbers
func buildSliderMapping() map[string]int {
	mapping := make(map[string]int)
	sliderMap := userConfig.GetStringMap(configKeySliderMapping)

	for key, value := range sliderMap {
		sliderNum, err := strconv.Atoi(key)
		if err != nil {
			continue
		}
		if sliderName, ok := value.(string); ok {
			mapping[sliderName] = sliderNum
			if verbose {
				fmt.Printf("Slider %d -> %s\n", sliderNum, sliderName)
			}
		} else {
			fmt.Println("Something went wrong during slider mapping")
		}
	}

	return mapping
}

// Continuously read from Arduino
func readFromArduino(port io.ReadWriteCloser, msgChan chan<- ArduinoMessage) {
	reader := bufio.NewReader(port)

	for {
		line, err := reader.ReadString('\n')
		if err != nil {
			log.Printf("Error reading from Arduino: %v", err)
			time.Sleep(100 * time.Millisecond)
			continue
		}

		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		// Parse the message
		if strings.HasPrefix(line, "OK:") || strings.HasPrefix(line, "ERROR:") {
			if verbose {
				fmt.Printf("[Arduino Response] %s\n", line)
			}
		} else if line == "Arduino ready" {
			if verbose {
				fmt.Println("[Arduino] Ready!")
			}
		} else if line == "PONG" {
			fmt.Println("[Arduino] PONG received")
		} else {
			// Parse sensor data: s0v75|b1v1
			msg := parseArduinoData(line)
			if len(msg.SliderValues) > 0 || len(msg.ButtonStates) > 0 {
				msgChan <- msg
			}
		}
	}
}

// Parse Arduino data format: s0v75|b1v1
func parseArduinoData(data string) ArduinoMessage {
	msg := ArduinoMessage{
		Timestamp:    time.Now(),
		SliderValues: make(map[int]int),
		ButtonStates: make(map[int]bool),
	}

	parts := strings.Split(data, "|")
	for _, part := range parts {
		if len(part) < 4 {
			continue
		}

		switch part[0] {
		case 's':
			// Slider: s0v75
			var sliderNum, value int
			n, err := fmt.Sscanf(part, "s%dv%d", &sliderNum, &value)
			if err == nil && n == 2 {
				msg.SliderValues[sliderNum] = value

				// Check slider name and route to appropriate volume control
				sliderName := getSliderName(sliderNum)
				switch sliderName {
				case "master":
					go setSystemVolume(value)
				case "mic":
					go setMicrophoneVolume(value)
				}
			}
		case 'b':
			// Button: b1v1
			var buttonNum, value int
			n, err := fmt.Sscanf(part, "b%dv%d", &buttonNum, &value)
			if err == nil && n == 2 {
				msg.ButtonStates[buttonNum] = (value == 1)

				// Send button press to Windows if button is pressed
				if value == 1 {
					buttonMapping := userConfig.GetStringMap(configKeyButtonMapping)
					if keyCodeVal, exists := buttonMapping[strconv.Itoa(buttonNum)]; exists {
						if keyCode, ok := keyCodeVal.(int); ok {
							go sendKeyPress(keyCode)
						}
					}
				}
			}
		}
	}

	return msg
}

// getSliderName returns the name mapped to a slider number
func getSliderName(sliderNum int) string {
	sliderMap := userConfig.GetStringMap(configKeySliderMapping)
	if nameVal, exists := sliderMap[strconv.Itoa(sliderNum)]; exists {
		if name, ok := nameVal.(string); ok {
			return name
		}
	}
	return ""
}

// setSystemVolume sets the Windows system volume (0-100)
func setSystemVolume(percentage int) {
	err := volume.SetVolume(percentage)
	if err != nil {
		log.Printf("Error setting volume to %d%%: %v", percentage, err)
	} else if verbose {
		fmt.Printf("[Volume] Set to %d%%\n", percentage)
	}
}

// setMicrophoneVolume sets the Windows microphone input volume (0-100)
func setMicrophoneVolume(percentage int) {
	// Initialize COM for this goroutine
	ole.CoInitializeEx(0, ole.COINIT_APARTMENTTHREADED)
	defer ole.CoUninitialize()

	var err error
	var mmde *wca.IMMDeviceEnumerator

	// Create device enumerator
	if err = wca.CoCreateInstance(wca.CLSID_MMDeviceEnumerator, 0, wca.CLSCTX_ALL, wca.IID_IMMDeviceEnumerator, &mmde); err != nil {
		log.Printf("Error creating device enumerator: %v", err)
		return
	}
	defer mmde.Release()

	// Get default audio capture device (microphone)
	var mmDevice *wca.IMMDevice
	if err = mmde.GetDefaultAudioEndpoint(wca.ECapture, wca.EConsole, &mmDevice); err != nil {
		log.Printf("Error getting default microphone: %v", err)
		return
	}
	defer mmDevice.Release()

	// Activate endpoint volume interface
	var endpointVolume *wca.IAudioEndpointVolume
	if err = mmDevice.Activate(wca.IID_IAudioEndpointVolume, wca.CLSCTX_ALL, nil, &endpointVolume); err != nil {
		log.Printf("Error activating endpoint volume: %v", err)
		return
	}
	defer endpointVolume.Release()

	// Convert percentage (0-100) to scalar (0.0-1.0)
	volumeScalar := float32(percentage) / 100.0

	// Set the microphone volume
	if err = endpointVolume.SetMasterVolumeLevelScalar(volumeScalar, nil); err != nil {
		log.Printf("Error setting microphone volume to %d%%: %v", percentage, err)
	} else if verbose {
		fmt.Printf("[Microphone] Set to %d%%\n", percentage)
	}
}

// Send key press to Windows
func sendKeyPress(keyCode int) {
	kb.SetKeys(keyCode)
	err := kb.Launching()
	if err != nil {
		log.Printf("Error sending key press %d: %v", keyCode, err)
	} else if verbose {
		fmt.Printf("[Key Press] Sent key code: %d\n", keyCode)
	}
	time.Sleep(50 * time.Millisecond)
	kb.Clear()
}

// Process incoming messages from Arduino
func processMessages(msgChan <-chan ArduinoMessage) {
	for msg := range msgChan {
		if verbose {
			fmt.Printf("\n[%s] Arduino Update:\n", msg.Timestamp.Format("15:04:05"))

			for slider, value := range msg.SliderValues {
				fmt.Printf("  → Slider %d: %d%%\n", slider, value)
			}

			for button, state := range msg.ButtonStates {
				if state {
					fmt.Printf("  → Button %d: PRESSED\n", button)
				}
			}
		}
	}
}

// Handle user input and send commands to Arduino
func handleUserInput(port io.ReadWriteCloser) {
	reader := bufio.NewReader(os.Stdin)

	for {
		input, err := reader.ReadString('\n')
		if err != nil {
			log.Printf("Error reading input: %v", err)
			continue
		}

		input = strings.TrimSpace(input)
		if input == "" {
			continue
		}

		// Parse command
		parts := strings.Fields(input)
		cmd := strings.ToLower(parts[0])

		switch cmd {
		case "set":
			// set 0 75 (set slider 0 to 75%)
			if len(parts) == 3 {
				slider, err1 := strconv.Atoi(parts[1])
				percentage, err2 := strconv.Atoi(parts[2])
				if err1 != nil || err2 != nil || percentage < 0 || percentage > 100 {
					fmt.Println("Invalid parameters. Usage: set <slider> <percentage>")
					continue
				}
				sendCommand(port, fmt.Sprintf("SET:%d:%d", slider, percentage))
				parseArduinoData(fmt.Sprintf("s%dv%d", slider, percentage))
			} else {
				fmt.Println("Usage: set <slider> <percentage>")
			}

		case "ping":
			sendCommand(port, "PING")

		case "help":
			printHelp()

		case "quit", "exit", "q":
			fmt.Println("Exiting...")
			return

		default:
			fmt.Printf("Unknown command: %s\n", cmd)
			fmt.Println("Type 'help' for available commands.")
		}
	}
}

// Send command to Arduino
func sendCommand(port io.ReadWriteCloser, command string) {
	message := command + "\n"
	n, err := port.Write([]byte(message))
	if err != nil {
		log.Printf("Error sending command: %v", err)
		return
	}

	if verbose {
		fmt.Printf("[Sent %d bytes] %s\n", n, strings.TrimSpace(command))
	}
}

// Print available commands
func printHelp() {
	fmt.Println("\n=== Available Commands ===")
	fmt.Println("  set <slider> <percentage>  - Set specific slider to percentage")
	fmt.Println("  ping                       - Ping Arduino")
	fmt.Println("  help                       - Show this help")
	fmt.Println("  quit/exit/q                - Exit program")
	fmt.Println("\nSlider Mapping:")
	for name, num := range sliderMapping {
		fmt.Printf("  Slider %d -> %s\n", num, name)
	}
	fmt.Println("========================")
}
