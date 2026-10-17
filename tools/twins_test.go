package main

import (
	"encoding/binary"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
)

func mipsWords(words ...uint32) []byte {
	code := make([]byte, 4*len(words))
	for index, word := range words {
		binary.LittleEndian.PutUint32(code[4*index:], word)
	}
	return code
}

func TestTwinSymbolizeResolvesOperandsAndStripsDivisionChecks(t *testing.T) {
	p := validTestProject(t)
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	m, _ := config.module("main")
	scope := newTwinScope(config, m)
	probe := &functionSpec{Addr: 0x80010200, Name: "probe"}
	// lui v0,%hi(g_main_value); lw v0,%lo(g_main_value)(v0); jal main_first; nop;
	// beq v0,zero,past the division; nop; div zero,v0,v1; [check]; mflo v0; jr ra; nop
	checked := mipsWords(0x3c028001, 0x8c420100, 0x0c004000, 0, 0x1040000b, 0, 0x0043001a,
		0x14600002, 0, 0x0007000d, 0x2401ffff, 0x14610004, 0x3c018000, 0x14410002, 0, 0x0006000d,
		0x00001012, 0x03e00008, 0)
	unchecked := mipsWords(0x3c028001, 0x8c420100, 0x0c004000, 0, 0x10400002, 0, 0x0043001a,
		0x00001012, 0x03e00008, 0)
	probe.Size = len(checked)
	a := twinSymbolize(m, probe, checked, scope)
	probe.Size = len(unchecked)
	b := twinSymbolize(m, probe, unchecked, scope)
	want := "lui\tv0,%hi(#0)\nlw\tv0,%lo(#0)(v0)\njal\t#1\nnop\nbeqz\tv0,@7\nnop\ndiv\tzero,v0,v1\nmflo\tv0\njr\tra\nnop"
	if got := strings.Join(a.tokens, "\n"); got != want {
		t.Fatalf("checked stream:\n%s\nwant:\n%s", got, want)
	}
	if got := strings.Join(b.tokens, "\n"); got != want {
		t.Fatalf("unchecked stream:\n%s", got)
	}
	if a.divchecks != 1 || b.divchecks != 0 || strings.Join(a.symbols, ",") != "g_main_value,main_first" || a.unresolved != 0 {
		t.Fatalf("divchecks %d/%d symbols %v unresolved %d", a.divchecks, b.divchecks, a.symbols, a.unresolved)
	}
	if twinKey(a.tokens) != twinKey(b.tokens) || twinKey(a.mnemonics) != twinKey(b.mnemonics) {
		t.Fatal("checked and unchecked streams key differently")
	}
	// An address nothing names becomes an unresolved slot; one inside a
	// function names it with an offset.
	probe.Size = 8
	c := twinSymbolize(m, probe, mipsWords(0x3c028002, 0x8c420004), scope)
	if c.unresolved != 1 || c.symbols[0] != "0x80020004" {
		t.Fatalf("unresolved: %v (%d)", c.symbols, c.unresolved)
	}
	d := twinSymbolize(m, probe, mipsWords(0x3c028001, 0x24420004), scope)
	if d.unresolved != 0 || d.tokens[1] != "addiu\tv0,v0,%lo(#0+0x4)" || d.symbols[0] != "main_first" {
		t.Fatalf("interior: %v %v", d.tokens, d.symbols)
	}
	// lui at,%hi(g_main_value); addu at,at,v1; lw v0,%lo(g_main_value)(at)
	probe.Size = 12
	e := twinSymbolize(m, probe, mipsWords(0x3c018001, 0x00230821, 0x8c220100), scope)
	if strings.Join(e.tokens, "\n") != "lui\tat,%hi(#0)\naddu\tat,at,v1\nlw\tv0,%lo(#0)(at)" || e.symbols[0] != "g_main_value" {
		t.Fatalf("indexed: %v %v", e.tokens, e.symbols)
	}
}

func twinFixture(module, name, source string, symbols ...string) *twinFunction {
	m := &moduleSpec{ID: module, SourceDir: "src/" + module, Profile: defaultProfile}
	f := &functionSpec{Name: name, Size: 64, Source: source}
	tokens := []string{"jal\t#0", "nop"}
	for index := range symbols[1:] {
		tokens = append(tokens, "lw\tv0,%lo(#"+string(rune('1'+index))+")(v0)")
	}
	return &twinFunction{m: m, f: f, source: source, tokens: tokens, mnemonics: tokens, symbols: symbols}
}

func TestTwinGroupClassifiesByNamesAndSources(t *testing.T) {
	shared := twinGroup([]*twinFunction{
		twinFixture("battle", "menu_x", "src/menu/menu_x.c", "thread_yield", "g_a"),
		twinFixture("world", "menu_x", "src/menu/menu_x.c", "thread_yield", "g_a"),
	})
	if shared.Status != "shared" || shared.Unlocked != 0 || shared.Unified != 2 {
		t.Fatalf("shared = %+v", shared)
	}
	mergeable := twinGroup([]*twinFunction{
		twinFixture("battle", "menu_x", "src/battle/menu_x.c", "thread_yield", "g_a"),
		twinFixture("world", "menu_x", "src/world/menu_x.c", "thread_yield", "g_a"),
	})
	if mergeable.Status != "mergeable" || mergeable.Unlocked != 64 || len(mergeable.Renames) != 0 {
		t.Fatalf("mergeable = %+v", mergeable)
	}
	renames := twinGroup([]*twinFunction{
		twinFixture("world", "world_menu_x", "src/world/world_menu_x.c", "thread_yield", "g_world_a"),
		twinFixture("battle", "battle_menu_x", "src/battle/battle_menu_x.c", "thread_yield", "g_battle_a"),
	})
	if renames.Status != "needs-renames" || renames.Unified != 1 || len(renames.Renames) != 1 ||
		renames.Renames[0]["battle"] != "g_battle_a" || renames.Renames[0]["world"] != "g_world_a" || renames.Members[0].Module != "battle" {
		t.Fatalf("renames = %+v", renames)
	}
}

func TestSplitAmbiguousPairsShapesAcrossModulesByStrippedNames(t *testing.T) {
	members := []*twinFunction{
		twinFixture("battle", "effect_call_play", "src/battle/effect_call_play.c", "effect_play"),
		twinFixture("battle", "menu_alloc_buffer", "src/battle/menu_alloc_buffer.c", "menu_alloc_memory"),
		twinFixture("world", "menu_alloc_buffer", "src/world/menu_alloc_buffer.c", "menu_alloc_memory"),
		twinFixture("world", "world_game_reset", "src/world/world_game_reset.c", "world_reset"),
		twinFixture("event-card", "card_bits_read_primary", "src/event/card_bits_read_primary.c", "g_card_primary"),
		twinFixture("event-equip", "equip_bits_read_primary", "src/event/equip_bits_read_primary.c", "g_equip_primary"),
		twinFixture("event-card", "card_other", "src/event/card_other.c", "g_card_secondary"),
		twinFixture("event-equip", "equip_different", "src/event/equip_different.c", "g_equip_secondary"),
	}
	groups := splitAmbiguous(members)
	var got []string
	for _, group := range groups {
		var names []string
		for _, fn := range group {
			names = append(names, fn.label())
		}
		got = append(got, strings.Join(names, "+"))
	}
	want := "battle:menu_alloc_buffer+world:menu_alloc_buffer event-card:card_bits_read_primary+event-equip:equip_bits_read_primary event-card:card_other+event-equip:equip_different"
	if strings.Join(got, " ") != want {
		t.Fatalf("groups = %v", got)
	}
	plain := []*twinFunction{members[2], members[5]}
	if groups := splitAmbiguous(plain); len(groups) != 1 || len(groups[0]) != 2 {
		t.Fatalf("unambiguous members were split: %v", groups)
	}
	for name, want := range map[string]string{"world_menu_close": "menu_close", "g_card_gfx_context": "g_gfx_context", "menu_x": "menu_x", "g_x": "g_x"} {
		if got := stripModulePrefix(name); got != want {
			t.Errorf("stripModulePrefix(%s) = %s, want %s", name, got, want)
		}
	}
}

func TestTwinGroupTrialsSubstituteRenamesBothWays(t *testing.T) {
	p := validTestProject(t)
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	group := twinsGroup{ID: "t0001", Status: "needs-renames",
		Members: []twinsMember{
			{Module: "battle", Name: "battle_helper", Source: "src/battle/battle_helper.c"},
			{Module: "event-attack", Name: "attack_entry", Source: "src/event/attack_entry.c"},
		},
		Renames: []map[string]string{{"battle": "g_battle_value", "event-attack": "g_main_value"}},
	}
	specs, err := p.twinGroupTrials(config, group)
	if err != nil {
		t.Fatal(err)
	}
	if len(specs) != 2 {
		t.Fatalf("specs = %+v", specs)
	}
	first := specs[0]
	if first.Module != "event-attack" || first.Entry != "attack_entry" || first.Source != "src/battle/battle_helper.c" || first.Symbol != "battle_helper" || first.Subst["g_battle_value"] != "g_main_value" {
		t.Fatalf("first = %+v", first)
	}
	second := specs[1]
	if second.Module != "battle" || second.Source != "src/event/attack_entry.c" || second.Subst["g_main_value"] != "g_battle_value" {
		t.Fatalf("second = %+v", second)
	}
	if err := p.twinsCommand([]string{"--out=src/twins"}); err == nil || !strings.Contains(err.Error(), "under build/") {
		t.Fatalf("out outside build: %v", err)
	}
	if err := p.twinsCommand([]string{"--module=nowhere"}); err == nil || !strings.Contains(err.Error(), "unknown module") {
		t.Fatalf("unknown module: %v", err)
	}
	if _, err := p.twinTrials(config, "build/analysis/twins/none.yaml", []string{"t0001"}); err == nil {
		t.Fatal("missing report accepted")
	}
}

func TestTwinTrialIncludesUseCurrentSourceAndProgramView(t *testing.T) {
	p := validTestProject(t)
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	m, _ := config.module("event-attack")
	f := m.function("attack_entry")
	// The current unit binding, not the report's old source path, owns the
	// destination dependencies. Keep only its active includes, in source order.
	m.Units = []unitDef{{Name: "entry", Source: "src/event/entry.c"}}
	f.Unit, f.Source = "entry", ""
	m.Defines = []defineSpec{{Name: "VARIANT_DETAIL", Value: "2"}}
	writeTestFile(t, filepath.Join(p.root, "src/event/entry.c"), []byte(`#include "fft/menu.h"
#if defined(FFT_EVENT) && defined(FFT_EVENT_ATTACK)
#include "fft/deploy.h"
#else
#include "fft/unrelated.h"
#endif
#if VARIANT_DETAIL == 2
# include <psx/types.h> /* the selected detail */
#endif
#include "fft/menu.h"
void attack_entry(void) {}
`))
	group := twinsGroup{ID: "t0001", Members: []twinsMember{
		{Module: "battle", Name: "battle_helper", Source: "src/battle/battle_helper.c"},
		{Module: "event-attack", Name: "attack_entry", Source: "src/event/old_entry.c"},
	}}
	specs, err := p.twinGroupTrials(config, group)
	if err != nil {
		t.Fatal(err)
	}
	want := []string{"fft/menu.h", "fft/deploy.h", "psx/types.h"}
	if len(specs) != 2 || !reflect.DeepEqual(specs[0].Includes, want) || len(specs[1].Includes) != 0 {
		t.Fatalf("trial includes = %+v, want %v and no additions", specs, want)
	}
	writeTestFile(t, filepath.Join(p.root, "src/event/entry.c"), []byte("#include HEADER_MACRO\nvoid attack_entry(void) {}\n"))
	if _, err := p.twinGroupTrials(config, group); err == nil || !strings.Contains(err.Error(), "literal include path") {
		t.Fatalf("macro include should need an explicit trial: %v", err)
	}
}

func TestTwinTrialsCompileWithDestinationConceptHeaders(t *testing.T) {
	requirePinnedToolchain(t)
	p := validTestProject(t)
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	// Each source includes only the concept that declares its own global.
	// A cross-source trial also needs the other global's owning header.
	writeTestFile(t, filepath.Join(p.root, "include/fft/left.h"), []byte("extern int g_battle_value;\n"))
	writeTestFile(t, filepath.Join(p.root, "include/fft/right.h"), []byte("extern int g_main_value;\n"))
	writeTestFile(t, filepath.Join(p.root, "src/battle/battle_helper.c"), []byte("#include \"fft/left.h\"\nint battle_helper(void) { return g_battle_value; }\n"))
	writeTestFile(t, filepath.Join(p.root, "src/event/attack_entry.c"), []byte("#include \"fft/right.h\"\nint attack_entry(void) { return g_main_value; }\n"))
	resolver := newSymbolResolver(config)
	group := twinsGroup{ID: "t0001", Members: []twinsMember{
		{Module: "battle", Name: "battle_helper", Source: "src/battle/battle_helper.c"},
		{Module: "event-attack", Name: "attack_entry", Source: "src/event/attack_entry.c"},
	}, Renames: []map[string]string{{"battle": "g_battle_value", "event-attack": "g_main_value"}}}
	for _, member := range group.Members {
		m, _ := config.module(member.Module)
		f := m.function(member.Name)
		sections, err := p.compileFunction(resolver, m, f, filepath.Join(p.root, "build/learn", member.Module))
		if err != nil {
			t.Fatal(err)
		}
		f.Size, f.Hash = len(sections[".text"]), shortHash(sections[".text"])
	}
	specs, err := p.twinGroupTrials(config, group)
	if err != nil {
		t.Fatal(err)
	}
	for index, spec := range specs {
		result := p.runTrial(config, resolver, map[string][]byte{}, spec, index, 4)
		if result.Status != "match" {
			t.Fatalf("cross-source trial: %+v", result)
		}
	}
}
