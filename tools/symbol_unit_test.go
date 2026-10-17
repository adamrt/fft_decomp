package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestUnitSymbolsCommandMergesContiguousEntries(t *testing.T) {
	p := unitsTestProject(t)
	if err := p.unitSymbolsCommand([]string{"--name", "menu_b", "--source", "src/battle/menu_b.c", "battle:menu_b2", "battle:menu_b1"}); err != nil {
		t.Fatal(err)
	}
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	battle := config.byID["battle"]
	if battle.unit("menu_b") == nil || battle.unit("menu_b").Source != "src/battle/menu_b.c" {
		t.Fatalf("units = %+v", battle.Units)
	}
	for _, name := range []string{"menu_b1", "menu_b2"} {
		f := battle.function(name)
		if f.Unit != "menu_b" || f.Source != "" || battle.sourceOf(f) != "src/battle/menu_b.c" {
			t.Fatalf("%s = %+v", name, *f)
		}
	}
	data, err := os.ReadFile(filepath.Join(p.root, "src", "battle", "menu_b.c"))
	if err != nil {
		t.Fatal(err)
	}
	if strings.Index(string(data), "menu_b1(void)") > strings.Index(string(data), "menu_b2(void)") {
		t.Fatalf("members out of address order:\n%s", data)
	}
	for _, old := range []string{"menu_b1.c", "menu_b2.c"} {
		if _, err := os.Stat(filepath.Join(p.root, "src", "battle", old)); !os.IsNotExist(err) {
			t.Fatalf("%s should be removed", old)
		}
	}
	if err := validateUnitSource(p.root, "src/battle/menu_b.c", []string{"menu_b1", "menu_b2"}); err != nil {
		t.Fatal(err)
	}
	rendered, err := os.ReadFile(filepath.Join(p.root, "target", "battle.yaml"))
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(string(rendered), "units:\n  - {name: menu_b, source: src/battle/menu_b.c, profile: gcc-2.6.3_O1_aspsx-2.34}") || !strings.Contains(string(rendered), "unit: menu_b") {
		t.Fatalf("rendered config lacks the unit:\n%s", rendered)
	}
	if _, err := p.loadCheckedConfig(); err != nil {
		t.Fatalf("checked config: %v", err)
	}
	if err := p.unitSymbolsCommand([]string{"--name", "gap", "--source", "src/battle/gap.c", "battle:menu_b3", "battle:map_d1"}); err == nil || !strings.Contains(err.Error(), "does not follow") {
		t.Fatalf("gap error = %v", err)
	}
	// Moving a unit member moves the unit's file for every module listing it.
	if err := p.moveSymbolsCommand([]string{"--dir", "src/menu", "battle:menu_b1"}); err != nil {
		t.Fatalf("move unit: %v", err)
	}
	config, err = p.loadCheckedConfig()
	if err != nil {
		t.Fatal(err)
	}
	battle = config.byID["battle"]
	if battle.unit("menu_b").Source != "src/menu/menu_b.c" || battle.sourceOf(battle.function("menu_b2")) != "src/menu/menu_b.c" {
		t.Fatalf("unit not moved: %+v", battle.Units)
	}
	if err := p.moveSymbolsCommand([]string{"--dir", "src/battle", "battle:menu_b2"}); err != nil {
		t.Fatalf("move back: %v", err)
	}
	// A unit file may carry its own leading comment; members still get the includes.
	unitPath := filepath.Join(p.root, "src", "battle", "menu_b.c")
	unitText, err := os.ReadFile(unitPath)
	if err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(unitPath, append([]byte("/* The b menus. */\n\n#include \"fft/menu.h\"\n\n"), unitText...), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := p.splitSymbolsCommand([]string{"--unit", "menu_b"}); err != nil {
		t.Fatal(err)
	}
	if piece, err := os.ReadFile(filepath.Join(p.root, "src", "battle", "menu_b2.c")); err != nil || !strings.HasPrefix(string(piece), "#include \"fft/menu.h\"") || strings.Contains(string(piece), "The b menus") {
		t.Fatalf("split piece lost its includes or kept the file comment:\n%s", piece)
	}
	config, err = p.loadCheckedConfig()
	if err != nil {
		t.Fatal(err)
	}
	battle = config.byID["battle"]
	if battle.unit("menu_b") != nil || battle.function("menu_b1").Unit != "" || battle.sourceOf(battle.function("menu_b2")) != "src/battle/menu_b2.c" {
		t.Fatalf("split left %+v / %+v", battle.Units, *battle.function("menu_b2"))
	}
	if _, err := os.Stat(filepath.Join(p.root, "src", "battle", "menu_b.c")); !os.IsNotExist(err) {
		t.Fatal("unit file should be removed")
	}
	for _, name := range []string{"menu_b1", "menu_b2"} {
		if err := validateSourceDefinition(p.root, "src/battle/"+name+".c", name); err != nil {
			t.Fatal(err)
		}
	}
}

func TestSplitSymbolsCommandPreservesOwnedData(t *testing.T) {
	p := unitsTestProject(t)
	if err := p.unitSymbolsCommand([]string{"--name", "menu_b", "--source", "src/battle/menu_b.c", "battle:menu_b1", "battle:menu_b2"}); err != nil {
		t.Fatal(err)
	}
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	battle := config.byID["battle"]
	battle.unit("menu_b").Data = []rodataSpec{{Addr: 0x80067204, Size: 4, Hash: shortHash([]byte{7, 0, 0, 0})}}
	for path, data := range renderConfigFiles(config) {
		writeTestFile(t, filepath.Join(p.root, path), data)
	}
	sourcePath := filepath.Join(p.root, "src/battle/menu_b.c")
	source, err := os.ReadFile(sourcePath)
	if err != nil {
		t.Fatal(err)
	}
	source = []byte(strings.Replace(string(source), "extern int g_b_scalar;", "int g_b_scalar = 7;", 1))
	writeTestFile(t, sourcePath, source)
	if _, err := p.loadCheckedConfig(); err != nil {
		t.Fatal(err)
	}
	targetPath := filepath.Join(p.root, "target/battle.yaml")
	target, err := os.ReadFile(targetPath)
	if err != nil {
		t.Fatal(err)
	}
	for _, args := range [][]string{{"--unit", "menu_b", "--dry-run"}, {"--unit", "menu_b"}} {
		if err := p.splitSymbolsCommand(args); err == nil || !strings.Contains(err.Error(), "owns initialized data") {
			t.Fatalf("split guard error = %v", err)
		}
	}
	for path, before := range map[string][]byte{sourcePath: source, targetPath: target} {
		after, err := os.ReadFile(path)
		if err != nil || string(after) != string(before) {
			t.Fatalf("refused split changed %s: %v", path, err)
		}
	}
	for _, name := range []string{"menu_b1", "menu_b2"} {
		if _, err := os.Stat(filepath.Join(p.root, "src/battle", name+".c")); !os.IsNotExist(err) {
			t.Fatalf("refused split created %s.c: %v", name, err)
		}
	}
}

func TestUnitSymbolsCommandAbsorbsUnitsAndWrapsModuleBodies(t *testing.T) {
	p := unitsTestProject(t)
	// A shared two-function unit in both modules first.
	if err := p.unitSymbolsCommand([]string{"--name", "menu_a_head", "--source", "src/menu/menu_a_head.c", "battle:menu_a1", "battle:menu_a2", "world:menu_a1", "world:menu_a2"}); err != nil {
		t.Fatal(err)
	}
	// Listing one member of it without the other is refused.
	err := p.unitSymbolsCommand([]string{"--name", "menu_all", "--source", "src/menu/menu_all.c", "battle:menu_a1", "battle:menu_a3", "world:menu_a1", "world:menu_a3"})
	if err == nil || !strings.Contains(err.Error(), "list all of that unit's members or none") {
		t.Fatalf("partial absorb error = %v", err)
	}
	// Absorb it into a larger unit with a battle-only tail: menu_b1 follows
	// menu_a4 in battle and world has no such function. The fixture compiles
	// menu_b1 at -O1; give it the default profile so the unit has one.
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	config.byID["battle"].function("menu_b1").Profile = ""
	for path, rendered := range renderConfigFiles(config) {
		if err := os.WriteFile(filepath.Join(p.root, filepath.FromSlash(path)), rendered, 0o644); err != nil {
			t.Fatal(err)
		}
	}
	args := []string{"--name", "menu_all", "--source", "src/menu/menu_all.c"}
	for _, name := range []string{"menu_a1", "menu_a2", "menu_a3", "menu_a4", "menu_b1"} {
		args = append(args, "battle:"+name)
	}
	for _, name := range []string{"menu_a1", "menu_a2", "menu_a3", "menu_a4"} {
		args = append(args, "world:"+name)
	}
	if err := p.unitSymbolsCommand(args); err != nil {
		t.Fatal(err)
	}
	data, err := os.ReadFile(filepath.Join(p.root, "src", "menu", "menu_all.c"))
	if err != nil {
		t.Fatal(err)
	}
	text := string(data)
	if !strings.Contains(text, "#ifdef FFT_BATTLE\nextern int g_b_scalar;\nint menu_b1(void) { return g_b_scalar / 3; }\n#endif") {
		t.Fatalf("battle-only member not wrapped:\n%s", text)
	}
	if strings.Count(text, "#if") != 1 || strings.Index(text, "menu_a4(void)") > strings.Index(text, "#ifdef FFT_BATTLE") {
		t.Fatalf("unexpected conditionals or order:\n%s", text)
	}
	if _, err := os.Stat(filepath.Join(p.root, "src", "menu", "menu_a_head.c")); !os.IsNotExist(err) {
		t.Fatal("absorbed unit file should be removed")
	}
	config, err = p.loadCheckedConfig()
	if err != nil {
		t.Fatalf("checked config: %v", err)
	}
	battle, world := config.byID["battle"], config.byID["world"]
	if battle.unit("menu_a_head") != nil || world.unit("menu_a_head") != nil || battle.unit("menu_all") == nil || world.unit("menu_all") == nil {
		t.Fatalf("units = %+v / %+v", battle.Units, world.Units)
	}
	if battle.function("menu_b1").Unit != "menu_all" || world.function("menu_a4").Unit != "menu_all" {
		t.Fatalf("members not rebound: %+v / %+v", *battle.function("menu_b1"), *world.function("menu_a4"))
	}
	if err := validateUnitSourceFor(p.root, world, "src/menu/menu_all.c", []string{"menu_a1", "menu_a2", "menu_a3", "menu_a4"}); err != nil {
		t.Fatal(err)
	}
	if err := validateUnitSourceFor(p.root, battle, "src/menu/menu_all.c", []string{"menu_a1", "menu_a2", "menu_a3", "menu_a4"}); err == nil {
		t.Fatal("battle sees menu_b1 too; the member list without it must fail")
	}
	// Per-module bodies of one name become one block per body.
	if err := os.WriteFile(filepath.Join(p.root, "src", "text", "text_c1_world.c"), []byte("extern int g_c_table[];\nint text_c1(void) { return g_c_table[1]; }\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	world.function("text_c1").Source = "src/text/text_c1_world.c"
	for path, rendered := range renderConfigFiles(config) {
		if err := os.WriteFile(filepath.Join(p.root, filepath.FromSlash(path)), rendered, 0o644); err != nil {
			t.Fatal(err)
		}
	}
	if err := p.unitSymbolsCommand([]string{"--name", "text_c", "--source", "src/text/text_c.c", "battle:text_c1", "battle:text_c2", "world:text_c1", "world:text_c2"}); err != nil {
		t.Fatal(err)
	}
	data, err = os.ReadFile(filepath.Join(p.root, "src", "text", "text_c.c"))
	if err != nil {
		t.Fatal(err)
	}
	text = string(data)
	if !strings.HasPrefix(text, "#ifdef FFT_BATTLE\nextern int g_c_table[];\nint text_c1(void) { return g_c_table[0]; }\n#endif\n\n#ifdef FFT_WORLD\nextern int g_c_table[];\nint text_c1(void) { return g_c_table[1]; }\n#endif\n\nint text_c2(void)") {
		t.Fatalf("per-module bodies not wrapped in module order:\n%s", text)
	}
	// A unit with conditionals cannot be split or absorbed mechanically.
	if err := p.splitSymbolsCommand([]string{"--unit", "text_c"}); err == nil || !strings.Contains(err.Error(), "program conditionals") {
		t.Fatalf("split error = %v", err)
	}
	if err := p.unitSymbolsCommand([]string{"--name", "text_more", "--source", "src/text/text_more.c", "battle:text_c1", "battle:text_c2", "battle:text_c3", "world:text_c1", "world:text_c2", "world:text_c3"}); err == nil || !strings.Contains(err.Error(), "program conditionals") {
		t.Fatalf("absorb error = %v", err)
	}
	if order, err := mergeMemberOrders([][]string{{"a", "b", "c"}, {"a", "c", "d"}, {"x", "b"}}); err != nil || strings.Join(order, " ") != "a x b c d" {
		t.Fatalf("merged order = %v, %v", order, err)
	}
	if _, err := mergeMemberOrders([][]string{{"a", "b"}, {"b", "a"}}); err == nil || !strings.Contains(err.Error(), "order the members differently") {
		t.Fatalf("inconsistent order error = %v", err)
	}
}

func TestRenameUnitCommandMovesTheFileAndRebindsMembers(t *testing.T) {
	p := unitsTestProject(t)
	if err := p.unitSymbolsCommand([]string{"--name", "menu_a_head", "--source", "src/menu/menu_a_head.c", "battle:menu_a1", "battle:menu_a2", "world:menu_a1", "world:menu_a2"}); err != nil {
		t.Fatal(err)
	}
	if err := p.renameUnitCommand([]string{"--unit", "menu_a_head", "--name", "menu_openers"}); err != nil {
		t.Fatal(err)
	}
	config, err := p.loadCheckedConfig()
	if err != nil {
		t.Fatalf("checked config: %v", err)
	}
	for _, id := range []string{"battle", "world"} {
		m := config.byID[id]
		if m.unit("menu_a_head") != nil || m.unit("menu_openers") == nil || m.unit("menu_openers").Source != "src/menu/menu_openers.c" || m.function("menu_a1").Unit != "menu_openers" {
			t.Fatalf("%s: %+v %+v", id, m.Units, *m.function("menu_a1"))
		}
	}
	if _, err := os.Stat(filepath.Join(p.root, "src", "menu", "menu_a_head.c")); !os.IsNotExist(err) {
		t.Fatal("old file should be gone")
	}
	if err := p.renameUnitCommand([]string{"--unit", "nowhere", "--name", "x"}); err == nil || !strings.Contains(err.Error(), "no module has a unit") {
		t.Fatalf("missing unit error = %v", err)
	}
}

func TestUnitSymbolsCommandMaterializesAnAliasWhoseTargetStaysOutside(t *testing.T) {
	const yaml = `module: battle
file: BATTLE.BIN
lba: 300
size: 4096
sha256: 33333333333333333333333333333333333333333333333333333333333333dd
load: 0x80067000
source_dir: src/battle
profile: gcc-2.6.3_O2_aspsx-2.34

functions:
  - {addr: 0x80067000, size: 32, name: input_a, hash: 0000000000000a01}
  - {addr: 0x80067020, size: 32, name: input_b, hash: 0000000000000b01}
  - {addr: 0x80067040, size: 32, name: card_input_a, hash: 0000000000000a02, source: src/battle/input_a.c, source_symbol: input_a}
  - {addr: 0x80067060, size: 32, name: card_c, hash: 0000000000000c01}

overrides:
  - {function: card_input_a, name: input_b, addr: 0x80067060}
`
	p := testProject(t, map[string]string{
		"target/main.yaml":       testMainYAML,
		"target/battle.yaml":     yaml,
		"src/main/main_first.c":  "void main_first(void) {}\n",
		"src/main/main_second.c": "void main_second(void) {}\n",
		"src/battle/input_a.c":   "int input_b(void);\nint input_a(void) { return input_b(); }\n",
		"src/battle/input_b.c":   "int input_b(void) { return 2; }\n",
		"src/battle/card_c.c":    "int card_c(void) { return 3; }\n",
	})
	// The primary cannot join a unit while an alias entry still compiles its file.
	if err := p.unitSymbolsCommand([]string{"--name", "inputs", "--source", "src/battle/inputs.c", "battle:input_a", "battle:input_b"}); err == nil || !strings.Contains(err.Error(), "also compiles") {
		t.Fatalf("primary merge error = %v", err)
	}
	// The alias joins a unit as a materialized duplicate; the primary keeps its file.
	if err := p.unitSymbolsCommand([]string{"--name", "card", "--source", "src/battle/card.c", "battle:card_input_a", "battle:card_c"}); err != nil {
		t.Fatal(err)
	}
	config, err := p.loadCheckedConfig()
	if err != nil {
		t.Fatal(err)
	}
	battle := config.byID["battle"]
	if alias := battle.function("card_input_a"); alias.Unit != "card" || alias.Source != "" || alias.SourceSymbol != "" {
		t.Fatalf("alias = %+v", *alias)
	}
	if primary := battle.function("input_a"); primary.Unit != "" || battle.sourceOf(primary) != "src/battle/input_a.c" {
		t.Fatalf("primary = %+v", *primary)
	}
	if _, err := os.Stat(filepath.Join(p.root, "src", "battle", "input_a.c")); err != nil {
		t.Fatalf("the primary's file should remain: %v", err)
	}
	data, err := os.ReadFile(filepath.Join(p.root, "src", "battle", "card.c"))
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(string(data), "a second time as card_input_a") || !strings.Contains(string(data), "int card_input_a(void) { return card_c(); }") {
		t.Fatalf("duplicate not materialized with its override applied:\n%s", data)
	}
	if len(battle.Overrides) != 0 {
		t.Fatalf("alias override not spent: %+v", battle.Overrides)
	}
	// With the alias materialized, the primary is free to join a unit.
	if err := p.unitSymbolsCommand([]string{"--name", "inputs", "--source", "src/battle/inputs.c", "battle:input_a", "battle:input_b"}); err != nil {
		t.Fatal(err)
	}
	if _, err := p.loadCheckedConfig(); err != nil {
		t.Fatal(err)
	}
}
