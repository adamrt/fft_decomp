package main

import (
	"bytes"
	"encoding/binary"
	"os"
	"path/filepath"
	"testing"
)

func TestSuzukiPitchFormulaMatchesOriginalTable(t *testing.T) {
	values := suzukiPitchTable()
	data := make([]byte, len(values)*2)
	for i, value := range values {
		binary.LittleEndian.PutUint16(data[2*i:], value)
	}
	// Independently recovered original bytes at MAIN 0x800290d8.
	if len(data) != 6144 || shortHash(data) != "d3f07a5746f7a2fb" {
		t.Fatalf("pitch formula: %d bytes, hash %s", len(data), shortHash(data))
	}
}

func TestGeneratedTablesRepairStaleOutputWithoutOriginalFiles(t *testing.T) {
	p := project{root: t.TempDir()}
	if err := p.generateTables(); err != nil {
		t.Fatal(err)
	}
	path := filepath.Join(p.root, "build", "generated", "suzuki_pitch.inc")
	want, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	before, err := os.Stat(path)
	if err != nil {
		t.Fatal(err)
	}
	if err := p.generateTables(); err != nil {
		t.Fatal(err)
	}
	after, err := os.Stat(path)
	if err != nil || !os.SameFile(before, after) || !before.ModTime().Equal(after.ModTime()) {
		t.Fatal("unchanged generated table was rewritten", err)
	}
	if err := os.WriteFile(path, []byte("stale output"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := p.generateTables(); err != nil {
		t.Fatal(err)
	}
	got, err := os.ReadFile(path)
	if err != nil || !bytes.Equal(got, want) {
		t.Fatal("stale generated table was not repaired", err)
	}
}

func TestGeneratedPitchCompilesFromMirroredSources(t *testing.T) {
	requirePinnedToolchain(t)
	t.Setenv("TOOLS_CACHE", "0")
	t.Setenv("TOOLS_WORK_DIR", t.TempDir())
	p := project{root: t.TempDir()}
	writeTestFile(t, filepath.Join(p.root, "src", "pitch.c"), []byte("unsigned short pitches[3072] = {\n#include \"suzuki_pitch.inc\"\n};\n"))
	writeTestFile(t, filepath.Join(p.root, "include", "empty.h"), nil)
	if err := p.generateTables(); err != nil {
		t.Fatal(err)
	}
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
		t.Fatalf("generated pitch table: text %d, data %d, hash %s", len(sections[".text"]), len(sections[".data"]), shortHash(sections[".data"]))
	}
}
