package main

import (
	"bytes"
	"path/filepath"
	"strings"
	"testing"
)

// tableUnitFixture adds a unit that owns one initialized table and no
// functions to the test project's CARD module.
func tableUnitFixture(t *testing.T) (project, *projectConfig, *moduleSpec, []byte) {
	t.Helper()
	raw := []byte{1, 2, 3, 4, 5, 6, 7, 8}
	card := "profile: gcc-2.6.3_O0_aspsx-2.34\n"
	event := strings.Replace(testEventYAML, card, card+"\nunits:\n  - {name: card_tables, source: src/event/card_tables.c, data: [{addr: 0x801bf080, size: 8, hash: "+shortHash(raw)+"}]}\n", 1)
	event += "\ndata:\n  - {addr: 0x801bf080, name: g_card_table}\n"
	p := testProject(t, map[string]string{
		"target/main.yaml":           testMainYAML,
		"target/event.yaml":          event,
		"target/battle.yaml":         testBattleYAML,
		"src/main/main_first.c":      "void main_first(void) {}\n",
		"src/main/main_second.c":     "void main_second(void) {}\n",
		"src/event/attack_entry.c":   "void attack_entry(void) { g_main_value; g_battle_value; }\n",
		"src/event/card_entry.c":     "void card_entry(void) {}\n",
		"src/event/card_tables.c":    "unsigned char g_card_table[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };\n",
		"src/battle/battle_helper.c": "void battle_helper(void) {}\n",
	})
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	return p, config, config.byID["event-card"], raw
}

func TestTableUnitResolvesWithoutFunctions(t *testing.T) {
	p, config, m, _ := tableUnitFixture(t)
	unit, err := m.unitInstance(config, "card_tables")
	if err != nil {
		t.Fatal(err)
	}
	if len(unit.members) != 0 || unit.size != 0 || unit.profile.name != m.Profile {
		t.Fatalf("table unit = %d members, %d bytes, profile %s", len(unit.members), unit.size, unit.profile.name)
	}
	script := unitLinkerScript(unit, nil, true)
	if !strings.Contains(script, ".data 0x") || strings.Contains(script, ".text") {
		t.Fatalf("table unit linker script places text or omits data:\n%s", script)
	}
	if err := p.validateProjectConfig(config); err != nil {
		t.Fatal(err)
	}
}

func TestTableUnitMustOwnData(t *testing.T) {
	p, config, m, _ := tableUnitFixture(t)
	m.Units[len(m.Units)-1].Data = nil
	err := p.validateProjectConfig(config)
	if err == nil || !strings.Contains(err.Error(), "owns no data") {
		t.Fatalf("a unit with neither functions nor data was accepted: %v", err)
	}
}

func TestTableUnitSourceDefinesNoFunctions(t *testing.T) {
	p, config, _, _ := tableUnitFixture(t)
	writeTestFile(t, filepath.Join(p.root, "src/event/card_tables.c"),
		[]byte("unsigned char g_card_table[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };\nvoid card_helper(void) {}\n"))
	err := p.validateProjectConfig(config)
	if err == nil || !strings.Contains(err.Error(), "card_helper") {
		t.Fatalf("a table unit defining a function was accepted: %v", err)
	}
}

func TestTableUnitCompilesItsTable(t *testing.T) {
	requirePinnedToolchain(t)
	p, config, m, raw := tableUnitFixture(t)
	unit, err := m.unitInstance(config, "card_tables")
	if err != nil {
		t.Fatal(err)
	}
	t.Setenv("TOOLS_CACHE", "0")
	source := filepath.Join(p.root, unit.spec.source)
	sections, err := p.compileUnitInstance(newSymbolResolver(config), unit, source, filepath.Join(p.root, "work"))
	if err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(sections[".data"], raw) || len(sections[".text"]) != 0 {
		t.Fatalf(".data = %x, .text = %d bytes", sections[".data"], len(sections[".text"]))
	}
	if _, problems := compareUnitSections(unit, sections); len(problems) != 0 {
		t.Fatal(problems)
	}
	if spans := sliceUnitOwnedData(unit, sections); len(spans) != 1 || !bytes.Equal(spans[0].data, raw) {
		t.Fatalf("owned spans = %+v", spans)
	}
}
