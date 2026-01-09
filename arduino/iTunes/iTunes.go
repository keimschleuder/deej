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
	"strings"
	"time"
	"unicode"
	"unicode/utf8"

	"github.com/go-ole/go-ole"
	"github.com/go-ole/go-ole/oleutil"
	"github.com/jacobsa/go-serial/serial"
	"github.com/nfnt/resize"
	"golang.org/x/text/runes"
	"golang.org/x/text/transform"
	"golang.org/x/text/unicode/norm"
)

const (
	SERIAL_PORT = "COM9"
	BAUD_RATE   = 115200

	TARGET_WIDTH  = 100
	TARGET_HEIGHT = 100

	imagePath = `C:\Users\nikla\Documents\GitHub\deej\arduino\iTunes\itunes_artwork.jpg`
)

type TrackInfo struct {
	Name   string
	Artist string
	Album  string
}

func main() {
	log.Println("iTunes Cover Art to Arduino Serial Transmitter")
	log.Println("==============================================")

	var lastTrackInfo TrackInfo

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
				if requestBuffer[0] == 'R' && requestBuffer[1] == 'E' && requestBuffer[2] == 'Q' && requestBuffer[3] == '\n' {
					log.Println("\n=== Image requested by Arduino ===")
					trackInfo, err := getCurrentTrackArtwork()
					if trackInfo != lastTrackInfo {
						lastTrackInfo = trackInfo
					}
					log.Println("Got Track Data")
					err = sendImage(port)
					if err != nil {
						log.Printf("Error sending image: %v", err)
					} else {
						log.Println("Image sent successfully!")
					}

					title, artist := processTrackInfo(trackInfo.Name, trackInfo.Artist)
					serialMessage := title + "\t" + artist + "\n"
					_, err = port.Write([]byte(serialMessage))
					if err != nil {
						log.Printf("Error sending image: %v", err)
					} else {
						log.Println("Trackdata sent successfully!")
					}
				}
				requestIndex = 0
			}
		}
	}
}

func processTrackInfo(title string, artist string) (string, string) {
	title = strings.TrimSpace(title)
	artist = strings.TrimSpace(artist)
	if strings.Index(title, "(") > 5 {
		title = title[0:strings.Index(title, "(")]
	}
	if strings.Index(title, "-") > 12 {
		title = title[0:strings.Index(title, "-")]
	}
	if utf8.RuneCountInString(title) > 52 {
		title = title[0:50] + ".."
	}

	if utf8.RuneCountInString(artist) > 26 {
		artist = artist[0:24] + ".."
	}

	title = RemoveSpecialChars(title)
	artist = RemoveSpecialChars(artist)

	return title, artist
}

func RemoveSpecialChars(s string) string {
	// First, handle German umlauts specifically
	replacer := strings.NewReplacer(
		"ä", "a", "Ä", "A",
		"ö", "o", "Ö", "O",
		"ü", "u", "Ü", "U",
		"ß", "ss",
	)
	s = replacer.Replace(s)

	// Then remove accents from other characters
	t := transform.Chain(norm.NFD, runes.Remove(runes.In(unicode.Mn)), norm.NFC)
	result, _, _ := transform.String(t, s)

	return result
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

	// Send size as 4 bytes
	size := uint32(len(rgb565Data))
	sizeBytes := []byte{byte(size >> 24), byte(size >> 16), byte(size >> 8), byte(size)}
	log.Println("Sending size...")

	_, err = port.Write(sizeBytes)
	if err != nil {
		return fmt.Errorf("failed to write size: %v", err)
	}

	log.Println("Sending Image data")

	_, err = port.Write(rgb565Data)
	if err != nil {
		return fmt.Errorf("Failed to Send the Image Data: %v", err)
	}
	log.Println("All Data sent")

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
			rgb565 := uint16((r8&0xF8)<<8) | uint16((g8&0xFC)<<3) | uint16(b8>>3)

			// Send as big-endian (high byte first)
			rgb565Data[index] = uint8(rgb565 >> 8)
			rgb565Data[index+1] = uint8(rgb565 & 0xFF)
			index += 2
		}
	}

	return rgb565Data
}

func getCurrentTrackArtwork() (TrackInfo, error) {
	// Initialize COM
	ole.CoInitialize(0)
	defer ole.CoUninitialize()

	// Create iTunes COM object
	unknown, err := oleutil.CreateObject("iTunes.Application")
	if err != nil {
		return TrackInfo{}, fmt.Errorf("failed to create iTunes object: %v", err)
	}
	defer unknown.Release()

	itunes, err := unknown.QueryInterface(ole.IID_IDispatch)
	if err != nil {
		return TrackInfo{}, fmt.Errorf("failed to query interface: %v", err)
	}
	defer itunes.Release()

	// Get current track
	currentTrack, err := oleutil.GetProperty(itunes, "CurrentTrack")
	if err != nil {
		return TrackInfo{}, fmt.Errorf("no track playing")
	}
	defer currentTrack.Clear()

	if currentTrack.VT == ole.VT_NULL || currentTrack.VT == ole.VT_EMPTY {
		return TrackInfo{}, fmt.Errorf("no track playing")
	}

	track := currentTrack.ToIDispatch()

	// Get track info
	trackInfo := TrackInfo{}
	if name, err := oleutil.GetProperty(track, "Name"); err == nil {
		trackInfo.Name = name.ToString()
	}
	if artist, err := oleutil.GetProperty(track, "Artist"); err == nil {
		trackInfo.Artist = artist.ToString()
	}
	if album, err := oleutil.GetProperty(track, "Album"); err == nil {
		trackInfo.Album = album.ToString()
	}

	// Get artwork
	artworks, err := oleutil.GetProperty(track, "Artwork")
	if err != nil {
		return trackInfo, nil
	}
	defer artworks.Clear()

	artworkCollection := artworks.ToIDispatch()

	// Get artwork count
	count, err := oleutil.GetProperty(artworkCollection, "Count")
	if err != nil || count.Val == 0 {
		return trackInfo, nil
	}

	// Get first artwork
	artwork, err := oleutil.GetProperty(artworkCollection, "Item", 1)
	if err != nil {
		return trackInfo, nil
	}
	defer artwork.Clear()

	artworkObj := artwork.ToIDispatch()

	// Save artwork to temp file
	_, err = oleutil.CallMethod(artworkObj, "SaveArtworkToFile", imagePath)
	if err != nil {
		return trackInfo, fmt.Errorf("failed to save artwork: %v", err)
	}

	ole.CoUninitialize()

	return trackInfo, nil
}
