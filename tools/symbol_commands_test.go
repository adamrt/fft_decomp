package main

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func readTestFile(t *testing.T, p project, relative string) string {
	t.Helper()
	data, err := os.ReadFile(filepath.Join(p.root, filepath.FromSlash(relative)))
	if err != nil {
		t.Fatal(err)
	}
	return string(data)
}

func TestRenameFunctionMovesSourceConfigAndCallers(t *testing.T) {
	p := validTestProject(t)
	writeSymbolRenameFixture(t, p.root, "include/fft/main.h", "void main_first(void);\n#include \"fft/main_first.h\"\n")
	if err := p.symbolsCommand([]string{"rename-function", "--old", "main_first", "--new", "main_start", "--dry-run"}); err != nil {
		t.Fatal(err)
	}
	if _, err := os.Stat(filepath.Join(p.root, "src", "main", "main_first.c")); err != nil {
		t.Fatal("dry run moved the source")
	}
	if err := p.symbolsCommand([]string{"rename-function", "--old", "main_first", "--new", "main_start"}); err != nil {
		t.Fatal(err)
	}
	if _, err := os.Stat(filepath.Join(p.root, "src", "main", "main_first.c")); !os.IsNotExist(err) {
		t.Fatal("old source still exists")
	}
	if got := readTestFile(t, p, "src/main/main_start.c"); got != "void main_start(void) {}\n" {
		t.Fatalf("moved source = %q", got)
	}
	if got := readTestFile(t, p, "include/fft/main.h"); got != "void main_start(void);\n#include \"fft/main_first.h\"\n" {
		t.Fatalf("header = %q", got)
	}
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	main, _ := config.module("main")
	if main.function("main_start") == nil || main.function("main_first") != nil {
		t.Fatal("configuration was not renamed")
	}
	if err := p.validateProjectConfig(config); err != nil {
		t.Fatal(err)
	}
}

func TestRenameFunctionUpdatesOverridesAndRejectsCollisions(t *testing.T) {
	p := validTestProject(t)
	if err := p.symbolsCommand([]string{"rename-function", "--old", "attack_entry", "--new", "attack_start"}); err != nil {
		t.Fatal(err)
	}
	if text := readTestFile(t, p, "target/event.yaml"); !strings.Contains(text, "{function: attack_start, name: battle_helper") {
		t.Fatalf("override owner not renamed:\n%s", text)
	}
	before := readTestFile(t, p, "target/main.yaml")
	for _, args := range [][]string{
		{"--old", "main_first", "--new", "main_second"},
		{"--old", "main_first", "--new", "g_main_value"},
		{"--old", "missing", "--new", "main_new"},
	} {
		if err := p.symbolsCommand(append([]string{"rename-function"}, args...)); err == nil {
			t.Fatalf("%v accepted", args)
		}
	}
	if readTestFile(t, p, "target/main.yaml") != before {
		t.Fatal("a rejected rename changed the configuration")
	}
}

func TestRenameFunctionKeepsConceptAndSharedUnitPaths(t *testing.T) {
	for _, source := range []string{"src/lib/calendar.c", "src/lib/main_first.c"} {
		for _, sharedUnit := range []bool{false, true} {
			if !sharedUnit && strings.HasSuffix(source, "/main_first.c") {
				continue // The existing standalone-source test covers this move.
			}
			t.Run(source+fmt.Sprint(sharedUnit), func(t *testing.T) {
				p := validTestProject(t)
				config, err := p.loadProjectConfig()
				if err != nil {
					t.Fatal(err)
				}
				main, _ := config.module("main")
				main.Functions[0].Source = source
				body := "void main_first(void) {}\n"
				if sharedUnit {
					battle, _ := config.module("battle")
					battle.Units = []unitDef{{Name: "calendar", Source: source}}
					battle.Functions[0].Unit = "calendar"
					battle.Functions[0].Source = ""
					battle.Functions[0].SourceSymbol = "main_first"
					battle.Functions = append(battle.Functions, functionSpec{
						Addr: battle.Functions[0].Addr + uint32(battle.Functions[0].Size),
						Size: 4, Name: "calendar_reverse", Hash: "0123456789abcdef", Unit: "calendar",
					})
					body += "#ifdef FFT_BATTLE\nvoid calendar_reverse(void) {}\n#endif\n"
				}
				for path, contents := range renderConfigFiles(config) {
					writeSymbolRenameFixture(t, p.root, path, string(contents))
				}
				if err := os.Remove(filepath.Join(p.root, "src/main/main_first.c")); err != nil {
					t.Fatal(err)
				}
				writeSymbolRenameFixture(t, p.root, source, body)
				before := main.Functions[0]
				if err := p.symbolsCommand([]string{"rename-function", "--old", "main_first", "--new", "calendar_forward"}); err != nil {
					t.Fatal(err)
				}
				if got := readTestFile(t, p, source); got != strings.ReplaceAll(body, "main_first", "calendar_forward") {
					t.Fatalf("source = %q", got)
				}
				config, err = p.loadProjectConfig()
				if err != nil {
					t.Fatal(err)
				}
				main, _ = config.module("main")
				after := main.function("calendar_forward")
				if after == nil || main.sourceOf(after) != source || after.Addr != before.Addr || after.Size != before.Size || after.Hash != before.Hash || after.Profile != before.Profile {
					t.Fatalf("entry contract changed: %+v", after)
				}
				if sharedUnit {
					battle, _ := config.module("battle")
					if battle.Units[0].Source != source || battle.Functions[0].sourceSymbol() != "calendar_forward" {
						t.Fatal("shared unit source or binding changed")
					}
				}
			})
		}
	}
}

func TestRenameGlobalRenamesDataImportsAndMergesAliases(t *testing.T) {
	p := validTestProject(t)
	if err := p.symbolsCommand([]string{"rename-global", "--old", "g_main_value", "--new", "g_main_counter"}); err != nil {
		t.Fatal(err)
	}
	for _, path := range []string{"target/main.yaml", "target/event.yaml", "target/battle.yaml", "src/event/attack_entry.c"} {
		if text := readTestFile(t, p, path); strings.Contains(text, "g_main_value\n") || strings.Contains(text, "g_main_value}") || !strings.Contains(text, "g_main_counter") {
			t.Fatalf("%s not renamed:\n%s", path, text)
		}
	}
	// An alias at the same address folds into the existing name.
	if err := p.symbolsCommand([]string{"rename-global", "--old", "g_main_value_alias", "--new", "g_main_counter"}); err != nil {
		t.Fatal(err)
	}
	if text := readTestFile(t, p, "target/main.yaml"); strings.Count(text, "g_main_counter") != 1 || strings.Contains(text, "alias") {
		t.Fatalf("alias not merged:\n%s", text)
	}
	if err := p.symbolsCommand([]string{"rename-global", "--old", "main_first", "--new", "main_x"}); err == nil || !strings.Contains(err.Error(), "rename-function") {
		t.Fatalf("function rename through rename-global: %v", err)
	}
}

func TestRenamePairsApplyAsOneTransaction(t *testing.T) {
	p := validTestProject(t)
	writeSymbolRenameFixture(t, p.root, "pairs.tsv", "# old\tnew\nmain_first\tmain_start\nmain_second\tmain_next\n")
	if err := p.symbolsCommand([]string{"rename-function", "--pairs", "pairs.tsv"}); err != nil {
		t.Fatal(err)
	}
	text := readTestFile(t, p, "target/main.yaml")
	if !strings.Contains(text, "name: main_start") || !strings.Contains(text, "name: main_next") {
		t.Fatalf("pairs not applied:\n%s", text)
	}
	writeSymbolRenameFixture(t, p.root, "bad.tsv", "main_start\tmain_again\nmissing\tmain_other\n")
	before := readTestFile(t, p, "target/main.yaml")
	if err := p.symbolsCommand([]string{"rename-function", "--pairs", "bad.tsv"}); err == nil {
		t.Fatal("bad pair accepted")
	}
	if readTestFile(t, p, "target/main.yaml") != before {
		t.Fatal("a failed batch wrote files")
	}
	if _, err := parseRenamePairs([]byte("one\ttwo\tthree\n")); err == nil {
		t.Fatal("three-column pair accepted")
	}
}

func TestSymbolsCommandRejectsMalformedArguments(t *testing.T) {
	p := validTestProject(t)
	for _, args := range [][]string{
		{},
		{"sort"},
		{"rename-function"},
		{"rename-function", "--old", "a"},
		{"rename-function", "--old", "a", "--new", "b", "--pairs", "x"},
		{"rename-global", "--old", "a", "--new", "b", "extra"},
	} {
		if err := p.symbolsCommand(args); err == nil {
			t.Errorf("%v accepted", args)
		}
	}
}
