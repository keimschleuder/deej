package main

import (
	"fmt"
	_ "image/png"
	"log"
	"os"
	"time"

	"github.com/go-ole/go-ole"
	"github.com/go-ole/go-ole/oleutil"
)

const (
	// Adjust these settings for your Arduino
	SERIAL_PORT = "COM9" // Change to your Arduino's COM port
	BAUD_RATE   = 115200

	// Image settings - adjust based on your display
	TARGET_WIDTH  = 100
	TARGET_HEIGHT = 100
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

	// Main loop
	for {
		trackInfo, artworkData, err := getCurrentTrackArtwork()
		if err != nil {
			log.Printf("Error getting track info: %v", err)
			time.Sleep(5 * time.Second)
			continue
		}

		// Check if track changed
		if trackInfo != lastTrackInfo {
			log.Printf("Now playing: %s - %s (%s)", trackInfo.Artist, trackInfo.Name, trackInfo.Album)

			if artworkData != nil {
				log.Println("Processing cover art...")
				log.Println("Sending coverart")
			} else {
				log.Println("No artwork available for this track")
			}

			lastTrackInfo = trackInfo
		}
		time.Sleep(2 * time.Second)
	}
}

func getCurrentTrackArtwork() (TrackInfo, []byte, error) {
	// Initialize COM
	ole.CoInitialize(0)
	defer ole.CoUninitialize()

	// Create iTunes COM object
	unknown, err := oleutil.CreateObject("iTunes.Application")
	if err != nil {
		return TrackInfo{}, nil, fmt.Errorf("failed to create iTunes object: %v", err)
	}
	defer unknown.Release()

	itunes, err := unknown.QueryInterface(ole.IID_IDispatch)
	if err != nil {
		return TrackInfo{}, nil, fmt.Errorf("failed to query interface: %v", err)
	}
	defer itunes.Release()

	// Get current track
	currentTrack, err := oleutil.GetProperty(itunes, "CurrentTrack")
	if err != nil {
		return TrackInfo{}, nil, fmt.Errorf("no track playing")
	}
	defer currentTrack.Clear()

	if currentTrack.VT == ole.VT_NULL || currentTrack.VT == ole.VT_EMPTY {
		return TrackInfo{}, nil, fmt.Errorf("no track playing")
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
		return trackInfo, nil, nil
	}
	defer artworks.Clear()

	artworkCollection := artworks.ToIDispatch()

	// Get artwork count
	count, err := oleutil.GetProperty(artworkCollection, "Count")
	if err != nil || count.Val == 0 {
		return trackInfo, nil, nil
	}

	// Get first artwork
	artwork, err := oleutil.GetProperty(artworkCollection, "Item", 1)
	if err != nil {
		return trackInfo, nil, nil
	}
	defer artwork.Clear()

	artworkObj := artwork.ToIDispatch()

	// Save artwork to temp file
	tempFile := `C:\Users\nikla\Documents\GitHub\deej\arduino\iTunes\itunes_artwork.jpg`
	_, err = oleutil.CallMethod(artworkObj, "SaveArtworkToFile", tempFile)
	if err != nil {
		return trackInfo, nil, fmt.Errorf("failed to save artwork: %v", err)
	}

	// Read the artwork file
	artworkData, err := os.ReadFile(tempFile)
	if err != nil {
		return trackInfo, nil, fmt.Errorf("failed to read artwork file: %v", err)
	}

	ole.CoUninitialize()

	return trackInfo, artworkData, nil
}
