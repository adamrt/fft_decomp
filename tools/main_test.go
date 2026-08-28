package main

import (
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
)

func TestPSXEXEMetadataRejectsShortFiles(t *testing.T) {
	for _, contents := range [][]byte{nil, []byte("not an executable")} {
		path := filepath.Join(t.TempDir(), "wip.bin")
		if err := os.WriteFile(path, contents, 0o600); err != nil {
			t.Fatal(err)
		}
		_, ok, err := psxEXEMetadata(path)
		if err != nil {
			t.Fatalf("psxEXEMetadata returned an error: %v", err)
		}
		if ok {
			t.Fatal("short file was identified as a PS-X EXE")
		}
	}
}

func TestDumpsxisoArgs(t *testing.T) {
	got := dumpsxisoArgs("files", "disc.xml", "disc.bin")
	want := []string{"--quiet", "--lba", "-x", "files", "-s", "disc.xml", "disc.bin"}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("dumpsxisoArgs() = %q, want %q", got, want)
	}
}

func TestExtractionSourcePath(t *testing.T) {
	root := filepath.Join(t.TempDir(), "build", "extracted")
	files := filepath.Join(root, "files")

	tests := []struct {
		name   string
		source string
		want   string
	}{
		{name: "relative", source: "files/EVENT/SMALL.OUT", want: filepath.Join("EVENT", "SMALL.OUT")},
		{name: "absolute", source: filepath.Join(files, "SCUS_942.21"), want: "SCUS_942.21"},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			_, relative, err := extractionSourcePath(files, root, test.source)
			if err != nil {
				t.Fatal(err)
			}
			if relative != test.want {
				t.Fatalf("relative path = %q, want %q", relative, test.want)
			}
		})
	}
	if _, _, err := extractionSourcePath(files, root, "../outside.bin"); err == nil {
		t.Fatal("expected an out-of-tree source to be rejected")
	}
}

func TestMismatch(t *testing.T) {
	tests := []struct {
		name     string
		expected []byte
		actual   []byte
		offset   int
		differs  bool
	}{
		{name: "equal", expected: []byte{1, 2}, actual: []byte{1, 2}},
		{name: "byte", expected: []byte{1, 2}, actual: []byte{1, 3}, offset: 1, differs: true},
		{name: "short", expected: []byte{1, 2}, actual: []byte{1}, offset: 1, differs: true},
		{name: "long", expected: []byte{1}, actual: []byte{1, 2}, offset: 1, differs: true},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			offset, differs := mismatch(test.expected, test.actual)
			if offset != test.offset || differs != test.differs {
				t.Fatalf("mismatch() = (%d, %v), want (%d, %v)", offset, differs, test.offset, test.differs)
			}
		})
	}
}

func TestCandidatesSortAndExcludeConfigured(t *testing.T) {
	got := candidates(map[uint64]int{0x80010020: 3, 0x80010010: 1, 0x80010000: 2}, map[uint64]bool{0x80010020: true})
	if len(got) != 2 || got[0].Address != "0x80010000" || got[1].Address != "0x80010010" {
		t.Fatalf("candidates() = %+v", got)
	}
	if got[0].Confidence != "medium" || got[1].Confidence != "low" {
		t.Fatalf("unexpected confidence levels: %+v", got)
	}
}

func TestInstructionDifference(t *testing.T) {
	expected := []byte{0, 0, 0, 0, 1, 2, 3, 4}
	actual := []byte{0, 0, 0, 0, 1, 2, 9, 4}
	if got, want := instructionDifference(expected, actual, 6), "instruction +0x4: expected 0x04030201, got 0x04090201"; got != want {
		t.Fatalf("instructionDifference() = %q, want %q", got, want)
	}
	if got, want := instructionDifference(expected, actual[:5], 5), "instruction +0x4: expected 0x04030201, got <truncated>"; got != want {
		t.Fatalf("instructionDifference() = %q, want %q", got, want)
	}
}

func TestMismatchDiagnostics(t *testing.T) {
	target := []byte{0, 0, 0, 0, 0x20, 0x08, 0x02, 0x00, 0, 0, 0, 0}
	compiled := []byte{0, 0, 0, 0, 0x20, 0x10, 0x03, 0x00, 0, 0, 0, 0}
	got := mismatchDiagnostics(target, compiled, 4, 0x80010000)
	for _, want := range []string{"sizes:", "inside the function", "0x80010004", "rs"} {
		if !strings.Contains(got, want) {
			t.Fatalf("diagnostics missing %q:\n%s", want, got)
		}
	}
}

func TestReadMode2File(t *testing.T) {
	const sectorSize = 2352
	image := make([]byte, sectorSize*3)
	for i := range 2048 {
		image[sectorSize+24+i] = byte(i)
	}
	for i := range 17 {
		image[sectorSize*2+24+i] = byte(0x80 + i)
	}
	path := filepath.Join(t.TempDir(), "disc.bin")
	if err := os.WriteFile(path, image, 0o600); err != nil {
		t.Fatal(err)
	}
	got, err := readMode2File(path, 1, 2048+17)
	if err != nil {
		t.Fatal(err)
	}
	if len(got) != 2065 || got[1] != 1 || got[2048] != 0x80 || got[2064] != 0x90 {
		t.Fatalf("unexpected extracted data boundary: len=%d first=%02x second-sector=%02x last=%02x", len(got), got[1], got[2048], got[2064])
	}
}
