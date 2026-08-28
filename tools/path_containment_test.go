package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestProjectPathSymlinkContainment(t *testing.T) {
	base := t.TempDir()
	root := filepath.Join(base, "project")
	outside := filepath.Join(base, "project-other")
	writeTestFile(t, filepath.Join(root, "source.c"), []byte("inside"))
	writeTestFile(t, filepath.Join(outside, "source.c"), []byte("outside"))
	cwd, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
	}
	relativeRoot, err := filepath.Rel(cwd, root)
	if err != nil {
		t.Fatal(err)
	}
	for _, link := range []struct{ target, path string }{
		{"source.c", filepath.Join(root, "internal.c")},
		{filepath.Join(outside, "source.c"), filepath.Join(root, "external.c")},
		{outside, filepath.Join(root, "external-dir")},
		{"missing.c", filepath.Join(root, "dangling.c")},
		{"cycle.c", filepath.Join(root, "cycle.c")},
		{root, filepath.Join(base, "root-alias")},
	} {
		if err := os.Symlink(link.target, link.path); err != nil {
			t.Fatal(err)
		}
	}
	for _, tc := range []struct {
		name, root, value, errorText string
	}{
		{"regular", root, "source.c", ""},
		{"relative root", relativeRoot, "internal.c", ""},
		{"internal link", root, "internal.c", ""},
		{"linked root", filepath.Join(base, "root-alias"), "internal.c", ""},
		{"external file", root, "external.c", "must stay inside"},
		{"external directory", root, "external-dir/source.c", "must stay inside"},
		{"parent escape", root, "../project-other/source.c", "must stay inside"},
		{"absolute", root, filepath.Join(root, "source.c"), "must stay inside"},
		{"dangling", root, "dangling.c", "source"},
		{"cycle", root, "cycle.c", "source"},
		{"directory", root, ".", "not a regular file"},
	} {
		t.Run(tc.name, func(t *testing.T) {
			got, err := projectPath(tc.root, tc.value, "source")
			if tc.errorText != "" {
				if err == nil || !strings.Contains(err.Error(), tc.errorText) {
					t.Fatalf("projectPath error = %v, want %q", err, tc.errorText)
				}
				return
			}
			if err != nil || got != filepath.Join(tc.root, tc.value) {
				t.Fatalf("projectPath = %q, %v; logical path must be preserved", got, err)
			}
		})
	}
}
