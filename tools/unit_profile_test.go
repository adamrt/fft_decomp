package main

import (
	"strings"
	"testing"
)

func TestUnitProfileHoistsASharedMemberProfile(t *testing.T) {
	const o1 = "gcc-2.6.3_O1_aspsx-2.34"
	const divcheck = "gcc-2.6.3_O2_aspsx-2.34_divcheck"
	m := &moduleSpec{
		ID: "world", Profile: "gcc-2.6.3_O2_aspsx-2.34", SourceDir: "src/world",
		Units: []unitDef{{Name: "grid", Source: "src/world/grid.c"}, {Name: "mixed", Source: "src/world/mixed.c"}},
		Functions: []functionSpec{
			{Addr: 0x80100000, Size: 8, Name: "a", Unit: "grid", Profile: o1},
			{Addr: 0x80100008, Size: 8, Name: "b", Unit: "grid", Profile: o1},
			{Addr: 0x80100010, Size: 8, Name: "c", Unit: "mixed", Profile: divcheck},
			{Addr: 0x80100018, Size: 8, Name: "d", Unit: "mixed"},
		},
	}
	m.normalize()
	if m.unit("grid").Profile != o1 {
		t.Fatalf("grid profile = %q", m.unit("grid").Profile)
	}
	if m.Functions[0].Profile != "" || m.Functions[1].Profile != "" {
		t.Fatalf("members still repeat the unit profile: %+v", m.Functions[:2])
	}
	if got := m.profileOf(&m.Functions[0]); got != o1 {
		t.Fatalf("member profile = %q", got)
	}
	if m.unit("mixed").Profile != "" || m.Functions[2].Profile != divcheck {
		t.Fatalf("mixed unit changed: unit %q member %q", m.unit("mixed").Profile, m.Functions[2].Profile)
	}
	rendered := string(renderModuleConfig(m))
	if !strings.Contains(rendered, "{name: grid, source: src/world/grid.c, profile: "+o1+"}") {
		t.Fatalf("rendered units:\n%s", rendered)
	}
	if strings.Contains(rendered, "name: a, profile:") {
		t.Fatalf("member row repeats its unit profile:\n%s", rendered)
	}
}
