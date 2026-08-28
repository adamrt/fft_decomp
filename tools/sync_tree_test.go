package main

import (
	"os"
	"path/filepath"
	"testing"
	"time"
)

func TestSyncTreeMirrorsChangesAndDeletions(t *testing.T) {
	root := t.TempDir()
	source := filepath.Join(root, "source")
	mirror := filepath.Join(root, "mirror")
	write := func(path, text string) {
		t.Helper()
		full := filepath.Join(source, path)
		if err := os.MkdirAll(filepath.Dir(full), 0o755); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(full, []byte(text), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	read := func(path string) (string, bool) {
		data, err := os.ReadFile(filepath.Join(mirror, path))
		return string(data), err == nil
	}

	write("a.h", "one")
	write("dir/b.c", "two")
	if err := syncTree(source, mirror); err != nil {
		t.Fatal(err)
	}
	if got, ok := read("dir/b.c"); !ok || got != "two" {
		t.Fatalf("initial copy: got %q, %v", got, ok)
	}

	// A same-size edit must still propagate: modification times differ.
	write("a.h", "uno")
	later := time.Now().Add(time.Minute)
	if err := os.Chtimes(filepath.Join(source, "a.h"), later, later); err != nil {
		t.Fatal(err)
	}
	if err := os.RemoveAll(filepath.Join(source, "dir")); err != nil {
		t.Fatal(err)
	}
	if err := syncTree(source, mirror); err != nil {
		t.Fatal(err)
	}
	if got, _ := read("a.h"); got != "uno" {
		t.Fatalf("changed file not refreshed: got %q", got)
	}
	if _, ok := read("dir/b.c"); ok {
		t.Fatal("deleted file still present in mirror")
	}
	if _, err := os.Stat(filepath.Join(mirror, "dir")); !os.IsNotExist(err) {
		t.Fatalf("deleted directory still present in mirror: %v", err)
	}
}
