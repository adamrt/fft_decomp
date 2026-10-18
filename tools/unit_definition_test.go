package main

import (
	"bytes"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func namedDataUnitFixture(t *testing.T) (project, *projectConfig, *unitInstance, []byte) {
	t.Helper()
	p := validTestProject(t)
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	m := config.byID["event-card"]
	f := &m.Functions[0]
	f.Unit = "named_data"
	raw := []byte{0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12}
	m.Data = []symbolSpec{
		{Name: "g_first", Addr: m.Load + 0x80},
		{Name: "g_second", Addr: m.Load + 0x84},
		{Name: "g_alias", Addr: m.Load + 0x80},
		{Name: "g_unrecovered", Addr: m.Load + 0xc0},
	}
	m.Imports = []symbolSpec{{Name: "g_external", Addr: 0x80020000}}
	m.Units = []unitDef{{Name: f.Unit, Source: "src/event/card_entry.c",
		Data: []rodataSpec{{Addr: m.Load + 0x80, Size: len(raw), Hash: shortHash(raw)}},
	}}
	unit, err := m.unitInstance(config, f.Unit)
	if err != nil {
		t.Fatal(err)
	}
	return p, config, unit, raw
}

const namedDataUnitBody = `
extern int g_alias, g_external, g_unrecovered;
void card_entry(void) { g_external = g_alias; }
`

func TestUnitRejectsSwappedNamedDataDefinitions(t *testing.T) {
	requirePinnedToolchain(t)
	for _, storage := range []string{"", "static "} {
		t.Run(storage+"objects", func(t *testing.T) {
			p, config, unit, raw := namedDataUnitFixture(t)
			resolver := newSymbolResolver(config)
			source := filepath.Join(p.root, unit.spec.source)
			first := storage + "int g_first = 0x12345678;\n"
			second := storage + "int g_second = 0x12345678;\n"
			writeTestFile(t, source, []byte(first+second+namedDataUnitBody))
			t.Setenv("TOOLS_CACHE", "0")
			original, err := p.compileUnitInstance(resolver, unit, source, filepath.Join(p.root, "correct"))
			if err != nil || !bytes.Equal(original[".data"], raw) {
				t.Fatalf("valid definitions and fixed-address extern aliases: %v, %x", err, original[".data"])
			}
			f := unit.members[0]
			f.Size, f.Hash, unit.size = len(original[".text"]), shortHash(original[".text"]), len(original[".text"])
			writeTestFile(t, source, []byte(second+first+namedDataUnitBody))
			for _, mode := range []string{"cold", "cache-fill", "cache-hit"} {
				t.Run(mode, func(t *testing.T) {
					cache := "1"
					if mode == "cold" {
						cache = "0"
					}
					t.Setenv("TOOLS_CACHE", cache)
					work := filepath.Join(p.root, mode)
					sections, err := p.compileUnitInstance(resolver, unit, source, work)
					if err == nil {
						if !bytes.Equal(sections[".data"], raw) || !bytes.Equal(sections[".text"], original[".text"]) {
							t.Fatal("fixture no longer preserves the original section bytes")
						}
						t.Error("equal initialized objects exchanged names without rejection")
					} else if !strings.Contains(err.Error(), "target binds") {
						t.Fatalf("unexpected rejection: %v", err)
					}
					if mode == "cache-hit" {
						object := filepath.Join(work, unit.module.ID, unit.profile.name+".o")
						if _, err := os.Stat(object); !os.IsNotExist(err) {
							t.Fatalf("cache-hit check unexpectedly compiled an object: %v", err)
						}
					}
				})
			}
		})
	}
}

func TestUnitRechecksCachedDefinitionsAgainstCurrentBindings(t *testing.T) {
	requirePinnedToolchain(t)
	t.Setenv("TOOLS_CACHE", "1")
	p, config, unit, _ := namedDataUnitFixture(t)
	// Definitions introduced by an include do not need PROVIDE bindings in
	// the linker script. Changing their target addresses leaves that script
	// and the compiler cache key unchanged, so validation must run on a hit.
	writeTestFile(t, filepath.Join(p.root, "include", "objects.h"), []byte("static int g_first = 0x12345678;\nstatic int g_second = 0x12345678;\n"))
	source := filepath.Join(p.root, unit.spec.source)
	writeTestFile(t, source, []byte("#include \"objects.h\"\n"+namedDataUnitBody))
	resolver := newSymbolResolver(config)
	if _, err := p.compileUnitInstance(resolver, unit, source, filepath.Join(p.root, "fill")); err != nil {
		t.Fatal(err)
	}
	unit.module.Data[0].Addr, unit.module.Data[1].Addr = unit.module.Data[1].Addr, unit.module.Data[0].Addr
	work := filepath.Join(p.root, "changed-target")
	_, err := p.compileUnitInstance(newSymbolResolver(config), unit, source, work)
	if err == nil || !strings.Contains(err.Error(), "target binds") {
		t.Fatalf("cached metadata was not checked against the current target: %v", err)
	}
	object := filepath.Join(work, unit.module.ID, unit.profile.name+".o")
	if _, err := os.Stat(object); !os.IsNotExist(err) {
		t.Fatalf("fixture did not exercise a cache hit: %v", err)
	}
}
