package main

import (
	"bufio"
	"flag"
	"fmt"
	"io"
	"log"
	"os"
	"runtime"
	"strconv"
	"strings"
	"syscall"
	"time"
	"unsafe"

	"github.com/go-ole/go-ole"
	"github.com/itchyny/volume-go"
	"github.com/jacobsa/go-serial/serial"
	"github.com/micmonay/keybd_event"
	"github.com/moutend/go-wca/pkg/wca"
	"github.com/spf13/viper"
	"golang.org/x/sys/windows"
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

var (
	user32 = windows.NewLazySystemDLL("user32.dll")
	psapi  = windows.NewLazySystemDLL("psapi.dll")

	procGetForegroundWindow      = user32.NewProc("GetForegroundWindow")
	procGetWindowThreadProcessId = user32.NewProc("GetWindowThreadProcessId")
	procGetModuleBaseNameW       = psapi.NewProc("GetModuleBaseNameW")

	kb            keybd_event.KeyBonding
	userConfig    *viper.Viper
	sliderMapping map[string]int
	verbose       bool
)

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
				case "deej.current":
					// Get process name
					processName, err := CurrentProcessName()
					if err != nil {
						log.Println(err)
					}
					go setApplicationVolume(processName, value)
				default:
					// If it ends with .exe, treat it as an application name
					if strings.HasSuffix(strings.ToLower(sliderName), ".exe") {
						go setApplicationVolume(sliderName, value)
					}
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

func CurrentProcessName() (string, error) {
	hwnd, _, err := procGetForegroundWindow.Call()
	if hwnd == 0 {
		return "", err
	}

	var pid uint32
	procGetWindowThreadProcessId.Call(hwnd, uintptr(unsafe.Pointer(&pid)))

	handle, err := windows.OpenProcess(
		windows.PROCESS_QUERY_INFORMATION|windows.PROCESS_VM_READ,
		false,
		pid,
	)
	if err != nil {
		return "", err
	}
	defer windows.CloseHandle(handle)

	buf := make([]uint16, windows.MAX_PATH)
	ret, _, err := procGetModuleBaseNameW.Call(
		uintptr(handle),
		0,
		uintptr(unsafe.Pointer(&buf[0])),
		uintptr(len(buf)),
	)
	if ret == 0 {
		return "", err
	}

	return syscall.UTF16ToString(buf), nil
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

func setMicrophoneVolume(percentage int) {
	runtime.LockOSThread()
	defer runtime.UnlockOSThread()

	ole.CoInitializeEx(0, ole.COINIT_APARTMENTTHREADED)
	defer ole.CoUninitialize()

	var err error
	var mmde *wca.IMMDeviceEnumerator

	if err = wca.CoCreateInstance(wca.CLSID_MMDeviceEnumerator, 0, wca.CLSCTX_ALL, wca.IID_IMMDeviceEnumerator, &mmde); err != nil {
		log.Printf("Error creating device enumerator: %v", err)
		return
	}
	if mmde != nil {
		defer mmde.Release()
	}

	var mmDevice *wca.IMMDevice
	if err = mmde.GetDefaultAudioEndpoint(wca.ECapture, wca.EConsole, &mmDevice); err != nil {
		log.Printf("Error getting default microphone: %v", err)
		return
	}
	if mmDevice != nil {
		defer mmDevice.Release()
	}

	var endpointVolume *wca.IAudioEndpointVolume
	if err = mmDevice.Activate(wca.IID_IAudioEndpointVolume, wca.CLSCTX_ALL, nil, &endpointVolume); err != nil {
		log.Printf("Error activating endpoint volume: %v", err)
		return
	}
	if endpointVolume != nil {
		defer endpointVolume.Release()
	}

	volumeScalar := float32(percentage) / 100.0

	if err = endpointVolume.SetMasterVolumeLevelScalar(volumeScalar, nil); err != nil {
		log.Printf("Error setting microphone volume to %d%%: %v", percentage, err)
	} else if verbose {
		fmt.Printf("[Microphone] Set to %d%%\n", percentage)
	}
}

func setApplicationVolume(processName string, percentage int) {
	runtime.LockOSThread()
	defer runtime.UnlockOSThread()

	ole.CoInitializeEx(0, ole.COINIT_APARTMENTTHREADED)
	defer ole.CoUninitialize()

	var err error
	var mmde *wca.IMMDeviceEnumerator

	if err = wca.CoCreateInstance(wca.CLSID_MMDeviceEnumerator, 0, wca.CLSCTX_ALL, wca.IID_IMMDeviceEnumerator, &mmde); err != nil {
		log.Printf("Error creating device enumerator: %v", err)
		return
	}
	if mmde != nil {
		defer mmde.Release()
	}

	var mmDevice *wca.IMMDevice
	if err = mmde.GetDefaultAudioEndpoint(wca.ERender, wca.EConsole, &mmDevice); err != nil {
		log.Printf("Error getting default audio endpoint: %v", err)
		return
	}
	if mmDevice != nil {
		defer mmDevice.Release()
	}

	var sessionManager *wca.IAudioSessionManager2
	if err = mmDevice.Activate(wca.IID_IAudioSessionManager2, wca.CLSCTX_ALL, nil, &sessionManager); err != nil {
		log.Printf("Error activating session manager: %v", err)
		return
	}
	if sessionManager != nil {
		defer sessionManager.Release()
	}

	var sessionEnumerator *wca.IAudioSessionEnumerator
	if err = sessionManager.GetSessionEnumerator(&sessionEnumerator); err != nil {
		log.Printf("Error getting session enumerator: %v", err)
		return
	}
	if sessionEnumerator != nil {
		defer sessionEnumerator.Release()
	}

	var sessionCount int
	if err = sessionEnumerator.GetCount(&sessionCount); err != nil {
		log.Printf("Error getting session count: %v", err)
		return
	}

	processNameLower := strings.ToLower(processName)
	found := false

	for i := 0; i < sessionCount; i++ {
		var sessionControl *wca.IAudioSessionControl
		if err = sessionEnumerator.GetSession(i, &sessionControl); err != nil {
			continue
		}
		if sessionControl == nil {
			continue
		}

		sessionControl2Dispatch, err := sessionControl.QueryInterface(wca.IID_IAudioSessionControl2)
		if err != nil {
			sessionControl.Release()
			continue
		}
		sessionControl2 := (*wca.IAudioSessionControl2)(unsafe.Pointer(sessionControl2Dispatch))

		var processId uint32
		if err = sessionControl2.GetProcessId(&processId); err != nil {
			sessionControl2Dispatch.Release()
			sessionControl.Release()
			continue
		}

		currentProcessName := getProcessName(processId)

		if strings.ToLower(currentProcessName) == processNameLower {
			simpleVolumeDispatch, err := sessionControl2.QueryInterface(wca.IID_ISimpleAudioVolume)
			if err != nil {
				sessionControl2Dispatch.Release()
				sessionControl.Release()
				continue
			}
			simpleVolume := (*wca.ISimpleAudioVolume)(unsafe.Pointer(simpleVolumeDispatch))

			volumeScalar := float32(percentage) / 100.0

			if err = simpleVolume.SetMasterVolume(volumeScalar, nil); err != nil {
				log.Printf("Error setting volume for %s: %v", processName, err)
			} else {
				if verbose {
					fmt.Printf("[App: %s] Set to %d%%\n", processName, percentage)
				}
				found = true
			}

			// release dispatch objects explicitly (in reverse order)
			simpleVolumeDispatch.Release()
			sessionControl2Dispatch.Release()
			sessionControl.Release()
			break
		}

		// not target -> release and continue
		sessionControl2Dispatch.Release()
		sessionControl.Release()
	}

	if !found && verbose {
		log.Printf("Application %s not found or not playing audio", processName)
	}
}

// getProcessName returns the executable name for a given process ID
func getProcessName(pid uint32) string {
	// Simple approach: read from /proc or use Windows API
	// For Windows, we'll use a simple approach with the process name
	// This is a placeholder - in production you'd use Windows API calls

	// For now, we'll use a simple system call
	return getProcessNameWindows(pid)
}

// getProcessNameWindows retrieves process name on Windows
func getProcessNameWindows(pid uint32) string {
	// Use Windows API to get process name
	// This requires additional Windows API calls
	// For simplicity, we'll use a basic approach

	kernel32 := syscall.NewLazyDLL("kernel32.dll")
	openProcess := kernel32.NewProc("OpenProcess")
	queryFullProcessImageName := kernel32.NewProc("QueryFullProcessImageNameW")
	closeHandle := kernel32.NewProc("CloseHandle")

	// Open process with QUERY_LIMITED_INFORMATION access
	handle, _, _ := openProcess.Call(
		0x1000, // PROCESS_QUERY_LIMITED_INFORMATION
		0,
		uintptr(pid),
	)

	if handle == 0 {
		return ""
	}
	defer closeHandle.Call(handle)

	// Get process image name
	var size uint32 = 260 // MAX_PATH
	buffer := make([]uint16, size)

	ret, _, _ := queryFullProcessImageName.Call(
		handle,
		0, // Win32 path format
		uintptr(unsafe.Pointer(&buffer[0])),
		uintptr(unsafe.Pointer(&size)),
	)

	if ret == 0 {
		return ""
	}

	// Convert to string and extract just the filename
	fullPath := syscall.UTF16ToString(buffer[:size])
	parts := strings.Split(fullPath, "\\")
	if len(parts) > 0 {
		return parts[len(parts)-1]
	}

	return ""
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
