package main

import (
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestCompileCommandsCarryEachSourcesProgramView(t *testing.T) {
	p := unitsTestProject(t)
	if err := os.WriteFile(filepath.Join(p.root, "src", "loose.c"), []byte("int loose;\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := p.compileCommandsCommand([]string{"--cc=clang-test", "--directory=/editor/checkout"}); err != nil {
		t.Fatal(err)
	}
	data, err := os.ReadFile(filepath.Join(p.root, "compile_commands.json"))
	if err != nil {
		t.Fatal(err)
	}
	var commands []compileCommand
	if err := json.Unmarshal(data, &commands); err != nil {
		t.Fatal(err)
	}
	byFile := map[string]compileCommand{}
	for _, command := range commands {
		byFile[command.File] = command
		if command.Directory != "/editor/checkout" || command.Arguments[0] != "clang-test" {
			t.Fatalf("entry %+v", command)
		}
	}
	loose, ok := byFile["src/loose.c"]
	if !ok || strings.Contains(strings.Join(loose.Arguments, " "), "-DFFT_") {
		t.Fatalf("a source no module compiles gets no program view: %+v", loose)
	}
	found := false
	for file, command := range byFile {
		if strings.HasPrefix(file, "src/battle/") {
			found = true
			if !strings.Contains(strings.Join(command.Arguments, " "), "-DFFT_BATTLE=1") {
				t.Fatalf("battle source without its program macro: %+v", command)
			}
		}
	}
	if !found {
		t.Fatalf("no battle sources in %v", byFile)
	}
}
