package main

import (
	"bytes"
	"debug/elf"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
)

func originalUnitCheckTestConfig(t *testing.T) *projectConfig {
	t.Helper()
	_, config := unitCheckTestProject(t)
	m := config.byID["battle"]
	m.Units = []unitDef{{
		Name: "original_ab", Source: "src/battle/original_ab.c", Profile: "gcc-2.6.3_O1_aspsx-2.34",
		Rodata: []rodataSpec{{Addr: 0x80067208, Size: 8, Hash: shortHash([]byte("table123"))}},
		Data:   []rodataSpec{{Addr: 0x80067300, Size: 8, Hash: shortHash([]byte("data1234"))}},
	}, {Name: "other_unit", Source: "src/battle/unit_c.c"}, {Name: "empty_unit", Source: "src/battle/empty.c"}}
	m.function("unit_a").Unit, m.function("unit_b").Unit = "original_ab", "original_ab"
	m.function("unit_c").Unit = "other_unit"
	m.function("unit_a").Rodata = []rodataSpec{{Addr: 0x80067200, Size: 8, Hash: shortHash([]byte("literal1"))}}
	m.function("unit_b").Rodata = []rodataSpec{{Addr: 0x80067210, Size: 8, Hash: shortHash([]byte("literal2"))}}
	return config
}

func TestUnitSpecOriginalUnitInheritsTargetContracts(t *testing.T) {
	config := originalUnitCheckTestConfig(t)
	spec, err := parseUnitSpec("build/trial.yaml", []byte(`name: trial
source: build/trial.c
functions: [unit_a, unit_b]
instances:
  - {module: battle, original_unit: original_ab, profile: gcc-2.6.3_O1_aspsx-2.34}
`))
	if err != nil {
		t.Fatal(err)
	}
	got, err := resolveUnitInstance(config, spec, spec.instances[0])
	if err != nil {
		t.Fatal(err)
	}
	m := config.byID["battle"]
	want, err := m.unitInstance(config, "original_ab")
	if err != nil {
		t.Fatal(err)
	}
	if got.spec != spec || got.spec.source != "build/trial.c" || got.module != want.module ||
		!reflect.DeepEqual(got.members, want.members) || got.addr != want.addr || got.size != want.size ||
		got.profile != want.profile || !reflect.DeepEqual(got.rodata, want.rodata) || !reflect.DeepEqual(got.data, want.data) {
		t.Fatalf("scratch instance lost original contracts: got %+v, original %+v", got, want)
	}
	if got.profile.name != "gcc-2.6.3_O1_aspsx-2.34" || len(got.rodata) != 3 || len(got.data) != 1 {
		t.Fatalf("unit-level profile or tables not inherited: %+v", got)
	}
	got.data[0].Hash = "changed only in scratch"
	if m.unit("original_ab").Data[0].Hash != shortHash([]byte("data1234")) {
		t.Fatal("scratch data aliases target metadata")
	}
	if _, err := parseUnitSpec("build/bad.yaml", []byte("functions: [unit_a]\ninstances: [{module: battle, original_unit: ../bad}]\n")); err == nil {
		t.Fatal("accepted invalid original unit name")
	}
}

func TestUnitSpecOriginalUnitRejectsDifferentContracts(t *testing.T) {
	config := originalUnitCheckTestConfig(t)
	spec := &unitSpec{name: "trial", source: "build/trial.c", functions: []string{"unit_a", "unit_b"}}
	for name, tc := range map[string]struct {
		instance unitInstanceYAML
		want     string
	}{
		"missing":    {unitInstanceYAML{Module: "battle", OriginalUnit: "missing"}, "has no unit missing"},
		"wrong":      {unitInstanceYAML{Module: "battle", OriginalUnit: "other_unit"}, "must list all members"},
		"empty":      {unitInstanceYAML{Module: "battle", OriginalUnit: "empty_unit"}, "no functions listed"},
		"incomplete": {unitInstanceYAML{Module: "battle", OriginalUnit: "original_ab", Functions: []string{"unit_a"}}, "must list all members"},
		"reordered":  {unitInstanceYAML{Module: "battle", OriginalUnit: "original_ab", Functions: []string{"unit_b", "unit_a"}}, "must list all members"},
		"extra":      {unitInstanceYAML{Module: "battle", OriginalUnit: "original_ab", Functions: []string{"unit_a", "unit_b", "unit_c"}}, "must list all members"},
		"stale":      {unitInstanceYAML{Module: "battle", OriginalUnit: "original_ab", Functions: []string{"unit_a", "old_name"}}, "must list all members"},
		"profile":    {unitInstanceYAML{Module: "battle", OriginalUnit: "original_ab", Profile: "gcc-2.6.3_O2_aspsx-2.34"}, "asserts profile"},
	} {
		t.Run(name, func(t *testing.T) {
			if _, err := resolveUnitInstance(config, spec, tc.instance); err == nil || !strings.Contains(err.Error(), tc.want) {
				t.Fatalf("error = %v; want %q", err, tc.want)
			}
		})
	}
	withRodata := *spec
	withRodata.rodata = []rodataSpec{{Addr: 0x80067218, Size: 8, Hash: "0000000000000001"}}
	instance := unitInstanceYAML{Module: "battle", OriginalUnit: "original_ab"}
	if _, err := resolveUnitInstance(config, &withRodata, instance); err == nil || !strings.Contains(err.Error(), "cannot combine") {
		t.Fatalf("scratch table override: %v", err)
	}
	config.byID["battle"].unit("original_ab").Data = append(config.byID["battle"].unit("original_ab").Data,
		rodataSpec{Addr: 0x80067400, Size: 4, Hash: "0000000000000002"})
	if _, err := resolveUnitInstance(config, spec, instance); err == nil || !strings.Contains(err.Error(), "data ranges are not contiguous") {
		t.Fatalf("invalid original data layout: %v", err)
	}
}

func TestOriginalUnitChecksInheritedDataWithoutPermittingOtherSections(t *testing.T) {
	config := originalUnitCheckTestConfig(t)
	m := config.byID["battle"]
	m.function("unit_a").Hash = shortHash(bytes.Repeat([]byte{1}, 32))
	m.function("unit_b").Hash = shortHash(bytes.Repeat([]byte{2}, 32))
	spec := &unitSpec{name: "trial", functions: []string{"unit_a", "unit_b"}}
	unit, err := resolveUnitInstance(config, spec, unitInstanceYAML{Module: "battle", OriginalUnit: "original_ab"})
	if err != nil {
		t.Fatal(err)
	}
	sections := map[string][]byte{
		".text":   append(bytes.Repeat([]byte{1}, 32), bytes.Repeat([]byte{2}, 32)...),
		".rodata": []byte("literal1table123literal2"),
		".data":   []byte("data1234"),
	}
	results, problems := compareUnitSections(unit, sections)
	if len(results) != 2 || !results[0].ok || !results[1].ok || len(problems) != 0 {
		t.Fatalf("original tables rejected: %+v, %v", results, problems)
	}
	for name, changed := range map[string]struct {
		section string
		data    []byte
	}{
		"changed data":   {".data", []byte("DATA1234")},
		"missing data":   {".data", nil},
		"extra data":     {".data", []byte("data1234extra")},
		"changed rodata": {".rodata", []byte("literal1TABLE123literal2")},
		"stray bss":      {".bss", make([]byte, 4)},
	} {
		t.Run(name, func(t *testing.T) {
			trial := map[string][]byte{}
			for section, data := range sections {
				trial[section] = data
			}
			trial[changed.section] = changed.data
			if _, problems := compareUnitSections(unit, trial); len(problems) == 0 {
				t.Fatal("accepted changed section contract")
			}
		})
	}
}

// Compile an initialized original object, then check an unchanged scratch copy
// through the command. Mutating only its initializer must still fail the hash.
func TestOriginalUnitScratchCompileRetainsInitializedData(t *testing.T) {
	requirePinnedToolchain(t)
	t.Setenv("TOOLS_CACHE", "0")
	p, config := unitCheckTestProject(t)
	source := "int g_values[2] = { 3, 7 };\n" +
		"const int g_lookup[2] = { 5, 9 };\n" +
		"int unit_a(int n) { return g_values[n & 1]; }\n" +
		"int unit_b(int n) { return g_values[n & 1] + g_lookup[n & 1]; }\n"
	originalSource := "src/battle/original_ab.c"
	if err := os.WriteFile(filepath.Join(p.root, originalSource), []byte(source), 0o644); err != nil {
		t.Fatal(err)
	}
	m := config.byID["battle"]
	m.Units = []unitDef{{Name: "original_ab", Source: originalSource,
		Data:   []rodataSpec{{Addr: 0x80067600, Size: 8, Hash: shortHash([]byte{3, 0, 0, 0, 7, 0, 0, 0})}},
		Rodata: []rodataSpec{{Addr: 0x80067500, Size: 8, Hash: shortHash([]byte{5, 0, 0, 0, 9, 0, 0, 0})}},
	}}
	a, b := m.function("unit_a"), m.function("unit_b")
	a.Unit, b.Unit = "original_ab", "original_ab"
	m.Data = append(m.Data, symbolSpec{Addr: 0x80067600, Name: "g_values"}, symbolSpec{Addr: 0x80067500, Name: "g_lookup"})
	unit, err := m.unitInstance(config, "original_ab")
	if err != nil {
		t.Fatal(err)
	}
	// Learn this synthetic fixture's function boundaries without assuming the
	// compiler's instruction count. Production targets are never rewritten.
	bootstrap := *unit
	bootstrap.members = []*functionSpec{a}
	work := filepath.Join(p.root, "build", "original")
	sections, err := p.compileUnitInstance(newSymbolResolver(config), &bootstrap, filepath.Join(p.root, originalSource), work)
	if err != nil {
		t.Fatal(err)
	}
	linked, err := elf.Open(filepath.Join(work, m.ID, unit.profile.name+".elf"))
	if err != nil {
		t.Fatal(err)
	}
	symbols, err := linked.Symbols()
	linked.Close()
	if err != nil {
		t.Fatal(err)
	}
	found := false
	for _, symbol := range symbols {
		if symbol.Name == b.Name {
			b.Addr = uint32(symbol.Value)
			found = true
		}
	}
	if !found {
		t.Fatal("original object omitted unit_b")
	}
	a.Size = int(b.Addr - a.Addr)
	b.Size = len(sections[".text"]) - a.Size
	a.Hash, b.Hash = shortHash(sections[".text"][:a.Size]), shortHash(sections[".text"][a.Size:])
	for path, data := range renderConfigFiles(config) {
		if err := os.WriteFile(filepath.Join(p.root, filepath.FromSlash(path)), data, 0o644); err != nil {
			t.Fatal(err)
		}
	}
	spec := "build/original.yaml"
	if err := os.WriteFile(filepath.Join(p.root, spec), []byte("name: original_copy\nsource: build/copy.c\nfunctions: [unit_a, unit_b]\ninstances: [{module: battle, original_unit: original_ab}]\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := p.unitCheckCommand([]string{"--spec", spec, "--merge"}); err != nil {
		t.Fatal(err)
	}
	copyPath := filepath.Join(p.root, "build", "copy.c")
	copied, err := os.ReadFile(copyPath)
	if err != nil {
		t.Fatal(err)
	}
	if strings.Count(string(copied), "int unit_a(") != 1 || strings.Count(string(copied), "int unit_b(") != 1 {
		t.Fatalf("merge duplicated the original unit:\n%s", copied)
	}
	changed := strings.Replace(string(copied), "{ 3, 7 }", "{ 3, 8 }", 1)
	if changed == string(copied) {
		t.Fatal("initializer mutation did not apply")
	}
	if err := os.WriteFile(copyPath, []byte(changed), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := p.unitCheckCommand([]string{"--spec", spec}); err == nil || !strings.Contains(err.Error(), "unit-level problems") {
		t.Fatalf("changed initializer accepted: %v", err)
	}
}
