package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func multipleFunctionRodataFixture(t *testing.T, gap int) (project, *projectConfig, *moduleSpec, []byte) {
	t.Helper()
	p := validTestProject(t)
	if err := os.MkdirAll(filepath.Join(p.root, "include"), 0o755); err != nil {
		t.Fatal(err)
	}
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	m, _ := config.module("event-card")
	f := &m.Functions[0]
	values := make([]byte, 40+gap)
	for i := range values {
		values[i] = byte(i + 1)
	}
	var source strings.Builder
	fmt.Fprintf(&source, "const unsigned char fixture_values[%d] = {", len(values))
	for i, value := range values {
		if i != 0 {
			source.WriteString(",")
		}
		fmt.Fprint(&source, value)
	}
	source.WriteString("};\nvoid card_entry(void) {}\n")
	sourcePath := "src/event/card_entry.c"
	writeTestFile(t, filepath.Join(p.root, sourcePath), []byte(source.String()))
	m.Units = []unitDef{{Name: "card_literals", Source: sourcePath}}
	f.Unit = "card_literals"
	f.Rodata = []rodataSpec{
		{Addr: m.Load + 0x80, Size: 8, Hash: shortHash(values[:8])},
		{Addr: m.Load + uint32(0x88+gap), Size: 32, Hash: shortHash(values[8+gap:])},
	}
	unit, err := m.unitInstance(config, "card_literals")
	if err != nil {
		t.Fatal(err)
	}
	baseline, err := p.compileUnitInstance(newSymbolResolver(config), unit, filepath.Join(p.root, sourcePath), filepath.Join(p.root, "bootstrap"))
	if err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(baseline[".rodata"], values) {
		t.Fatalf("unexpected synthetic literals: %x", baseline[".rodata"])
	}
	// Establish only this synthetic fixture's code contract, never game hashes.
	f.Size, f.Hash = len(baseline[".text"]), shortHash(baseline[".text"])
	image := bytes.Repeat([]byte{0xa5}, m.Size)
	copy(image[m.offset(f.Addr):], baseline[".text"])
	for _, span := range f.Rodata {
		start := int(span.Addr - f.Rodata[0].Addr)
		copy(image[m.offset(span.Addr):], values[start:start+span.Size])
	}
	sum := sha256.Sum256(image)
	m.SHA256 = hex.EncodeToString(sum[:])
	if _, err := p.writeProjectConfig(config); err != nil {
		t.Fatal(err)
	}
	return p, config, m, image
}

func TestMultipleFunctionRodataCheckAndBuild(t *testing.T) {
	requirePinnedToolchain(t)
	t.Setenv("TOOLS_CACHE", "0")
	for _, gap := range []int{0, 4} {
		t.Run(fmt.Sprintf("gap_%d", gap), func(t *testing.T) {
			p, config, m, image := multipleFunctionRodataFixture(t, gap)
			if _, err := os.Stat(p.filesRoot); !os.IsNotExist(err) {
				t.Fatalf("check fixture unexpectedly has originals: %v", err)
			}
			if err := p.checkCommand([]string{"--module=event-card"}); err != nil {
				t.Errorf("matching multiple ranges rejected by check: %v", err)
			}
			writeTestFile(t, filepath.Join(p.filesRoot, m.File), image)
			path, err := p.buildModule(config, newSymbolResolver(config), m)
			if err != nil {
				t.Errorf("matching multiple ranges rejected by build: %v", err)
			} else if output, err := os.ReadFile(path); err != nil || !bytes.Equal(output, image) {
				t.Errorf("build changed undeclared gaps or lost declared bytes: %v", err)
			}
		})
	}
}

func TestMultipleFunctionRodataRejectsChanges(t *testing.T) {
	requirePinnedToolchain(t)
	t.Setenv("TOOLS_CACHE", "0")
	for _, name := range []string{"first-range", "second-range", "missing-range", "extra-range", "short-output", "extra-output"} {
		t.Run(name, func(t *testing.T) {
			p, config, m, image := multipleFunctionRodataFixture(t, 4)
			f := &m.Functions[0]
			path := filepath.Join(p.root, "src/event/card_entry.c")
			source, err := os.ReadFile(path)
			if err != nil {
				t.Fatal(err)
			}
			text := string(source)
			switch name {
			case "first-range":
				text = strings.Replace(text, "{1,", "{201,", 1)
			case "second-range":
				text = strings.Replace(text, ",13,", ",213,", 1)
			case "missing-range":
				f.Rodata = f.Rodata[:1]
			case "extra-range":
				f.Rodata = append(f.Rodata, rodataSpec{Addr: m.Load + 0xac, Size: 4, Hash: shortHash([]byte{1, 2, 3, 4})})
			case "short-output":
				text = strings.Replace(strings.Replace(text, "[44]", "[43]", 1), ",44}", "}", 1)
			case "extra-output":
				text = strings.Replace(strings.Replace(text, "[44]", "[48]", 1), ",44}", ",44,45,46,47,48}", 1)
			}
			writeTestFile(t, path, []byte(text))
			if _, err := p.writeProjectConfig(config); err != nil {
				t.Fatal(err)
			}
			if err := p.checkCommand([]string{"--module=event-card"}); err == nil || !strings.Contains(err.Error(), "check:") {
				t.Fatalf("changed literal source/contract accepted by check: %v", err)
			}
			writeTestFile(t, filepath.Join(p.filesRoot, m.File), image)
			if _, err := p.buildModule(config, newSymbolResolver(config), m); err == nil || !strings.Contains(err.Error(), ".rodata") {
				t.Fatalf("changed literal source/contract accepted by build: %v", err)
			}
		})
	}
}

func TestMultipleFunctionRodataBuildDistrustsEditedHash(t *testing.T) {
	requirePinnedToolchain(t)
	t.Setenv("TOOLS_CACHE", "0")
	p, config, m, image := multipleFunctionRodataFixture(t, 0)
	path := filepath.Join(p.root, "src/event/card_entry.c")
	source, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	writeTestFile(t, path, []byte(strings.Replace(string(source), ",9,", ",209,", 1)))
	changed := append([]byte(nil), image[0x88:0xa8]...)
	changed[0] = 209
	m.Functions[0].Rodata[1].Hash = shortHash(changed)
	if _, err := p.writeProjectConfig(config); err != nil {
		t.Fatal(err)
	}
	// With only a synthetic edited contract, check has no original to distrust.
	if err := p.checkCommand([]string{"--module=event-card"}); err != nil {
		t.Fatalf("fixture no longer isolates original-byte verification: %v", err)
	}
	writeTestFile(t, filepath.Join(p.filesRoot, m.File), image)
	if _, err := p.buildModule(config, newSymbolResolver(config), m); err == nil || !strings.Contains(err.Error(), "hash does not match the original bytes") {
		t.Fatalf("edited later-range hash accepted against original image: %v", err)
	}
}

func TestFunctionRodataPackedExtent(t *testing.T) {
	f := &functionSpec{Rodata: []rodataSpec{{Addr: 0x1000, Size: 2}, {Addr: 0x1006, Size: 3}}}
	compiled := []byte{11, 13, 17, 19, 23}
	spans, err := splitFunctionRodata(f, compiled)
	if err != nil || len(spans) != 2 || !bytes.Equal(spans[0], []byte{11, 13}) || !bytes.Equal(spans[1], []byte{17, 19, 23}) {
		t.Fatalf("packed ranges were treated as an address envelope: %x, %v", spans, err)
	}
	for _, data := range [][]byte{nil, compiled[:2], compiled[:4], append(append([]byte(nil), compiled...), 29)} {
		if _, err := splitFunctionRodata(f, data); err == nil {
			t.Fatalf("wrong packed extent accepted: %x", data)
		}
	}
	for _, size := range []int{0, -1, int(^uint(0) >> 1)} {
		f.Rodata[1].Size = size
		if _, err := splitFunctionRodata(f, compiled); err == nil {
			t.Fatalf("invalid range size accepted: %d", size)
		}
	}
	if _, err := linkerScript(&functionSpec{Name: "standalone", Rodata: []rodataSpec{{Addr: 0x1000, Size: 2}, {Addr: 0x1006, Size: 3}}}, nil); err == nil || !strings.Contains(err.Error(), "more than one") {
		t.Fatalf("standalone multiple-range restriction changed: %v", err)
	}
}
