package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestHeaderDeclarationsSpanAttachedCommentsButNotLabels(t *testing.T) {
	header := "/* menu state */\n/* The live count. */\nextern int g_count;\nvoid menu_x(int a);\nvoid menu_x(int a); /* dup */\nvoid other(void);\n"
	spans, err := headerDeclarations("menu.h", []byte(header), "menu_x")
	if err != nil {
		t.Fatal(err)
	}
	if len(spans) != 2 || header[spans[0].start:spans[0].end] != "void menu_x(int a);\n" || header[spans[1].start:spans[1].end] != "void menu_x(int a); /* dup */\n" {
		t.Fatalf("spans = %+v", spans)
	}
	spans, err = headerDeclarations("menu.h", []byte(header), "g_count")
	if err != nil || len(spans) != 1 || header[spans[0].start:spans[0].end] != "/* The live count. */\nextern int g_count;\n" {
		t.Fatalf("attached comment: %+v %v", spans, err)
	}
	out, err := appendToHeaderGroup([]byte("#ifndef X\n#define X\n\n/* a */\nint a(void);\n\n#endif\n"), "unified", "int b(void);\n")
	if err != nil || string(out) != "#ifndef X\n#define X\n\n/* a */\nint a(void);\n\n/* unified */\nint b(void);\n\n#endif\n" {
		t.Fatalf("new group:\n%s (%v)", out, err)
	}
	out, err = appendToHeaderGroup(out, "unified", "int c(void);\n")
	if err != nil || !strings.Contains(string(out), "/* unified */\nint b(void);\nint c(void);\n\n#endif") {
		t.Fatalf("extended group:\n%s (%v)", out, err)
	}
}

func unifyFixture(t *testing.T) project {
	t.Helper()
	p := validTestProject(t)
	// battle_helper in BATTLE and attack_entry in ATTACK are twins; WORLD-style
	// override binds the BATTLE name to ATTACK's own copy.
	event := strings.Replace(testEventYAML, "overrides:\n  - {function: attack_entry, name: battle_helper, addr: 0x80070000}\n",
		"overrides:\n  - {function: attack_entry, name: battle_helper, addr: 0x801bf000}\n", 1)
	writeSymbolRenameFixture(t, p.root, "target/event.yaml", event)
	writeSymbolRenameFixture(t, p.root, "src/battle/battle_helper.c", "#include \"fft/battle_menu.h\"\nvoid battle_helper(void) { g_battle_value; }\n")
	writeSymbolRenameFixture(t, p.root, "src/event/attack_entry.c", "#include \"fft/event_attack.h\"\nvoid attack_entry(void) { g_main_value; }\n")
	writeSymbolRenameFixture(t, p.root, "include/fft/menu.h", "#ifndef MENU_H\n#define MENU_H\n\n/* menus */\nextern int g_menu_other;\n\n#endif\n")
	writeSymbolRenameFixture(t, p.root, "include/fft/battle_menu.h", "#ifndef BATTLE_MENU_H\n#define BATTLE_MENU_H\n#include \"fft/menu.h\"\n/* BATTLE's copy. */\nvoid battle_helper(void);\nextern int g_battle_value;\n#endif\n")
	writeSymbolRenameFixture(t, p.root, "include/fft/event_attack.h", "#ifndef EVENT_ATTACK_H\n#define EVENT_ATTACK_H\n#include \"fft/menu.h\"\nvoid attack_entry(void);\nextern int g_main_value;\n#endif\n")
	return p
}

func TestUnifyRenamesCopiesAcrossModulesAndKeepsOneDeclaration(t *testing.T) {
	p := unifyFixture(t)
	// Two bodies stay per module, so the shared name carries the suffix.
	if err := p.symbolsCommand([]string{"unify", "--new", "menu_helper_variant", "--header", "fft/menu.h", "battle_helper", "attack_entry"}); err != nil {
		t.Fatal(err)
	}
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	battle, _ := config.module("battle")
	attack, _ := config.module("event-attack")
	if battle.function("menu_helper_variant") == nil || attack.function("menu_helper_variant") == nil || len(attack.Overrides) != 0 {
		t.Fatalf("config: battle %+v attack %+v", battle.Functions, attack.Overrides)
	}
	if _, err := os.Stat(filepath.Join(p.root, "src", "battle", "menu_helper_variant.c")); err != nil {
		t.Fatal("battle source was not moved")
	}
	if got := readTestFile(t, p, "src/event/menu_helper_variant.c"); !strings.Contains(got, "void menu_helper_variant(void)") {
		t.Fatalf("attack source = %q", got)
	}
	menu := readTestFile(t, p, "include/fft/menu.h")
	if !strings.Contains(menu, "/* unified */\nvoid menu_helper_variant(void);\n") {
		t.Fatalf("menu.h = %q", menu)
	}
	for _, header := range []string{"include/fft/battle_menu.h", "include/fft/event_attack.h"} {
		if text := readTestFile(t, p, header); strings.Contains(text, "menu_helper_variant") || strings.Contains(text, "BATTLE's copy") {
			t.Fatalf("%s still declares the symbol: %q", header, text)
		}
	}
	if err := p.validateProjectConfig(config); err != nil {
		t.Fatal(err)
	}
	// Data names unify the same way, and their declaration moves too.
	if err := p.symbolsCommand([]string{"unify", "--new", "g_menu_value", "--header", "fft/menu.h", "g_battle_value"}); err != nil {
		t.Fatal(err)
	}
	if got := readTestFile(t, p, "src/battle/menu_helper_variant.c"); !strings.Contains(got, "g_menu_value;") {
		t.Fatalf("battle source = %q", got)
	}
	menu = readTestFile(t, p, "include/fft/menu.h")
	if !strings.Contains(menu, "void menu_helper_variant(void);\nextern int g_menu_value;\n") || strings.Contains(readTestFile(t, p, "include/fft/battle_menu.h"), "g_menu_value") {
		t.Fatalf("declarations: %q", menu)
	}
	// A module holding both names is a real collision for data too.
	if err := p.symbolsCommand([]string{"unify", "--new", "g_main_value", "g_menu_value"}); err == nil || !strings.Contains(err.Error(), "different addresses") {
		t.Fatalf("data collision: %v", err)
	}
	// A module holding both names is a real collision.
	if err := p.symbolsCommand([]string{"unify", "--new", "main_second", "main_first"}); err == nil || !strings.Contains(err.Error(), "already exists") {
		t.Fatalf("collision: %v", err)
	}
}

func TestShareBindsEntriesToOneSourceAndRemovesTwins(t *testing.T) {
	p := unifyFixture(t)
	if err := p.symbolsCommand([]string{"share", "--to", "src/shared/menu/battle_helper.c", "battle:battle_helper", "attack:attack_entry"}); err != nil {
		t.Fatal(err)
	}
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	battle, _ := config.module("battle")
	attack, _ := config.module("event-attack")
	b, a := battle.function("battle_helper"), attack.function("attack_entry")
	if b.Source != "src/shared/menu/battle_helper.c" || b.SourceSymbol != "" || a.Source != b.Source || a.SourceSymbol != "battle_helper" {
		t.Fatalf("bindings: %+v %+v", b, a)
	}
	for _, gone := range []string{"src/battle/battle_helper.c", "src/event/attack_entry.c"} {
		if _, err := os.Stat(filepath.Join(p.root, filepath.FromSlash(gone))); !os.IsNotExist(err) {
			t.Fatalf("%s still exists", gone)
		}
	}
	if err := p.validateProjectConfig(config); err != nil {
		t.Fatal(err)
	}
	if err := p.symbolsCommand([]string{"share", "--to", "src/shared/menu/x.c", "battle:nowhere"}); err == nil || !strings.Contains(err.Error(), "no function") {
		t.Fatalf("unknown entry: %v", err)
	}
	if err := p.symbolsCommand([]string{"share", "--to", "build/x.c", "battle:battle_helper"}); err == nil || !strings.Contains(err.Error(), "under src/") {
		t.Fatalf("outside src: %v", err)
	}
}
