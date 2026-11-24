package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestRenameFunctionRewritesCommentsAndOrdersShiftedPairs(t *testing.T) {
	p := validTestProject(t)
	writeSymbolRenameFixture(t, p.root, "src/main/main_first.c", "/* main_first starts; see main_second. */\nvoid main_first(void) { main_second(); }\n")
	writeSymbolRenameFixture(t, p.root, "src/main/main_second.c", "// main_second runs after main_first\nvoid main_second(void) { char* s = \"main_first\"; }\n")
	writeSymbolRenameFixture(t, p.root, "include/fft/main.h", "/* main_first, main_second; src/main/main_first.c */\nvoid main_first(void);\nvoid main_second(void);\n")
	writeSymbolRenameFixture(t, p.root, "QUIRKS.md", "main_first calls main_second.\n")
	// The first pair takes the name the second frees, so the second runs first.
	writeSymbolRenameFixture(t, p.root, "build/pairs.tsv", "main_first\tmain_second\nmain_second\tmain_third\n")
	if err := p.symbolsCommand([]string{"rename-function", "--pairs", "build/pairs.tsv"}); err != nil {
		t.Fatal(err)
	}
	// main_second is both an old and a new name: its comment mentions stay.
	for path, want := range map[string]string{
		"src/main/main_second.c": "/* main_second starts; see main_second. */\nvoid main_second(void) { main_third(); }\n",
		"src/main/main_third.c":  "// main_second runs after main_second\nvoid main_third(void) { char* s = \"main_first\"; }\n",
		"include/fft/main.h":     "/* main_second, main_second; src/main/main_first.c */\nvoid main_second(void);\nvoid main_third(void);\n",
		"QUIRKS.md":              "main_second calls main_second.\n",
	} {
		if got := readTestFile(t, p, path); got != want {
			t.Errorf("%s = %q, want %q", path, got, want)
		}
	}
	if _, err := os.Stat(filepath.Join(p.root, "src/main/main_first.c")); !os.IsNotExist(err) {
		t.Fatalf("old source remains: %v", err)
	}
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	main, _ := config.module("main")
	if main.function("main_first") != nil || main.sourceOf(main.function("main_second")) != "src/main/main_second.c" ||
		main.sourceOf(main.function("main_third")) != "src/main/main_third.c" {
		t.Fatalf("rows: %+v", main.Functions)
	}
	writeSymbolRenameFixture(t, p.root, "build/cycle.tsv", "main_second\tmain_third\nmain_third\tmain_second\n")
	if err := p.symbolsCommand([]string{"rename-function", "--pairs", "build/cycle.tsv"}); err == nil || !strings.Contains(err.Error(), "cycle") {
		t.Fatalf("cycle accepted: %v", err)
	}
}

func TestRenameFunctionKeepsAnotherProgramsStaticCopy(t *testing.T) {
	p := validTestProject(t)
	static := "/* battle_helper here is CARD's own. */\nstatic void battle_helper(void) {}\nvoid card_entry(void) { battle_helper(); }\n"
	writeSymbolRenameFixture(t, p.root, "src/event/card_entry.c", static)
	writeSymbolRenameFixture(t, p.root, "include/fft/battle.h", "void battle_helper(void);\n")
	if err := p.symbolsCommand([]string{"rename-function", "--old", "battle_helper", "--new", "battle_run_helper"}); err != nil {
		t.Fatal(err)
	}
	if got := readTestFile(t, p, "src/event/card_entry.c"); got != static {
		t.Fatalf("static copy renamed: %q", got)
	}
	if got := readTestFile(t, p, "src/battle/battle_run_helper.c"); got != "void battle_run_helper(void) {}\n" {
		t.Fatalf("public body = %q", got)
	}
	if got := readTestFile(t, p, "include/fft/battle.h"); got != "void battle_run_helper(void);\n" {
		t.Fatalf("header = %q", got)
	}
	if text := readTestFile(t, p, "target/event.yaml"); !strings.Contains(text, "name: battle_run_helper, addr: 0x80070000") {
		t.Fatalf("override binding not renamed:\n%s", text)
	}

	// Selecting the module renames its static copy instead, alone.
	if err := p.symbolsCommand([]string{"rename-function", "--module", "card", "--old", "battle_helper", "--new", "apply_helper"}); err == nil {
		t.Fatal("renamed a name absent from the selected module")
	}
	p = validTestProject(t)
	writeSymbolRenameFixture(t, p.root, "src/event/card_entry.c", static)
	writeSymbolRenameFixture(t, p.root, "include/fft/battle.h", "void battle_helper(void);\n")
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	card, _ := config.module("event-card")
	card.Functions = append(card.Functions, functionSpec{Addr: 0x801bf004, Size: 4, Name: "battle_helper", Hash: "00000000000000dd", Source: "src/event/card_entry.c"})
	for path, contents := range renderConfigFiles(config) {
		writeSymbolRenameFixture(t, p.root, path, string(contents))
	}
	if err := p.symbolsCommand([]string{"rename-function", "--module", "card", "--old", "battle_helper", "--new", "apply_helper"}); err != nil {
		t.Fatal(err)
	}
	if got, want := readTestFile(t, p, "src/event/card_entry.c"), strings.ReplaceAll(static, "battle_helper", "apply_helper"); got != want {
		t.Fatalf("selected static = %q, want %q", got, want)
	}
	if got := readTestFile(t, p, "src/battle/battle_helper.c"); got != "void battle_helper(void) {}\n" {
		t.Fatalf("unselected body = %q", got)
	}
	if got := readTestFile(t, p, "include/fft/battle.h"); got != "void battle_helper(void);\n" {
		t.Fatalf("header changed for unselected programs: %q", got)
	}
	config, err = p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	card, _ = config.module("event-card")
	battle, _ := config.module("battle")
	attack, _ := config.module("event-attack")
	if card.function("apply_helper") == nil || battle.function("battle_helper") == nil || attack.Overrides[0].Name != "battle_helper" {
		t.Fatalf("rows: card %+v battle %+v attack %+v", card.Functions, battle.Functions, attack.Overrides)
	}
}

func TestRenameFunctionModuleScopeRefusesSharedSources(t *testing.T) {
	p := validTestProject(t)
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	main, _ := config.module("main")
	battle, _ := config.module("battle")
	main.Functions[0].Source = "src/lib/shared.c"
	battle.Functions[0].Source = "src/lib/shared.c"
	for path, contents := range renderConfigFiles(config) {
		writeSymbolRenameFixture(t, p.root, path, string(contents))
	}
	writeSymbolRenameFixture(t, p.root, "src/lib/shared.c", "void main_first(void) { main_second(); }\nvoid battle_helper(void) {}\n")
	before := readTestFile(t, p, "target/main.yaml")
	err = p.symbolsCommand([]string{"rename-function", "--module", "main", "--old", "main_second", "--new", "main_run_second"})
	if err == nil || !strings.Contains(err.Error(), "compiled by main and also battle") {
		t.Fatalf("shared source accepted: %v", err)
	}
	if readTestFile(t, p, "target/main.yaml") != before {
		t.Fatal("refused rename changed the configuration")
	}
}

func TestRenameFunctionJoinsSameRoleNames(t *testing.T) {
	p := validTestProject(t)
	if err := p.symbolsCommand([]string{"rename-function", "--old", "battle_helper", "--new", "main_second"}); err == nil || !strings.Contains(err.Error(), "--join") {
		t.Fatalf("existing name accepted without --join: %v", err)
	}
	if err := p.symbolsCommand([]string{"rename-function", "--join", "--old", "main_first", "--new", "main_second"}); err == nil {
		t.Fatal("joined two names one module holds")
	}
	if err := p.symbolsCommand([]string{"rename-function", "--join", "--old", "battle_helper", "--new", "main_second"}); err != nil {
		t.Fatal(err)
	}
	if got := readTestFile(t, p, "src/battle/main_second.c"); got != "void main_second(void) {}\n" {
		t.Fatalf("joined body = %q", got)
	}
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	battle, _ := config.module("battle")
	attack, _ := config.module("event-attack")
	if battle.function("main_second") == nil || attack.Overrides[0].Name != "main_second" {
		t.Fatalf("rows: battle %+v attack %+v", battle.Functions, attack.Overrides)
	}
	if err := p.validateProjectConfig(config); err != nil {
		t.Fatal(err)
	}
}
