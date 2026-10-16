package main

import (
	"strings"
	"testing"
)

func TestUnitOwnedDataRendersAndCompares(t *testing.T) {
	table := []byte{3, 21, 4, 20, 5, 21, 6, 22}
	m := &moduleSpec{
		ID: "opening", Profile: "gcc-2.6.3_O2_aspsx-2.34", SourceDir: "src/open",
		Units: []unitDef{{Name: "movies", Source: "src/open/movies.c",
			Data: []rodataSpec{{Addr: 0x80070000, Size: len(table), Hash: shortHash(table)}}}},
		Functions: []functionSpec{{Addr: 0x80068000, Size: 8, Name: "a", Unit: "movies"}},
	}
	rendered := string(renderModuleConfig(m))
	if !strings.Contains(rendered, "{name: movies, source: src/open/movies.c, data: [{addr: 0x80070000, size: 8, hash: "+yamlText(shortHash(table))+"}]}") {
		t.Fatalf("rendered units:\n%s", rendered)
	}
	if problems := compareUnitRanges(".data", m.Units[0].Data, table); len(problems) != 0 {
		t.Fatalf("matching data reported %v", problems)
	}
	changed := append([]byte(nil), table...)
	changed[1] = 22
	if problems := compareUnitRanges(".data", m.Units[0].Data, changed); len(problems) != 1 || !strings.Contains(problems[0], "differs") {
		t.Fatalf("changed data reported %v", problems)
	}
	if problems := compareUnitRanges(".data", m.Units[0].Data, table[:4]); len(problems) != 1 || !strings.Contains(problems[0], "cover 8") {
		t.Fatalf("short data reported %v", problems)
	}
	unit := &unitInstance{addr: 0x80068000, data: m.Units[0].Data}
	if script := unitLinkerScript(unit, map[string]uint32{}, false); !strings.Contains(script, ".data 0x80070000 : SUBALIGN(4) { *(.data) }") {
		t.Fatalf("script:\n%s", script)
	}
}
