package main

import (
	"encoding/binary"
	"math"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"testing"
)

func readSuzukiPitchInitializer(t *testing.T) []byte {
	t.Helper()
	data, err := os.ReadFile(filepath.Join("..", "src", "lib", "suzuki", "suzuki_pitch.inc"))
	if err != nil {
		t.Fatal(err)
	}
	return data
}

func TestSuzukiPitchSourceMatchesFormulaAndOriginalTable(t *testing.T) {
	source := string(readSuzukiPitchInitializer(t))
	commentEnd := strings.Index(source, "*/")
	if commentEnd < 0 {
		t.Fatal("missing pitch table explanation")
	}
	values := strings.Split(strings.TrimSuffix(strings.TrimSpace(source[commentEnd+2:]), ","), ",")
	if len(values) != 12*256 {
		t.Fatalf("pitch table has %d entries, want 3072", len(values))
	}
	data := make([]byte, len(values)*2)
	for i, literal := range values {
		value, err := strconv.ParseUint(strings.TrimSpace(literal), 10, 16)
		if err != nil {
			t.Fatalf("pitch %d: %v", i, err)
		}
		want := uint16(math.Floor(8192*math.Exp2(float64(i)/3072) + 0.5))
		if uint16(value) != want {
			t.Fatalf("pitch %d: source %d, formula %d", i, value, want)
		}
		binary.LittleEndian.PutUint16(data[2*i:], uint16(value))
	}
	// Independently recovered original bytes at MAIN 0x800290d8.
	if shortHash(data) != "d3f07a5746f7a2fb" {
		t.Fatalf("pitch table: %d bytes, hash %s", len(data), shortHash(data))
	}
}

func TestSuzukiPitchCompilesFromMirroredSources(t *testing.T) {
	requirePinnedToolchain(t)
	t.Setenv("TOOLS_CACHE", "0")
	t.Setenv("TOOLS_WORK_DIR", t.TempDir())
	p := project{root: t.TempDir()}
	writeTestFile(t, filepath.Join(p.root, "src", "pitch.c"), []byte("unsigned short pitches[3072] = {\n#include \"suzuki_pitch.inc\"\n};\n"))
	writeTestFile(t, filepath.Join(p.root, "src", "suzuki_pitch.inc"), readSuzukiPitchInitializer(t))
	writeTestFile(t, filepath.Join(p.root, "include", "empty.h"), nil)
	p, err := p.withInputMirror()
	if err != nil {
		t.Fatal(err)
	}
	link := filepath.Join(p.root, "pitch.ld")
	writeTestFile(t, link, []byte("OUTPUT_ARCH(mips)\nSECTIONS { .data 0x800290d8 : SUBALIGN(4) { *(.data) } .text : { *(.text) } /DISCARD/ : { *(.reginfo) *(.MIPS.abiflags) *(.pdr) } }\n"))
	profile, ok := compilerProfileNamed("gcc-2.6.3_O2_aspsx-2.34")
	if !ok {
		t.Fatal("missing Suzuki compiler profile")
	}
	sections, err := p.compileHistoricalSections(filepath.Join(p.root, "src", "pitch.c"), link, filepath.Join(p.root, "work"), profile, "-DFFT_MAIN")
	if err != nil {
		t.Fatal(err)
	}
	if len(sections[".text"]) != 0 || len(sections[".data"]) != 6144 || shortHash(sections[".data"]) != "d3f07a5746f7a2fb" {
		t.Fatalf("source pitch table: text %d, data %d, hash %s", len(sections[".text"]), len(sections[".data"]), shortHash(sections[".data"]))
	}
}
