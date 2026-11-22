package main

import (
	"errors"
	"reflect"
	"strings"
	"testing"
)

func TestOrderBreaksFindsReorderedNeighbors(t *testing.T) {
	shared, breaks := orderBreaks([]string{"a", "b", "only_a", "c", "d"}, []string{"a", "b", "c", "only_b", "d"})
	if shared != 4 || len(breaks) != 0 {
		t.Fatalf("inserted functions are not order changes: %d %v", shared, breaks)
	}
	shared, breaks = orderBreaks([]string{"mode", "area", "move", "tail"}, []string{"move", "area", "mode", "tail"})
	want := []orderBreak{{"mode", "area"}, {"area", "move"}, {"move", "tail"}}
	if shared != 4 || !reflect.DeepEqual(breaks, want) {
		t.Fatalf("reversed run: %d %v", shared, breaks)
	}
}

// orderFixture builds a module whose functions sit 16 bytes apart in the
// given order; each entry is "name@source".
func orderFixture(id string, entries ...string) *moduleSpec {
	m := &moduleSpec{ID: id, SourceDir: "src/" + id, Profile: defaultProfile, Load: 0x80010000}
	for index, entry := range entries {
		name, source, _ := strings.Cut(entry, "@")
		m.Functions = append(m.Functions, functionSpec{Addr: 0x80010000 + uint32(16*index), Size: 16, Name: name, Source: source})
	}
	return m
}

func TestOrderReportSeparatesSharedSourcesFromSplitNames(t *testing.T) {
	battle := orderFixture("battle",
		"run@src/lib/script.c", "battle_only@src/lib/script.c", "stop@src/lib/script.c",
		"frame@src/battle/frame.c", "input@src/battle/frame.c",
		"mode@src/lib/mode.c", "area@src/lib/area.c")
	world := orderFixture("world",
		"run@src/lib/script.c", "stop@src/lib/script.c",
		"frame@src/world/frame.c", "input@src/world/frame.c",
		"area@src/lib/area.c", "mode@src/lib/mode.c")
	world.Functions[2].Profile = "gcc-2.6.3_O1_aspsx-2.34"
	config := &projectConfig{Modules: []*moduleSpec{battle, world}}
	noImage := func(*moduleSpec) ([]byte, error) { return nil, errors.New("no disc") }
	report := buildOrderReport(config, config.Modules, t.TempDir(), noImage)

	if len(report.Pairs) != 1 || report.Pairs[0].Shared != 6 || len(report.Pairs[0].Breaks) != 2 {
		t.Fatalf("pairs = %+v", report.Pairs)
	}
	sources := map[string]orderSource{}
	for _, source := range report.Sources {
		sources[source.Source] = source
	}
	script := sources["src/lib/script.c"]
	if script.Order != "same" || script.Functions != 3 || script.Partial != 1 || len(script.Modules) != 2 {
		t.Fatalf("script.c = %+v", script)
	}
	if _, listed := sources["src/battle/frame.c"]; listed || len(sources) != 3 {
		t.Fatalf("sources = %+v", report.Sources)
	}
	if len(report.Splits) != 1 {
		t.Fatalf("splits = %+v", report.Splits)
	}
	split := report.Splits[0]
	if split.Order != "same" || split.Shared != 2 || len(split.Files) != 2 || split.Files[0].Source != "src/battle/frame.c" {
		t.Fatalf("split = %+v", split)
	}
	if split.Members[0].Name != "frame" || !split.Members[0].ProfileGap || split.Members[0].Compared || split.Members[1].ProfileGap {
		t.Fatalf("members = %+v", split.Members)
	}
	if report.Totals.PairsReordered != 1 || report.Totals.SplitsOrdered != 1 || report.Totals.SourcesReordered != 0 {
		t.Fatalf("totals = %+v", report.Totals)
	}
	if text := renderOrderMarkdown(report); !strings.Contains(text, "| battle | world | 6 | 2 |") || !strings.Contains(text, "frame | battle | world") {
		t.Fatalf("summary:\n%s", text)
	}
}

func TestOrderReportFlagsSourcesAndSplitsThatChangeOrder(t *testing.T) {
	battle := orderFixture("battle", "first@src/lib/pair.c", "second@src/lib/pair.c", "open@src/battle/menu.c", "close@src/battle/menu.c")
	world := orderFixture("world", "second@src/lib/pair.c", "first@src/lib/pair.c", "close@src/world/menu.c", "open@src/world/menu.c")
	config := &projectConfig{Modules: []*moduleSpec{battle, world}}
	report := buildOrderReport(config, config.Modules, t.TempDir(), func(*moduleSpec) ([]byte, error) { return nil, errors.New("no disc") })
	if len(report.Sources) != 1 || report.Sources[0].Order != "differs" || report.Sources[0].Detail == "" {
		t.Fatalf("sources = %+v", report.Sources)
	}
	if len(report.Splits) != 1 || report.Splits[0].Order != "differs" {
		t.Fatalf("splits = %+v", report.Splits)
	}
}

func TestInstructionDifferencesCountsRunsAndUnmatchedSides(t *testing.T) {
	a := []string{"lui", "lw", "jal", "nop", "addiu", "sw", "sw", "jr", "nop"}
	b := []string{"lui", "lw", "addiu", "sw", "sw", "mflo", "jr", "nop"}
	places, onlyA, onlyB := instructionDifferences(a, b)
	if places != 2 || onlyA != 2 || onlyB != 1 {
		t.Fatalf("differences = %d places, %d/%d", places, onlyA, onlyB)
	}
	if places, onlyA, onlyB := instructionDifferences(a, a); places != 0 || onlyA != 0 || onlyB != 0 {
		t.Fatalf("identical streams differ: %d %d %d", places, onlyA, onlyB)
	}
}

func TestProgramConditionalCountIgnoresOrdinaryGuards(t *testing.T) {
	source := "#ifndef HEADER_H\n#ifdef FFT_BATTLE\n#endif\n#if VARIANT_X == 2\n#elif defined(FFT_WORLD)\n#endif\n#endif\n"
	if got := programConditionalCount(source); got != 2 {
		t.Fatalf("conditionals = %d", got)
	}
}
