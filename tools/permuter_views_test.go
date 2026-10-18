package main

import (
	"crypto/sha256"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestPermuterPreprocessesTheConfiguredProgramView(t *testing.T) {
	requirePinnedToolchain(t)
	p := validTestProject(t)
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	m := config.byID["event-attack"]
	m.Defines = []defineSpec{{Name: "VARIANT_VALUE", Value: "37"}}
	f := &m.Functions[0]
	body := `#if !defined(FFT_EVENT_ATTACK) || !defined(FFT_EVENT)
#error missing program or family
#endif
#if VARIANT_VALUE != 37
#error wrong configured value
#endif
int attack_entry(void) { return VARIANT_VALUE; }
`
	writeTestFile(t, filepath.Join(p.root, m.sourceOf(f)), []byte(body))
	// The command needs an original-byte fixture to create its workspace;
	// scoring is stopped below, so no external permuter process can mutate it.
	code := mipsWords(0x03e00008, 0x24020025)
	f.Size, f.Hash = len(code), shortHash(code)
	image := make([]byte, m.Size)
	copy(image, code)
	m.SHA256 = fmt.Sprintf("%x", sha256.Sum256(image))
	writeTestFile(t, filepath.Join(p.filesRoot, m.File), image)
	writeTestFile(t, filepath.Join(p.filesRoot, "SCUS_942.21"), nil)
	p.xmlPath = filepath.Join(p.root, "disc.xml")
	writeTestFile(t, p.xmlPath, []byte("<project><track><directory_tree>"+strings.Repeat("<file/>", expectedISOFileCount)+"</directory_tree></track></project>"))
	if _, err := p.writeProjectConfig(config); err != nil {
		t.Fatal(err)
	}
	bin := filepath.Join(p.root, "bin")
	stopper := filepath.Join(bin, "decomp-permuter")
	writeTestFile(t, stopper, []byte("#!/bin/sh\n: > scoring-reached\nexit 77\n"))
	if err := os.Chmod(stopper, 0o700); err != nil {
		t.Fatal(err)
	}
	t.Setenv("PATH", bin+string(os.PathListSeparator)+os.Getenv("PATH"))
	err = p.permuteCommand([]string{"--duration=1", "--module=event-attack", f.Name})
	if err == nil || !strings.Contains(err.Error(), "could not score the base") {
		t.Fatalf("command did not reach the scoring fixture: %v", err)
	}
	workspace := filepath.Join(p.root, "build", "permute", f.Name)
	if _, err := os.Stat(filepath.Join(workspace, "scoring-reached")); err != nil {
		t.Fatal(err)
	}
	base, err := os.ReadFile(filepath.Join(workspace, "base.c"))
	if err != nil || !strings.Contains(string(base), "return 37;") {
		t.Fatalf("base did not use configured value: %s, %v", base, err)
	}
}
