package main

import (
	"bytes"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"gopkg.in/yaml.v3"
)

func TestPlacementScoreVerdictAndNames(t *testing.T) {
	strong := placementSignals{prev: "menu", next: "menu", unitDominant: "menu",
		callees: map[string]int{"menu": 2}, calleeTotal: 2, callers: map[string]int{}}
	sub, score, evidence := placementScore(strong)
	if sub != "menu" || score != 6 || len(evidence) != 3 {
		t.Fatalf("strong signals: %s %d %v", sub, score, evidence)
	}
	if verdict := placementVerdict(sub, score, strong, 4); verdict != "move" {
		t.Fatalf("strong verdict = %s", verdict)
	}
	unitOnly := placementSignals{unitDominant: "menu", callees: map[string]int{}, callers: map[string]int{}}
	sub, score, _ = placementScore(unitOnly)
	if verdict := placementVerdict(sub, score, unitOnly, 4); sub != "menu" || score != 2 || verdict != "review" {
		t.Fatalf("unit-only: %s %d %s", sub, score, verdict)
	}
	weak := placementSignals{prev: "text", callees: map[string]int{}, callers: map[string]int{}}
	sub, score, _ = placementScore(weak)
	if verdict := placementVerdict(sub, score, weak, 4); sub != "text" || score != 1 || verdict != "keep" {
		t.Fatalf("weak: %s %d %s", sub, score, verdict)
	}
	cases := []struct{ name, want, note string }{
		{"battle_map_draw", "battle_map_draw", ""},
		{"menu_draw", "menu_draw", ""},
		{"draw_box", "draw_box", ""},
		{"text_x", "text_x", ""},
		{"thread_x_variant", "", "a _variant body stays beside its module"},
	}
	for _, test := range cases {
		got, note := placementNewName(test.name)
		if got != test.want || note != test.note {
			t.Fatalf("%s -> %q %q, want %q %q", test.name, got, note, test.want, test.note)
		}
	}
}

func TestPlacementCommandWritesReportsReadOnlyAndDeterministic(t *testing.T) {
	p := unitsTestProject(t)
	snapshot := func() map[string][32]byte {
		tree := shareAuditTree(t, p.root)
		for path := range tree {
			if strings.HasPrefix(path, "build"+string(filepath.Separator)) {
				delete(tree, path)
			}
		}
		return tree
	}
	before := snapshot()
	var first, second bytes.Buffer
	p.out = &first
	if err := p.placementCommand([]string{"--module=battle"}); err != nil {
		t.Fatal(err)
	}
	data, err := os.ReadFile(filepath.Join(p.root, "build", "analysis", "placement", "battle.yaml"))
	if err != nil {
		t.Fatal(err)
	}
	summary, err := os.ReadFile(filepath.Join(p.root, "build", "analysis", "placement", "summary.md"))
	if err != nil {
		t.Fatal(err)
	}
	var report placementReport
	if err := yaml.Unmarshal(data, &report); err != nil {
		t.Fatal(err)
	}
	rows := map[string]placementRow{}
	for _, row := range append(append([]placementRow(nil), report.Moves...), report.Review...) {
		rows[row.Function] = row
	}
	// Its one shared neighbor used to combine with the menu_ name prefix
	// to produce a suggestion. A name is no longer placement evidence.
	if row, ok := rows["menu_b1"]; ok {
		t.Fatalf("a single shared neighbor should not propose menu_b1: %+v", row)
	}
	if _, ok := rows["map_d1"]; ok || report.Totals["keep"] == 0 {
		t.Fatalf("map_d1 should be kept: totals %v", report.Totals)
	}
	if !strings.Contains(string(summary), "| battle |") {
		t.Fatalf("summary lacks the battle row:\n%s", summary)
	}
	p.out = &second
	if err := p.placementCommand([]string{"--module=battle"}); err != nil {
		t.Fatal(err)
	}
	again, err := os.ReadFile(filepath.Join(p.root, "build", "analysis", "placement", "battle.yaml"))
	if err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(data, again) || first.String() != second.String() {
		t.Fatal("placement report is not deterministic")
	}
	after := snapshot()
	if len(before) != len(after) {
		t.Fatalf("placement changed the tree: %d -> %d files", len(before), len(after))
	}
	for path, hash := range before {
		if after[path] != hash {
			t.Fatalf("placement changed %s", path)
		}
	}
	if err := p.placementCommand([]string{"--out=target"}); err == nil || !strings.Contains(err.Error(), "under build/") {
		t.Fatalf("bad --out error = %v", err)
	}
}

func TestPlacementNestedLibraryPathsAndConceptNames(t *testing.T) {
	for _, test := range []struct{ source, folder, shared string }{
		{"src/lib/menu/menu_draw.c", "lib/menu", "lib/menu"},
		{"src/lib/event/battle_map_update.c", "lib/event", "lib/event"},
		{"src/lib/thread/thread_yield.c", "lib/thread", "lib/thread"},
		{"src/lib/memcard.c", "lib", "lib"},
		{"src/lib/menu/world/menu_draw.c", "lib/menu", ""},
		{"src/world/menu_draw.c", "", ""},
		{"src/platform/psyq/libgpu/DrawOTag.c", "", ""},
	} {
		if got := subsystemFolder(test.source); got != test.folder {
			t.Errorf("subsystemFolder(%q) = %q, want %q", test.source, got, test.folder)
		}
		if got := sharedSubsystemOf(test.source); got != test.shared {
			t.Errorf("sharedSubsystemOf(%q) = %q, want %q", test.source, got, test.shared)
		}
	}
	p := testProject(t, nil)
	an := &unitModuleAnalysis{m: &moduleSpec{ID: "world"}}
	for i, name := range []string{"menu_before", "battle_map_update", "menu_after"} {
		source := "src/lib/menu/" + name + ".c"
		if i == 1 {
			source = "src/world/" + name + ".c"
		}
		an.functions = append(an.functions, unitFunction{
			f: &functionSpec{Name: name, Addr: uint32(0x1000 + i*32), Size: 32}, source: source, group: 0, unit: 0,
		})
	}
	an.groups = []unitGroup{{first: 0, last: 2, units: []unitCandidate{{id: "world.g1.u1", functions: []int{0, 1, 2}}}}}
	report := p.placementReport(an, placementOptions{minScore: 4}, nil)
	if len(report.Moves) != 1 {
		t.Fatalf("moves = %+v", report)
	}
	row := report.Moves[0]
	if row.NewName != "battle_map_update" || row.Subsystem != "lib/menu" || row.Destination != "src/lib/menu/battle_map_update.c" || !row.Apply {
		t.Fatalf("nested placement changed the concept name or folder: %+v", row)
	}
	// An intentionally separate body already in a library's program subfolder
	// must not be proposed for a move into the shared folder.
	an.functions[1].source = "src/lib/menu/world/battle_map_update.c"
	report = p.placementReport(an, placementOptions{minScore: 4}, nil)
	if report.Totals["candidates"] != 0 {
		t.Fatalf("program-specific library body became a candidate: %+v", report)
	}
}

func TestPlacementCounterpartBodiesKeepTheirNameAndProgramFolders(t *testing.T) {
	main := &moduleSpec{ID: "main", Functions: []functionSpec{{Name: "menu_draw", Size: 32}}}
	world := &moduleSpec{ID: "world", Functions: []functionSpec{{Name: "menu_draw", Size: 48}}}
	config := &projectConfig{Modules: []*moduleSpec{main, world}}
	analyses := map[string]*unitModuleAnalysis{
		"main":  {m: main, functions: []unitFunction{{f: &main.Functions[0], source: "src/lib/menu/menu_draw.c"}}},
		"world": {m: world, functions: []unitFunction{{f: &world.Functions[0], source: "src/world/menu_draw.c"}}},
	}
	reports := map[string]*placementReport{
		"main":  {},
		"world": {Review: []placementRow{{Module: "world", Function: "menu_draw", NewName: "menu_draw", Subsystem: "lib/menu"}}},
	}
	families := placementFamilies(config, analyses, reports, nil)
	if len(families) != 1 || families[0].Name != "menu_draw" || families[0].Kind != "variant" {
		t.Fatalf("separate bodies were renamed or lost: %+v", families)
	}
	folders := map[string]string{}
	for _, member := range families[0].Members {
		folders[member.Module] = member.Folder
	}
	if folders["main"] != "src/lib/menu" || folders["world"] != "src/world" {
		t.Fatalf("separate body folders = %v", folders)
	}
}
