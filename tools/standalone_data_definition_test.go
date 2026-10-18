package main

import (
	"bytes"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestStandaloneRejectsSwappedNamedRodataDefinitions(t *testing.T) {
	requirePinnedToolchain(t)
	for _, storage := range []string{"", "static "} {
		t.Run(storage+"objects", func(t *testing.T) {
			p := validTestProject(t)
			config, err := p.loadProjectConfig()
			if err != nil {
				t.Fatal(err)
			}
			m := config.byID["event-card"]
			f := &m.Functions[0]
			raw := []byte{0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12}
			f.Rodata = []rodataSpec{{Addr: m.Load + 0x80, Size: len(raw), Hash: shortHash(raw)}}
			m.Data = []symbolSpec{{Name: "g_first", Addr: m.Load + 0x80}, {Name: "g_second", Addr: m.Load + 0x84}}
			resolver := newSymbolResolver(config)
			source := filepath.Join(p.root, m.sourceOf(f))
			first := storage + "const int g_first = 0x12345678;\n"
			second := storage + "const int g_second = 0x12345678;\n"
			body := "void card_entry(void) {}\n"
			writeTestFile(t, source, []byte(first+second+body))
			t.Setenv("TOOLS_CACHE", "0")
			original, err := p.compileFunction(resolver, m, f, filepath.Join(p.root, "correct"))
			if err != nil || !bytes.Equal(original[".rodata"], raw) {
				t.Fatalf("valid definitions: %v, %x", err, original[".rodata"])
			}
			writeTestFile(t, source, []byte(second+first+body))
			for _, mode := range []string{"cold", "cache-fill", "cache-hit"} {
				t.Run(mode, func(t *testing.T) {
					cache := "1"
					if mode == "cold" {
						cache = "0"
					}
					t.Setenv("TOOLS_CACHE", cache)
					work := filepath.Join(p.root, mode)
					sections, err := p.compileFunction(resolver, m, f, work)
					if err == nil {
						if !bytes.Equal(sections[".rodata"], raw) || !bytes.Equal(sections[".text"], original[".text"]) {
							t.Fatal("fixture no longer preserves the original section bytes")
						}
						t.Error("equal constants exchanged names without rejection")
					} else if !strings.Contains(err.Error(), "target binds") {
						t.Fatalf("unexpected rejection: %v", err)
					}
					if mode == "cache-hit" {
						object := filepath.Join(work, f.Name, m.profileOf(f)+".o")
						if _, err := os.Stat(object); !os.IsNotExist(err) {
							t.Fatalf("cache-hit check unexpectedly compiled an object: %v", err)
						}
					}
				})
			}
		})
	}
}
