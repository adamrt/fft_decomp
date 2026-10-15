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
	if !strings.Contains(string(rendered), "units:\n  - {name: menu_b, source: src/battle/menu_b.c}") || !strings.Contains(string(rendered), "unit: menu_b") {
		t.Fatalf("rendered config lacks the unit:\n%s", rendered)
	}
	if _, err := p.loadCheckedConfig(); err != nil {
		t.Fatalf("checked config: %v", err)
	}
	if err := p.unitSymbolsCommand([]string{"--name", "gap", "--source", "src/battle/gap.c", "battle:menu_b3", "battle:map_d1"}); err == nil || !strings.Contains(err.Error(), "does not follow") {
		t.Fatalf("gap error = %v", err)
	}
	if err := p.moveSymbolsCommand([]string{"--dir", "src/shared/menu", "battle:menu_b1"}); err == nil || !strings.Contains(err.Error(), "within unit") {
		t.Fatalf("move guard error = %v", err)
	}
}
