package main

import (
	"bytes"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"

	"gopkg.in/yaml.v3"
)

// unitsTestBattleYAML lays out three four-function objects (menu at -O2, menu
// at -O1, text) with a hand-written block between the second and third, then
// a data gap and a two-function object.
const unitsTestBattleYAML = `module: battle
file: BATTLE.BIN
lba: 300
size: 1024
sha256: 33333333333333333333333333333333333333333333333333333333333333dd
load: 0x80067000
source_dir: src/battle
profile: gcc-2.6.3_O2_aspsx-2.34

functions:
  - {addr: 0x80067000, size: 32, name: menu_a1, hash: 0000000000000a01, source: src/menu/menu_a1.c}
  - {addr: 0x80067020, size: 32, name: menu_a2, hash: 0000000000000a02, source: src/menu/menu_a2.c}
  - {addr: 0x80067040, size: 32, name: menu_a3, hash: 0000000000000a03, source: src/menu/menu_a3.c}
  - {addr: 0x80067060, size: 32, name: menu_a4, hash: 0000000000000a04, source: src/menu/menu_a4.c}
  - {addr: 0x80067080, size: 32, name: menu_b1, hash: 0000000000000b01, profile: gcc-2.6.3_O1_aspsx-2.34}
  - {addr: 0x800670a0, size: 32, name: menu_b2, hash: 0000000000000b02, profile: gcc-2.6.3_O1_aspsx-2.34}
  - {addr: 0x800670c0, size: 32, name: menu_b3, hash: 0000000000000b03, profile: gcc-2.6.3_O1_aspsx-2.34}
  - {addr: 0x800670e0, size: 32, name: menu_b4, hash: 0000000000000b04, profile: gcc-2.6.3_O1_aspsx-2.34}
  - {addr: 0x80067120, size: 32, name: text_c1, hash: 0000000000000c01, source: src/text/text_c1.c}
  - {addr: 0x80067140, size: 32, name: text_c2, hash: 0000000000000c02, source: src/text/text_c2.c}
  - {addr: 0x80067160, size: 32, name: text_c3, hash: 0000000000000c03, source: src/text/text_c3.c}
  - {addr: 0x80067180, size: 32, name: text_c4, hash: 0000000000000c04, source: src/text/text_c4.c}
  - {addr: 0x80067300, size: 32, name: map_d1, hash: 0000000000000d01}
  - {addr: 0x80067320, size: 32, name: map_d2, hash: 0000000000000d02}

data:
  - {addr: 0x80067200, name: g_a_table}
  - {addr: 0x80067204, name: g_b_scalar}
  - {addr: 0x80067208, name: g_c_table}
  - {addr: 0x80067340, name: g_d_table}
  - {addr: 0x80067500, name: g_late}

regions:
  - {addr: 0x80067100, end: 0x80067120, kind: handwritten, why: "test context switch"}
`

// unitsTestWorldYAML links the same shared menu and text objects without the
// -O1 object between them.
const unitsTestWorldYAML = `module: world
file: WORLD/WORLD.BIN
lba: 400
size: 1024
sha256: 44444444444444444444444444444444444444444444444444444444444444ee
load: 0x800e0000
source_dir: src/world
profile: gcc-2.6.3_O2_aspsx-2.34

functions:
  - {addr: 0x800e0000, size: 32, name: menu_a1, hash: 1000000000000a01, source: src/menu/menu_a1.c}
  - {addr: 0x800e0020, size: 32, name: menu_a2, hash: 1000000000000a02, source: src/menu/menu_a2.c}
  - {addr: 0x800e0040, size: 32, name: menu_a3, hash: 1000000000000a03, source: src/menu/menu_a3.c}
  - {addr: 0x800e0060, size: 32, name: menu_a4, hash: 1000000000000a04, source: src/menu/menu_a4.c}
  - {addr: 0x800e0080, size: 32, name: text_c1, hash: 1000000000000c01, source: src/text/text_c1.c}
  - {addr: 0x800e00a0, size: 32, name: text_c2, hash: 1000000000000c02, source: src/text/text_c2.c}
  - {addr: 0x800e00c0, size: 32, name: text_c3, hash: 1000000000000c03, source: src/text/text_c3.c}
  - {addr: 0x800e00e0, size: 32, name: text_c4, hash: 1000000000000c04, source: src/text/text_c4.c}

data:
  - {addr: 0x800e0200, name: g_a_table}
  - {addr: 0x800e0204, name: g_c_table}
`

func unitsTestProject(t *testing.T) project {
	t.Helper()
	return testProject(t, map[string]string{
		"target/main.yaml":       testMainYAML,
		"target/battle.yaml":     unitsTestBattleYAML,
		"target/world.yaml":      unitsTestWorldYAML,
		"src/main/main_first.c":  "void main_first(void) {}\n",
		"src/main/main_second.c": "void main_second(void) {}\n",
		"src/menu/menu_a1.c":     "extern int g_a_table[];\nint menu_a1(void) { return g_a_table[0]; }\n",
		"src/menu/menu_a2.c":     "extern int g_a_table[];\nint menu_a2(void) { return menu_a1() + g_a_table[1]; }\n",
		"src/menu/menu_a3.c":     "int menu_a3(void) { return menu_a2(); }\n",
		"src/menu/menu_a4.c":     "int menu_a4(void) { return menu_a3(); }\n",
		"src/battle/menu_b1.c":   "extern int g_b_scalar;\nint menu_b1(void) { return g_b_scalar / 3; }\n",
		"src/battle/menu_b2.c":   "int menu_b2(void) { return menu_b1(); }\n",
		"src/battle/menu_b3.c":   "int menu_b3(void) { return menu_b2(); }\n",
		"src/battle/menu_b4.c":   "int menu_b4(void) { return menu_b3(); }\n",
		"src/text/text_c1.c":     "extern int g_c_table[];\nint text_c1(void) { return g_c_table[0]; }\n",
		"src/text/text_c2.c":     "int text_c2(void) { return text_c1(); }\n",
		"src/text/text_c3.c":     "int text_c3(void) { return text_c2(); }\n",
		"src/text/text_c4.c":     "int text_c4(void) { return text_c3(); }\n",
		"src/battle/map_d1.c":    "extern int g_d_table[];\nextern int g_late;\nint map_d1(void) { return g_d_table[0] + g_late; }\n",
		"src/battle/map_d2.c":    "int map_d2(void) { return map_d1(); }\n",
	})
}

func unitsTestAnalyses(t *testing.T, p project) map[string]*unitModuleAnalysis {
	t.Helper()
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	analyses, err := p.reconstructUnits(config, unitsOptions{minConfidence: "low", maxOwners: 3, outDir: "build/analysis/units"})
	if err != nil {
		t.Fatal(err)
	}
	return analyses
}

func unitCutAfter(t *testing.T, an *unitModuleAnalysis, name string) unitCut {
	t.Helper()
	index, ok := an.byName[name]
	if !ok {
		t.Fatalf("no function %s", name)
	}
	for _, group := range an.groups {
		for _, cut := range group.cuts {
			if cut.after == index {
				return cut
			}
		}
	}
	t.Fatalf("no cut after %s", name)
	return unitCut{}
}

func evidenceKinds(cut unitCut) map[string]int {
	kinds := map[string]int{}
	for _, evidence := range cut.evidence {
		kinds[evidence.Kind] += evidence.Weight
	}
	return kinds
}

func unitFunctionNames(an *unitModuleAnalysis, unit unitCandidate) []string {
	var names []string
	for _, index := range unit.functions {
		names = append(names, an.functions[index].f.Name)
	}
	return names
}

func TestUnitGroupsSplitAtDataGapsAndKeepHandwrittenBlocks(t *testing.T) {
	an := unitsTestAnalyses(t, unitsTestProject(t))["battle"]
	if len(an.groups) != 2 {
		t.Fatalf("groups = %d, want 2", len(an.groups))
	}
	first, second := an.groups[0], an.groups[1]
	if first.textStart != 0x80067000 || first.textEnd != 0x800671a0 || first.last-first.first+1 != 12 {
		t.Fatalf("first group %#x-%#x with %d functions", first.textStart, first.textEnd, first.last-first.first+1)
	}
	if second.textStart != 0x80067300 || first.dataEnd != second.start() {
		t.Fatalf("second group starts at %#x, first data ends at %#x", second.textStart, first.dataEnd)
	}
	cut := unitCutAfter(t, an, "menu_b4")
	if !cut.certain || evidenceKinds(cut)["asm_object"] == 0 || cut.confidence != "high" {
		t.Fatalf("hand-written block boundary: %+v", cut)
	}
	kinds := map[string]string{}
	for _, row := range an.rows {
		kinds[row.names[0]] = row.kind
	}
	want := map[string]string{"g_a_table": "data", "g_b_scalar": "data", "g_c_table": "data", "g_d_table": "data", "g_late": "bss"}
	if !reflect.DeepEqual(kinds, want) {
		t.Fatalf("row kinds = %v, want %v", kinds, want)
	}
	if an.rows[3].group != 1 || an.rows[0].group != 0 {
		t.Fatalf("row groups: g_a_table %d, g_d_table %d", an.rows[0].group, an.rows[3].group)
	}
}

func TestUnitEvidenceFromProfilesCallsDataAndAlignment(t *testing.T) {
	analyses := unitsTestAnalyses(t, unitsTestProject(t))
	battle, world := analyses["battle"], analyses["world"]
	cut := unitCutAfter(t, battle, "menu_a4")
	kinds := evidenceKinds(cut)
	if kinds["profile_object"] != 6 || kinds["call_locality"] != 1 || kinds["alignment_divergence"] != 3 {
		t.Fatalf("evidence after menu_a4 = %v", kinds)
	}
	if cut.confidence != "high" {
		t.Fatalf("confidence after menu_a4 = %s (score %d)", cut.confidence, cut.score)
	}
	straddled := unitCutAfter(t, battle, "menu_a1")
	if evidenceKinds(straddled)["ownership_straddle"] >= 0 || straddled.confidence != "none" {
		t.Fatalf("cut inside the first object: %+v", straddled)
	}
	if kinds := evidenceKinds(unitCutAfter(t, battle, "menu_b4")); kinds["alignment_divergence"] != 3 || kinds["prefix_change"] != 1 {
		t.Fatalf("evidence after menu_b4 = %v", kinds)
	}
	if kinds := evidenceKinds(unitCutAfter(t, world, "menu_a4")); kinds["alignment_divergence"] != 2 {
		t.Fatalf("world evidence after menu_a4 = %v", kinds)
	}
	if len(battle.alignments) != 1 || battle.alignments[0].Partner != "world" || battle.alignments[0].Matched != 8 {
		t.Fatalf("battle alignments: %+v", battle.alignments)
	}
	divergences := battle.alignments[0].Divergences
	if len(divergences) != 1 || divergences[0].Kind != "insertion" || divergences[0].Module != "battle" ||
		strings.Join(divergences[0].Functions, " ") != "menu_b1 menu_b2 menu_b3 menu_b4" {
		t.Fatalf("divergences: %+v", divergences)
	}
	if battle.functions[battle.byName["text_c1"]].aligned["world"] != "text_c1" || world.functions[0].aligned["battle"] != "menu_a1" {
		t.Fatal("aligned partners not recorded")
	}
	if !battle.functions[battle.byName["menu_b1"]].hasDivision || battle.functions[battle.byName["menu_a1"]].hasDivision {
		t.Fatal("division detection")
	}
}

func TestUnitSegmentationFollowsEvidenceAndSizePrior(t *testing.T) {
	analyses := unitsTestAnalyses(t, unitsTestProject(t))
	battle := analyses["battle"]
	var got [][]string
	for _, group := range battle.groups {
		for _, unit := range group.units {
			got = append(got, unitFunctionNames(battle, unit))
		}
	}
	want := [][]string{
		{"menu_a1", "menu_a2", "menu_a3", "menu_a4"},
		{"menu_b1", "menu_b2", "menu_b3", "menu_b4"},
		{"text_c1", "text_c2", "text_c3", "text_c4"},
		{"map_d1", "map_d2"},
	}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("units = %v, want %v", got, want)
	}
	units := battle.groups[0].units
	if units[0].suggestedName != "menu (src/menu)" || units[1].profile != "gcc-2.6.3_O1_aspsx-2.34" || units[2].suggestedName != "text (src/text)" {
		t.Fatalf("unit summaries: %q %q %q", units[0].suggestedName, units[1].profile, units[2].suggestedName)
	}
	if len(units[0].counterparts) != 1 || units[0].counterparts[0].Module != "world" || units[0].counterparts[0].Matched != 4 {
		t.Fatalf("counterparts: %+v", units[0].counterparts)
	}
	for _, function := range battle.functions {
		if function.unit < 0 {
			t.Fatalf("%s is in no unit", function.f.Name)
		}
	}
	for size, want := range map[int]int{1: 3, 2: 2, 3: 1, 4: 0, 40: 0, 60: 2} {
		if got := unitSizePenalty(size); got != want {
			t.Fatalf("size penalty for %d = %d, want %d", size, got, want)
		}
	}
}

func TestLongestIncreasingChainAndDataShift(t *testing.T) {
	chain, reordered := longestIncreasingChain([]unitAlignPair{{0, 0}, {1, 1}, {2, 5}, {3, 2}, {4, 3}, {5, 4}})
	if len(chain) != 5 || len(reordered) != 1 || reordered[0] != (unitAlignPair{2, 5}) {
		t.Fatalf("chain %v reordered %v", chain, reordered)
	}
	an := &unitModuleAnalysis{evidence: map[int][]unitEvidence{}}
	for i := 0; i < 4; i++ {
		an.functions = append(an.functions, unitFunction{index: i, group: 0})
	}
	before := &unitDataRow{names: []string{"g_before"}, owners: []int{0, 1}}
	after := &unitDataRow{names: []string{"g_after"}, owners: []int{2, 3}}
	an.addDataShiftEvidence(before, after, "test")
	if len(an.evidence[1]) != 1 || an.evidence[1][0].Kind != "data_shift" || an.evidence[1][0].Weight != 3 || len(an.evidence) != 1 {
		t.Fatalf("data shift evidence: %v", an.evidence)
	}
	wide := &unitDataRow{names: []string{"g_wide"}, owners: []int{0}, wide: true}
	an.addDataShiftEvidence(wide, after, "test")
	if len(an.evidence) != 1 {
		t.Fatal("widely referenced rows must not place boundaries")
	}
	for name, want := range map[string]string{"battle_menu_open": "menu", "world_text_draw": "text", "script_run": "script", "_start": "start", "bunit_panel_run": "panel"} {
		if got := unitPrefix(name); got != want {
			t.Errorf("unitPrefix(%q) = %q, want %q", name, got, want)
		}
	}
}

func TestUnitsCommandWritesReportsReadOnlyAndDeterministic(t *testing.T) {
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
	if err := p.unitsCommand([]string{"--module=battle,world"}); err != nil {
		t.Fatal(err)
	}
	battleReport, err := os.ReadFile(filepath.Join(p.root, "build", "analysis", "units", "battle.yaml"))
	if err != nil {
		t.Fatal(err)
	}
	summary, err := os.ReadFile(filepath.Join(p.root, "build", "analysis", "units", "summary.md"))
	if err != nil {
		t.Fatal(err)
	}
	p.out = &second
	if err := p.unitsCommand([]string{"--module=battle,world"}); err != nil {
		t.Fatal(err)
	}
	again, err := os.ReadFile(filepath.Join(p.root, "build", "analysis", "units", "battle.yaml"))
	if err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(first.Bytes(), second.Bytes()) || !bytes.Equal(battleReport, again) {
		t.Fatal("command output varies between runs")
	}
	if !reflect.DeepEqual(before, snapshot()) {
		t.Fatal("command writes outside build/")
	}
	var report unitsReport
	if err := yaml.Unmarshal(battleReport, &report); err != nil || !report.CandidateOnly || report.Totals.Groups != 2 || report.Totals.Units != 4 {
		t.Fatalf("battle report: %v %+v", err, report.Totals)
	}
	if !strings.Contains(string(summary), "## battle (BATTLE.BIN)") || !strings.Contains(string(summary), "battle.g1.u002") {
		t.Fatalf("summary:\n%s", summary)
	}
	if !strings.Contains(first.String(), "units: battle: 2 groups, 14 functions, 5 data rows, 4 units") {
		t.Fatalf("stdout: %s", first.String())
	}
	for _, args := range [][]string{{"--min-confidence=huge"}, {"--max-owners=0"}, {"--module=unknown"}, {"--out=src/evil"}, {"unexpected"}} {
		if err := p.unitsCommand(args); err == nil {
			t.Fatalf("accepted invalid args: %v", args)
		}
	}
}
