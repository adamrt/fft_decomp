package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"path/filepath"
	"strings"
	"testing"
)

func TestUnitOwnedDataBuildRechecksOriginal(t *testing.T) {
	requirePinnedToolchain(t)
	t.Setenv("TOOLS_CACHE", "0")
	for _, section := range []string{".data", ".rodata"} {
		t.Run(section, func(t *testing.T) {
			p := validTestProject(t)
			config, err := p.loadProjectConfig()
			if err != nil {
				t.Fatal(err)
			}
			m, _ := config.module("event-card")
			f := &m.Functions[0]
			source := "src/event/card_entry.c"
			qualifier := ""
			if section == ".rodata" {
				qualifier = "const "
			}
			originalSource := qualifier + "int g_card_table[2] = { 3, 7 };\nvoid card_entry(void) {}\n"
			changedSource := qualifier + "int g_card_table[2] = { 9, 7 };\nvoid card_entry(void) {}\n"
			originalData := []byte{3, 0, 0, 0, 7, 0, 0, 0}
			changedData := []byte{9, 0, 0, 0, 7, 0, 0, 0}
			addr := m.Load + 0x80
			contract := rodataSpec{Addr: addr, Size: 8, Hash: shortHash(originalData)}
			definition := unitDef{Name: "card_data", Source: source}
			if section == ".data" {
				definition.Data = []rodataSpec{contract}
			} else {
				definition.Rodata = []rodataSpec{contract}
			}
			m.Units = []unitDef{definition}
			m.Data = []symbolSpec{{Name: "g_card_table", Addr: addr}}
			f.Unit = definition.Name
			writeTestFile(t, filepath.Join(p.root, source), []byte(originalSource))
			unit, err := m.unitInstance(config, definition.Name)
			if err != nil {
				t.Fatal(err)
			}
			baseline, err := p.compileUnitInstance(newSymbolResolver(config), unit, filepath.Join(p.root, source), filepath.Join(p.root, "bootstrap"))
			if err != nil {
				t.Fatal(err)
			}
			if !bytes.Equal(baseline[section], originalData) {
				t.Fatalf("unexpected original fixture data: %x", baseline[section])
			}
			// Establish only this synthetic original's function contracts.
			// No production hash, image or target metadata is touched.
			f.Size = len(baseline[".text"])
			f.Hash = shortHash(baseline[".text"])
			image := make([]byte, m.Size)
			copy(image[m.offset(f.Addr):], baseline[".text"])
			copy(image[m.offset(addr):], originalData)
			sum := sha256.Sum256(image)
			m.SHA256 = hex.EncodeToString(sum[:])
			writeTestFile(t, filepath.Join(p.filesRoot, m.File), image)
			if err := validateUnitOwnedOriginal(m, image); err != nil {
				t.Fatal(err)
			}
			if _, err := p.buildModule(config, newSymbolResolver(config), m); err != nil {
				t.Fatal(err)
			}

			// A source-only edit must still fail the configured section hash.
			writeTestFile(t, filepath.Join(p.root, source), []byte(changedSource))
			if _, err := p.buildModule(config, newSymbolResolver(config), m); err == nil {
				t.Fatal("changed owned data was accepted against its original hash")
			}

			// Model an erroneous edited data hash: build should distrust it
			// against the verified original image, just as it distrusts an
			// erroneous function hash. This guard previously was absent.
			if section == ".data" {
				m.Units[0].Data[0].Hash = shortHash(changedData)
			} else {
				m.Units[0].Rodata[0].Hash = shortHash(changedData)
			}
			_, err = p.buildModule(config, newSymbolResolver(config), m)
			if err == nil || !strings.Contains(err.Error(), "hash does not match the original bytes") {
				t.Fatalf("changed owned-data hash was not rejected against the original: %v", err)
			}
			t.Logf("build rejects changed %s source/hash against the unchanged original image", section)
		})
	}
}
