package main

import (
	"bytes"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
)

func unitContractTestProject(t *testing.T) (project, *projectConfig) {
	t.Helper()
	p, config := unitCheckTestProject(t)
	m := config.byID["battle"]
	m.Functions = m.Functions[:2]
	m.Overrides = nil
	m.Units = []unitDef{
		{Name: "original_a", Source: "src/battle/unit_a.c", Profile: "gcc-2.6.3_O1_aspsx-2.34",
			Data:   []rodataSpec{{Addr: 0x80067600, Size: 8, Hash: shortHash([]byte{3, 0, 0, 0, 7, 0, 0, 0})}},
			Rodata: []rodataSpec{{Addr: 0x80067500, Size: 8, Hash: shortHash([]byte{5, 0, 0, 0, 9, 0, 0, 0})}}},
		{Name: "original_b", Source: "src/battle/unit_b.c", Profile: "gcc-2.6.3_O1_aspsx-2.34",
			Data:   []rodataSpec{{Addr: 0x80067608, Size: 8, Hash: shortHash([]byte{11, 0, 0, 0, 13, 0, 0, 0})}},
			Rodata: []rodataSpec{{Addr: 0x80067508, Size: 8, Hash: shortHash([]byte{17, 0, 0, 0, 19, 0, 0, 0})}}},
	}
	m.function("unit_a").Unit, m.function("unit_b").Unit = "original_a", "original_b"
	m.Data = []symbolSpec{
		{Addr: 0x80067500, Name: "g_lookup_a"}, {Addr: 0x80067508, Name: "g_lookup_b"},
		{Addr: 0x80067600, Name: "g_values_a"}, {Addr: 0x80067608, Name: "g_values_b"},
	}
	for path, source := range map[string]string{
		"src/battle/unit_a.c": "int g_values_a[2] = { 3, 7 };\nconst int g_lookup_a[2] = { 5, 9 };\nint unit_a(int n) { return g_values_a[n & 1] + g_lookup_a[n & 1]; }\n",
		"src/battle/unit_b.c": "int g_values_b[2] = { 11, 13 };\nconst int g_lookup_b[2] = { 17, 19 };\nint unit_b(int n) { return g_values_b[n & 1] + g_lookup_b[n & 1]; }\n",
	} {
		if err := os.WriteFile(filepath.Join(p.root, path), []byte(source), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	return p, config
}

func writeUnitContractConfig(t *testing.T, p project, config *projectConfig) {
	t.Helper()
	for path, data := range renderConfigFiles(config) {
		if err := os.WriteFile(filepath.Join(p.root, filepath.FromSlash(path)), data, 0o644); err != nil {
			t.Fatal(err)
		}
	}
}

func TestUnitSymbolsPreservesAbsorbedProfileAndSections(t *testing.T) {
	p, config := unitContractTestProject(t)
	writeUnitContractConfig(t, p, config)
	before := config.byID["battle"]
	wantData := append(append([]rodataSpec(nil), before.Units[0].Data...), before.Units[1].Data...)
	wantRodata := append(append([]rodataSpec(nil), before.Units[0].Rodata...), before.Units[1].Rodata...)
	if err := p.unitSymbolsCommand([]string{"--name", "merged", "--source", "src/battle/merged.c", "battle:unit_b", "battle:unit_a"}); err != nil {
		t.Fatal(err)
	}
	after, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	m := after.byID["battle"]
	unit := m.unit("merged")
	if unit == nil || unit.Profile != "gcc-2.6.3_O1_aspsx-2.34" || !reflect.DeepEqual(unit.Data, wantData) || !reflect.DeepEqual(unit.Rodata, wantRodata) {
		t.Fatalf("merged contracts = %+v", unit)
	}
	for _, f := range m.Functions {
		original := before.function(f.Name)
		if m.profileOf(&f) != before.profileOf(original) || f.Addr != original.Addr || f.Size != original.Size || f.Hash != original.Hash {
			t.Fatalf("changed original member contract: %+v", f)
		}
	}
	source, err := os.ReadFile(filepath.Join(p.root, unit.Source))
	if err != nil {
		t.Fatal(err)
	}
	previous := -1
	for _, spelling := range []string{"g_values_a[2]", "g_lookup_a[2]", "int unit_a(", "g_values_b[2]", "g_lookup_b[2]", "int unit_b("} {
		at := strings.Index(string(source), spelling)
		if at <= previous {
			t.Fatalf("declaration order lost at %s:\n%s", spelling, source)
		}
		previous = at
	}
}

func TestUnitSymbolsRejectsIncompatibleAbsorbedContracts(t *testing.T) {
	for name, tc := range map[string]struct {
		change func(*moduleSpec)
		want   string
	}{
		"optimization": {func(m *moduleSpec) { m.Units[1].Profile = m.Profile }, "profile disagreement"},
		"division checks": {func(m *moduleSpec) {
			m.Units[0].Profile = "gcc-2.6.3_O2_aspsx-2.34"
			m.Units[1].Profile = "gcc-2.6.3_O2_aspsx-2.34_divcheck"
		}, "profile disagreement"},
		"member override": {func(m *moduleSpec) { m.function("unit_b").Profile = m.Profile }, "profile disagreement"},
		"data gap":        {func(m *moduleSpec) { m.Units[1].Data[0].Addr += 8 }, "data ranges are not contiguous"},
		"data overlap":    {func(m *moduleSpec) { m.Units[1].Data[0].Addr -= 4 }, "data ranges are not contiguous"},
		"data order":      {func(m *moduleSpec) { m.Units[1].Data[0].Addr -= 16 }, "data ranges are not contiguous"},
		"rodata gap":      {func(m *moduleSpec) { m.Units[1].Rodata[0].Addr += 8 }, "rodata ranges are not contiguous"},
		"rodata overlap":  {func(m *moduleSpec) { m.Units[1].Rodata[0].Addr -= 4 }, "rodata ranges are not contiguous"},
	} {
		t.Run(name, func(t *testing.T) {
			p, config := unitContractTestProject(t)
			tc.change(config.byID["battle"])
			writeUnitContractConfig(t, p, config)
			before, err := os.ReadFile(filepath.Join(p.root, "target/battle.yaml"))
			if err != nil {
				t.Fatal(err)
			}
			err = p.unitSymbolsCommand([]string{"--name", "merged", "--source", "src/battle/merged.c", "battle:unit_a", "battle:unit_b"})
			if err == nil || !strings.Contains(err.Error(), tc.want) {
				t.Fatalf("merge error = %v; want %q", err, tc.want)
			}
			after, err := os.ReadFile(filepath.Join(p.root, "target/battle.yaml"))
			if err != nil || !bytes.Equal(before, after) {
				t.Fatalf("rejected merge changed target: %v", err)
			}
			if _, err := os.Stat(filepath.Join(p.root, "src/battle/merged.c")); !os.IsNotExist(err) {
				t.Fatalf("rejected merge created its source: %v", err)
			}
		})
	}
}

func TestOwnedRodataSkipsInheritedUnitTables(t *testing.T) {
	p, config := unitContractTestProject(t)
	_ = p
	m := config.byID["battle"]
	a, b := m.function("unit_a"), m.function("unit_b")
	a.Rodata = []rodataSpec{{Addr: 0x80067500, Size: 4, Hash: "0000000000000001"}}
	b.Rodata = []rodataSpec{{Addr: 0x80067510, Size: 4, Hash: "0000000000000002"}}
	inherited := []rodataSpec{{Addr: 0x80067508, Size: 8, Hash: "0000000000000003"}}
	m.Data = []symbolSpec{{Addr: 0x80067504, Name: "g_new_table"}, {Addr: 0x80067508, Name: "g_existing_table"}}
	owned, fillers, err := ownedRodataTables(make([]byte, m.Size), m, []*functionSpec{a, b}, inherited)
	if err != nil {
		t.Fatal(err)
	}
	if len(owned) != 1 || owned[0].Addr != 0x80067504 || owned[0].Size != 4 ||
		!strings.Contains(fillers[a.Name], "g_new_table") || strings.Contains(fillers[a.Name], "g_existing_table") {
		t.Fatalf("inherited table emitted again: %+v, %+v", owned, fillers)
	}
}

// Compile each original object separately, retain its code and storage hashes,
// then check the merged object against those unchanged contracts.
func TestUnitSymbolsMergedInitializersStillMatchOriginalBytes(t *testing.T) {
	requirePinnedToolchain(t)
	t.Setenv("TOOLS_CACHE", "0")
	p, config := unitContractTestProject(t)
	m := config.byID["battle"]
	for i, name := range []string{"original_a", "original_b"} {
		f := &m.Functions[i]
		if i > 0 {
			previous := m.Functions[i-1]
			f.Addr = previous.Addr + uint32(previous.Size)
		}
		unit, err := m.unitInstance(config, name)
		if err != nil {
			t.Fatal(err)
		}
		sections, err := p.compileUnitInstance(newSymbolResolver(config), unit, filepath.Join(p.root, unit.spec.source), filepath.Join(p.root, "build", name))
		if err != nil {
			t.Fatal(err)
		}
		f.Size, f.Hash = len(sections[".text"]), shortHash(sections[".text"])
		if problems := compareUnitRanges(".data", unit.data, sections[".data"]); len(problems) != 0 {
			t.Fatalf("original data: %v", problems)
		}
		if problems := compareUnitRanges(".rodata", unit.rodata, sections[".rodata"]); len(problems) != 0 {
			t.Fatalf("original rodata: %v", problems)
		}
	}
	writeUnitContractConfig(t, p, config)
	if err := p.unitSymbolsCommand([]string{"--name", "merged", "--source", "src/battle/merged.c", "battle:unit_a", "battle:unit_b"}); err != nil {
		t.Fatal(err)
	}
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	unit, err := config.byID["battle"].unitInstance(config, "merged")
	if err != nil {
		t.Fatal(err)
	}
	sourcePath := filepath.Join(p.root, unit.spec.source)
	sections, err := p.compileUnitInstance(newSymbolResolver(config), unit, sourcePath, filepath.Join(p.root, "build", "merged"))
	if err != nil {
		t.Fatal(err)
	}
	results, problems := compareUnitSections(unit, sections)
	if len(results) != 2 || !results[0].ok || !results[1].ok || len(problems) != 0 {
		t.Fatalf("merged bytes changed: %+v, %v", results, problems)
	}
	original, err := os.ReadFile(sourcePath)
	if err != nil {
		t.Fatal(err)
	}
	for name, replacement := range map[string][2]string{
		"data":   {"{ 3, 7 }", "{ 3, 8 }"},
		"rodata": {"{ 5, 9 }", "{ 5, 10 }"},
	} {
		t.Run(name, func(t *testing.T) {
			changed := strings.Replace(string(original), replacement[0], replacement[1], 1)
			if changed == string(original) {
				t.Fatal("initializer mutation did not apply")
			}
			if err := os.WriteFile(sourcePath, []byte(changed), 0o644); err != nil {
				t.Fatal(err)
			}
			sections, err := p.compileUnitInstance(newSymbolResolver(config), unit, sourcePath, filepath.Join(p.root, "build", "changed_"+name))
			if err != nil {
				t.Fatal(err)
			}
			if _, problems := compareUnitSections(unit, sections); len(problems) == 0 {
				t.Fatal("changed initializer accepted")
			}
		})
	}
}
