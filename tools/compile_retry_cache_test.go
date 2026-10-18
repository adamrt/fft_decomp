package main

import (
	"bytes"
	"os"
	"os/exec"
	"path/filepath"
	"testing"
)

func TestCompileRetryCachesOnlyTheSuccessfulInput(t *testing.T) {
	requirePinnedToolchain(t)
	t.Setenv("TOOLS_CACHE", "1")
	p := project{root: t.TempDir()}
	profile, _ := compilerProfileNamed(defaultProfile)
	source := filepath.Join(p.root, "candidate.c")
	linker := filepath.Join(p.root, "layout.ld")
	invalid := []byte("int candidate(void) { return ; broken syntax\n")
	writeTestFile(t, source, invalid)
	writeTestFile(t, linker, []byte("SECTIONS { . = 0x80010000; .text : { *(.text) } /DISCARD/ : { *(.reginfo) } }\n"))
	before := filepath.Join(p.root, "before.i")
	if err := p.repreprocess(source, before, nil); err != nil {
		t.Fatal(err)
	}
	oldKey, err := compilationCacheKey(before, linker, profile, "C")
	if err != nil {
		t.Fatal(err)
	}
	// Change the source after the first preprocessing, leaving the first
	// compiler input invalid. The retry then reads the completed write.
	cpp, err := exec.LookPath("mipsel-linux-gnu-cpp")
	if err != nil {
		t.Fatal(err)
	}
	bin := filepath.Join(p.root, "bin")
	wrapper := filepath.Join(bin, "mipsel-linux-gnu-cpp")
	writeTestFile(t, wrapper, []byte(`#!/bin/sh
set -eu
"$RETRY_TEST_CPP" "$@"
if [ ! -f "$RETRY_TEST_MARKER" ]; then
    printf '%s\n' 'int candidate(void) { return 7; }' > "$RETRY_TEST_SOURCE"
    : > "$RETRY_TEST_MARKER"
fi
`))
	if err := os.Chmod(wrapper, 0o700); err != nil {
		t.Fatal(err)
	}
	t.Setenv("RETRY_TEST_CPP", cpp)
	t.Setenv("RETRY_TEST_MARKER", filepath.Join(p.root, "edited"))
	t.Setenv("RETRY_TEST_SOURCE", source)
	t.Setenv("PATH", bin+string(os.PathListSeparator)+os.Getenv("PATH"))
	work := filepath.Join(p.root, "retry")
	sections, err := p.compileHistoricalSections(source, linker, work, profile)
	if err != nil {
		t.Fatal(err)
	}
	if len(sections[".text"]) == 0 {
		t.Fatal("retry produced no code")
	}
	finalKey, err := compilationCacheKey(filepath.Join(work, profile.name+".i"), linker, profile, "C")
	if err != nil || finalKey == oldKey {
		t.Fatalf("retry did not change the input key: %s, %v", finalKey, err)
	}
	cache := filepath.Join(p.root, "build", "cache", "tools")
	if _, err := os.Stat(filepath.Join(cache, oldKey+".json")); !os.IsNotExist(err) {
		t.Errorf("failed input received successful cached code: %v", err)
	}
	if _, err := os.Stat(filepath.Join(cache, finalKey+".json")); err != nil {
		t.Errorf("successful input has no cache entry: %v", err)
	}
	hit := filepath.Join(p.root, "cache-hit")
	cached, err := p.compileHistoricalSections(source, linker, hit, profile)
	if err != nil || !bytes.Equal(cached[".text"], sections[".text"]) {
		t.Fatalf("successful-input cache hit: %v", err)
	}
	if _, err := os.Stat(filepath.Join(hit, profile.name+".s")); !os.IsNotExist(err) {
		t.Error("successful-input cache lookup invoked the compiler")
	}
	writeTestFile(t, source, invalid)
	if _, err := p.compileHistoricalSections(source, linker, filepath.Join(p.root, "invalid-restored"), profile); err == nil {
		t.Fatal("restoring invalid pre-retry source returned cached valid code")
	}
}
