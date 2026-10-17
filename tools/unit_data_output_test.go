package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func unitDataOutputFixture() (*moduleSpec, *unitInstance, map[string][]byte, []byte) {
	m := &moduleSpec{ID: "fixture", Load: 0x80010000, Size: 256}
	data := []byte{3, 7, 11, 13, 0, 0, 0, 0, 17, 19, 23, 0, 0, 0, 0, 0}
	rodata := []byte{29, 31, 37, 41, 0, 0, 0, 0, 43, 47, 53, 59}
	m.Units = []unitDef{{Name: "tables",
		Data: []rodataSpec{
			{Addr: m.Load + 0x20, Size: 4, Hash: shortHash(data[:4])},
			{Addr: m.Load + 0x28, Size: 3, Hash: shortHash(data[8:11])},
		},
		Rodata: []rodataSpec{{Addr: m.Load + 0x68, Size: 4, Hash: shortHash(rodata[8:])}},
	}}
	literal := rodataSpec{Addr: m.Load + 0x60, Size: 4, Hash: shortHash(rodata[:4])}
	m.Functions = []functionSpec{{Name: "entry", Addr: m.Load, Size: 8, Rodata: []rodataSpec{literal}}}
	unit := &unitInstance{
		module: m,
		spec:   &unitSpec{name: "tables", rodata: m.Units[0].Rodata},
		data:   m.Units[0].Data,
		rodata: append([]rodataSpec{literal}, m.Units[0].Rodata...),
	}
	sections := map[string][]byte{".data": data, ".rodata": rodata}
	// Original padding deliberately differs from compiler padding. Those bytes
	// have no table contract and must remain untouched in the rebuilt image.
	original := bytes.Repeat([]byte{0xa5}, m.Size)
	for _, span := range m.Units[0].Data {
		start := int(span.Addr - unit.data[0].Addr)
		copy(original[m.offset(span.Addr):], data[start:start+span.Size])
	}
	copy(original[0x60:], rodata[:4])
	copy(original[0x68:], rodata[8:])
	return m, unit, sections, original
}

func TestUnitOwnedDataWritesDeclaredSpansOnly(t *testing.T) {
	m, unit, sections, original := unitDataOutputFixture()
	if err := validateUnitOwnedOriginal(m, original); err != nil {
		t.Fatal(err)
	}
	for name, spans := range map[string][]rodataSpec{".data": unit.data, ".rodata": unit.rodata} {
		if problems := compareUnitRanges(name, spans, sections[name]); len(problems) != 0 {
			t.Fatal(problems)
		}
	}
	compiled := sliceUnitOwnedData(unit, sections)
	// A copied original could conceal a missing write. Poison the whole
	// destination, and require exactly the declared table bytes to be restored.
	output := bytes.Repeat([]byte{0xcc}, m.Size)
	expected := append([]byte(nil), output...)
	for _, span := range append(append([]rodataSpec(nil), m.Units[0].Data...), m.Units[0].Rodata...) {
		start := m.offset(span.Addr)
		copy(expected[start:start+span.Size], original[start:start+span.Size])
	}
	if err := writeUnitOwnedData(m, &m.Units[0], compiled, original, output); err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(output, expected) {
		t.Fatal("table bytes were not emitted, or literals, gaps or trailing padding were overwritten")
	}
}

func TestUnitOwnedDataRejectsInvalidOutput(t *testing.T) {
	for _, name := range []string{"missing", "extra", "duplicate", "section", "short", "long", "changed", "hash", "bounds", "image size"} {
		t.Run(name, func(t *testing.T) {
			m, unit, sections, original := unitDataOutputFixture()
			compiled := sliceUnitOwnedData(unit, sections)
			switch name {
			case "missing":
				compiled = compiled[:2]
			case "extra":
				compiled = append(compiled, compiled[0])
			case "duplicate":
				compiled[1] = compiled[0]
			case "section":
				compiled[2].section = ".data"
			case "short":
				compiled[2].data = compiled[2].data[:3]
			case "long":
				compiled[2].data = append(compiled[2].data, 0)
			case "changed":
				compiled[2].data[0] ^= 1
			case "hash":
				m.Units[0].Rodata[0].Hash = shortHash([]byte("wrong"))
			case "bounds":
				m.Units[0].Rodata[0].Addr = m.Load + uint32(m.Size)
			case "image size":
				original = original[:len(original)-1]
			}
			output := bytes.Repeat([]byte{0xcc}, m.Size)
			before := append([]byte(nil), output...)
			if err := writeUnitOwnedData(m, &m.Units[0], compiled, original, output); err == nil {
				t.Fatal("invalid owned data was accepted")
			}
			if !bytes.Equal(output, before) {
				t.Fatal("a rejected unit partially modified the destination")
			}
		})
	}
}

func TestUnitOwnedDataLayoutConflicts(t *testing.T) {
	for _, name := range []string{"same unit", "other unit", "text", "writable literal", "shared read-only literal"} {
		t.Run(name, func(t *testing.T) {
			m, _, _, _ := unitDataOutputFixture()
			switch name {
			case "same unit":
				m.Units[0].Rodata[0].Addr = m.Units[0].Data[0].Addr + 1
			case "other unit":
				m.Units = append(m.Units, unitDef{Name: "duplicate", Data: []rodataSpec{m.Units[0].Data[0]}})
			case "text":
				m.Units[0].Data[0].Addr = m.Functions[0].Addr + 4
			case "writable literal":
				m.Functions[0].Rodata[0].Addr = m.Units[0].Data[0].Addr
			case "shared read-only literal":
				// BATTLE has an owned table partially overlapping another
				// unit's function literals. Both compiles must match retail.
				m.Functions[0].Rodata[0].Size = 10
			}
			err := validateUnitOwnedLayout(m)
			if name == "shared read-only literal" {
				if err != nil {
					t.Fatalf("intentional literal sharing was rejected: %v", err)
				}
			} else if err == nil || !strings.Contains(err.Error(), "overlaps") {
				t.Fatalf("ownership conflict was not rejected: %v", err)
			}
		})
	}
}

func TestUnitOwnedDataSurvivesModuleCompileAndBuild(t *testing.T) {
	requirePinnedToolchain(t)
	p := validTestProject(t)
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	m, _ := config.module("event-card")
	f := &m.Functions[0]
	source := "src/event/card_entry.c"
	writeTestFile(t, filepath.Join(p.root, source), []byte("int g_card_table[2] = {3, 7};\nconst int g_card_constants[2] = {11, 17};\nvoid card_entry(void) {}\n"))
	data := []byte{3, 0, 0, 0, 7, 0, 0, 0}
	rodata := []byte{11, 0, 0, 0, 17, 0, 0, 0}
	m.Units = []unitDef{{Name: "card_data", Source: source,
		Data:   []rodataSpec{{Addr: m.Load + 0x80, Size: len(data), Hash: shortHash(data)}},
		Rodata: []rodataSpec{{Addr: m.Load + 0xa0, Size: len(rodata), Hash: shortHash(rodata)}},
	}}
	m.Data = []symbolSpec{{Name: "g_card_table", Addr: m.Load + 0x80}, {Name: "g_card_constants", Addr: m.Load + 0xa0}}
	f.Unit = "card_data"
	unit, err := m.unitInstance(config, "card_data")
	if err != nil {
		t.Fatal(err)
	}
	t.Setenv("TOOLS_CACHE", "0")
	baseline, err := p.compileUnitInstance(newSymbolResolver(config), unit, filepath.Join(p.root, source), filepath.Join(p.root, "bootstrap"))
	if err != nil {
		t.Fatal(err)
	}
	// Only this synthetic fixture gets contracts from its initial compile.
	// Production metadata and original game hashes are never modified.
	f.Size, f.Hash = len(baseline[".text"]), shortHash(baseline[".text"])
	image := bytes.Repeat([]byte{0xa5}, m.Size)
	copy(image[m.offset(f.Addr):], baseline[".text"])
	copy(image[0x80:], data)
	copy(image[0xa0:], rodata)
	for _, mode := range []string{"cold", "cache-fill", "cache-hit"} {
		t.Run(mode, func(t *testing.T) {
			cache := "1"
			if mode == "cold" {
				cache = "0"
			}
			t.Setenv("TOOLS_CACHE", cache)
			work := filepath.Join(p.root, mode)
			t.Setenv("TOOLS_WORK_DIR", work)
			compiled, err := p.compileModule(config, newSymbolResolver(config), m, nil)
			if err != nil {
				t.Fatal(err)
			}
			if compiled.functions[0].err != nil {
				t.Fatal(compiled.functions[0].err)
			}
			if _, err := os.Stat(p.filesRoot); !os.IsNotExist(err) {
				t.Fatalf("hash checking must work without extracted originals: %v", err)
			}
			if mode == "cache-hit" {
				object := filepath.Join(work, "modules", m.ID, "units", "card_data", m.ID, unit.profile.name+".o")
				if _, err := os.Stat(object); !os.IsNotExist(err) {
					t.Fatalf("cache hit unexpectedly recompiled: %v", err)
				}
			}
			output := bytes.Repeat([]byte{0xcc}, m.Size)
			if err := writeUnitOwnedData(m, &m.Units[0], compiled.unitData[0], image, output); err != nil {
				t.Fatal(err)
			}
			if !bytes.Equal(output[0x80:0x88], data) || !bytes.Equal(output[0xa0:0xa8], rodata) {
				t.Fatal("module compilation lost owned .data or .rodata")
			}
		})
	}
	// The build additionally needs the verified original, and must report both
	// emitted spans while preserving all bytes outside the declared ranges.
	sum := sha256.Sum256(image)
	m.SHA256 = hex.EncodeToString(sum[:])
	writeTestFile(t, filepath.Join(p.filesRoot, m.File), image)
	var log bytes.Buffer
	p.out = &log
	outputPath, err := p.buildModule(config, newSymbolResolver(config), m)
	if err != nil {
		t.Fatal(err)
	}
	output, err := os.ReadFile(outputPath)
	if err != nil || !bytes.Equal(output, image) {
		t.Fatalf("rebuilt fixture differs: %v", err)
	}
	if strings.Count(log.String(), "matching C data: unit card_data") != 2 {
		t.Fatalf("build did not report both emitted tables: %s", log.String())
	}
}
