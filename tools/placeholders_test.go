package main

import (
	"path/filepath"
	"strings"
	"testing"
)

func TestValidatePlaceholderMembers(t *testing.T) {
	root := t.TempDir()
	header := "typedef struct unit {\n    u8 id;\n    u8 _unknown_01;\n    u16 _unused_02;\n    u8 _padding_04[4];\n} unit_t;\n"
	writeTestFile(t, filepath.Join(root, "include", "fft", "unit.h"), []byte(header))
	writeTestFile(t, filepath.Join(root, "src", "battle", "a.c"),
		[]byte("/* ._unused_02 in a comment is fine */\nvoid a(unit_t* u) { u->_unknown_01 = 1; }\n"))
	if err := validatePlaceholderMembers(root); err != nil {
		t.Fatalf("clean tree: %v", err)
	}

	writeTestFile(t, filepath.Join(root, "src", "battle", "b.c"),
		[]byte("void b(unit_t* u, unit_t v) {\n    u->_unused_02 = 0;\n    v._padding_04[0] = 0;\n}\n"))
	err := validatePlaceholderMembers(root)
	if err == nil {
		t.Fatal("expected accesses to be reported")
	}
	for _, want := range []string{"src/battle/b.c:2 _unused_02", "src/battle/b.c:3 _padding_04"} {
		if !strings.Contains(err.Error(), want) {
			t.Fatalf("error %q does not mention %q", err, want)
		}
	}
}
