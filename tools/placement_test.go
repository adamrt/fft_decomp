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
	strong := placementSignals{prev: "menu", next: "menu", unitDominant: "menu", prefix: "menu",
		callees: map[string]int{"menu": 2}, calleeTotal: 2, callers: map[string]int{}}
	sub, score, evidence := placementScore(strong)
	if sub != "menu" || score != 7 || len(evidence) != 4 {
		t.Fatalf("strong signals: %s %d %v", sub, score, evidence)
	}
	if verdict := placementVerdict(sub, score, strong, 4); verdict != "move" {
		t.Fatalf("strong verdict = %s", verdict)
	}
	unitOnly := placementSignals{unitDominant: "menu", prefix: "menu", callees: map[string]int{}, callers: map[string]int{}}
	sub, score, _ = placementScore(unitOnly)
	if verdict := placementVerdict(sub, score, unitOnly, 4); sub != "menu" || score != 3 || verdict != "review" {
		t.Fatalf("unit-only: %s %d %s", sub, score, verdict)
	}
	weak := placementSignals{prev: "text", prefix: "map", callees: map[string]int{}, callers: map[string]int{}}
	sub, score, _ = placementScore(weak)
	if verdict := placementVerdict(sub, score, weak, 4); sub != "map" || score != 1 || verdict != "keep" {
		t.Fatalf("weak: %s %d %s", sub, score, verdict)
	}
	tokens := map[string]string{"menu": "menu", "text": "text", "mem": "memory"}
	folderToken := map[string]string{"menu": "menu", "memory": "mem", "text": "text"}
	cases := []struct{ name, sub, want, note string }{
		{"battle_menu_draw", "menu", "menu_draw", ""},
		{"menu_draw", "menu", "menu_draw", ""},
		{"battle_draw_box", "memory", "mem_draw_box", "prefixed with the destination's token"},
		{"text_x", "menu", "text_x", "name prefix text belongs to text, not menu"},
		{"thread_x_variant", "menu", "", "a _variant body stays beside its module"},
	}
	for _, test := range cases {
		got, note := placementNewName(test.name, test.sub, tokens, folderToken)
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
	if row, ok := rows["menu_b1"]; !ok || row.Subsystem != "menu" || row.Destination != "src/menu/menu_b1.c" || !row.Apply {
		t.Fatalf("menu_b1 row = %+v (present %t)", row, ok)
	}
	if row, ok := rows["menu_b1"]; ok && row.Module != "battle" {
		t.Fatalf("menu_b1 module = %s", row.Module)
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
