// target.go holds the facts about the one supported disc image.
package main

const (
	targetID = "scus-94221"
	// targetInput is the user-supplied raw Mode 2/2352 BIN at the project root.
	targetInput       = "scus-94221.bin"
	targetInputBytes  = 541315152
	targetInputSHA256 = "29e63bb6d2e7b842fe87f7c0ee03b7076681c34bcc47095cca27ec53236bf4f9"
	// targetOutput names the rebuilt image and cue sheet under build/disc.
	targetOutput = "output-scus-94221"
	// psxEXEHeaderSize is the header in front of SCUS_942.21's text.
	psxEXEHeaderSize = 0x800
	// defaultProfile is the compiler profile of ordinary game code.
	defaultProfile = "gcc-2.6.3_O2_aspsx-2.34"
)
