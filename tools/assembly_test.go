package main

import (
	"bytes"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestAssemblyFunctionValidation(t *testing.T) {
	valid := "  - {addr: 0x80010000, size: 8, name: main_first, hash: 0123456789abcdef, asm: \"Test ABI boundary\"}"
	for bad, line := range map[string]string{
		"":        valid,
		"range":   strings.Replace(strings.Replace(valid, "0x80010000", "0x800449ec", 1), "main_first", "main_late", 1),
		"reason":  strings.Replace(valid, `asm: "Test ABI boundary"`, `asm: " "`, 1),
		"c-file":  strings.Replace(valid, `asm: "Test ABI boundary"`, `source: src/main/main_first.c`, 1),
		"s-for-c": strings.Replace(valid, `, asm: "Test ABI boundary"`, `, source: src/main/main_first.s`, 1),
	} {
		t.Run(bad, func(t *testing.T) {
			p := validTestProject(t)
			for _, name := range []string{"main_first.s", "main_late.s"} {
				if err := os.WriteFile(filepath.Join(p.root, "src", "main", name), []byte("nop\n"), 0o644); err != nil {
					t.Fatal(err)
				}
			}
			if err := os.Remove(filepath.Join(p.root, "src", "main", "main_first.c")); err != nil {
				t.Fatal(err)
			}
			text := strings.Replace(testMainYAML, "  - {addr: 0x80010000, size: 8, name: main_first, hash: 0123456789abcdef}", line, 1)
			if bad == "c-file" {
				if err := os.WriteFile(filepath.Join(p.root, "src", "main", "main_first.c"), []byte("x\n"), 0o644); err != nil {
					t.Fatal(err)
				}
			}
			if err := os.WriteFile(filepath.Join(p.root, "target", "main.yaml"), []byte(text), 0o644); err != nil {
				t.Fatal(err)
			}
			config, err := p.loadProjectConfig()
			if err != nil {
				t.Fatal(err)
			}
			if _, err := p.writeProjectConfig(config); err != nil {
				t.Fatal(err)
			}
			if config, err = p.loadProjectConfig(); err != nil {
				t.Fatal(err)
			}
			err = p.validateProjectConfig(config)
			if bad == "" && err != nil && !strings.Contains(err.Error(), "orphan") {
				t.Fatalf("valid assembly rejected: %v", err)
			}
			if bad != "" && (err == nil || !strings.Contains(err.Error(), "main_")) {
				t.Fatalf("%s accepted: %v", bad, err)
			}
		})
	}
}

func TestSourceInventoryRejectsOrphans(t *testing.T) {
	root := t.TempDir()
	if err := os.MkdirAll(filepath.Join(root, "src"), 0o755); err != nil {
		t.Fatal(err)
	}
	for _, name := range []string{"test.c", "extra.s"} {
		if err := os.WriteFile(filepath.Join(root, "src", name), []byte("nop\n"), 0o600); err != nil {
			t.Fatal(err)
		}
	}
	if err := validateSourceInventory(root, "src", map[string]bool{"src/test.c": true}, "test"); err == nil {
		t.Fatal("orphan assembly accepted")
	}
	if err := validateSourceInventory(root, "src", map[string]bool{"src/test.c": true, "src/extra.s": true}, "test"); err != nil {
		t.Fatal(err)
	}
}

func TestStandaloneAssembly(t *testing.T) {
	if err := (project{}).compilePermuterInput([]string{"fixture.ld", "fixture.s", "out.o", defaultProfile}); err == nil || !strings.Contains(err.Error(), "requires C source") {
		t.Fatalf("assembly permuter callback: %v", err)
	}
	for _, input := range []string{".include \"a.s\"", "label: .incbin \"media.bin\"", ".INCLUDE \"a.s\""} {
		if validateStandaloneAssembly([]byte(input)) == nil {
			t.Fatalf("accepted %s", input)
		}
	}
	if err := validateStandaloneAssembly([]byte("\tjr\t$ra\n\tnop\n")); err != nil {
		t.Fatal(err)
	}
	if _, err := sourceLanguage("object.bin"); err == nil {
		t.Fatal("accepted binary source")
	}
}

// Docker integration fixture: no game bytes or SDK source are needed.
func TestAssemblyBackendAndCacheSeparation(t *testing.T) {
	requirePinnedToolchain(t)
	root := t.TempDir()
	p := project{root: root}
	source, linker := filepath.Join(root, "fixture.s"), filepath.Join(root, "fixture.ld")
	assembly := "\t.text\n\t.set\tnoreorder\n\t.ent\tfixture\nfixture:\n\taddu\t$v0,$a0,$zero\n\tjr\t$ra\n\tnop\n\t.end\tfixture\n"
	if err := os.WriteFile(source, []byte(assembly), 0o600); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(linker, []byte("OUTPUT_ARCH(mips)\nSECTIONS { .text 0x80010000 : SUBALIGN(4) { *(.text) } /DISCARD/ : { *(.reginfo) *(.MIPS.abiflags) *(.pdr) } }\n"), 0o600); err != nil {
		t.Fatal(err)
	}
	profile, _ := compilerProfileNamed(defaultProfile)
	cKey, err := compilationCacheKey(source, linker, profile, "C")
	if err != nil {
		t.Fatal(err)
	}
	aKey, err := compilationCacheKey(source, linker, profile, "asm")
	if err != nil || cKey == aKey {
		t.Fatalf("cache language collision: %v", err)
	}
	t.Setenv("TOOLS_CACHE", "0")
	sections, err := p.compileHistoricalSections(source, linker, filepath.Join(root, "build"), profile)
	if err != nil {
		t.Fatal(err)
	}
	want := []byte{0x21, 0x10, 0x80, 0x00, 0x08, 0x00, 0xe0, 0x03, 0, 0, 0, 0}
	if !bytes.Equal(sections[".text"], want) {
		t.Fatalf("assembly bytes: %x", sections[".text"])
	}
	t.Setenv("TOOLS_CACHE", "1")
	if _, err := p.compileHistoricalSections(source, linker, filepath.Join(root, "cache-fill"), profile); err != nil {
		t.Fatal(err)
	}
	cached, err := p.compileHistoricalSections(source, linker, filepath.Join(root, "cache-hit"), profile)
	if err != nil || !bytes.Equal(cached[".text"], want) {
		t.Fatalf("assembly cache hit: %v", err)
	}
	if _, err := os.Stat(filepath.Join(root, "cache-hit", profile.name+".o")); !os.IsNotExist(err) {
		t.Fatalf("cache hit unexpectedly assembled: %v", err)
	}
	t.Setenv("TOOLS_CACHE", "0")
	changed := append([]byte(nil), want...)
	changed[0] ^= 1
	if _, differs := mismatch(changed, sections[".text"]); !differs {
		t.Fatal("accepted changed expected bytes")
	}
	if _, differs := mismatch(want[:8], sections[".text"]); !differs {
		t.Fatal("accepted wrong size")
	}
	// Allocatable sections must remain visible to the shared production checker.
	if err := os.WriteFile(source, []byte(assembly+"\t.section\t.extra,\"a\"\n\t.word\t7\n"), 0o600); err != nil {
		t.Fatal(err)
	}
	sections, err = p.compileHistoricalSections(source, linker, filepath.Join(root, "extra"), profile)
	if err != nil {
		t.Fatal(err)
	}
	if len(sections[".extra"]) == 0 {
		t.Fatal("extra allocatable section hidden")
	}
}
