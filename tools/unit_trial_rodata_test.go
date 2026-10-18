package main

import (
	"bytes"
	"crypto/sha256"
	"debug/elf"
	"fmt"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
)

func unitTrialRodataFixture() (*unitInstance, []byte) {
	unit, image := unitTrialDataFixture()
	unit.rodata = []rodataSpec{unit.module.Functions[0].Rodata[0], unit.module.Units[0].Rodata[0]}
	unit.members = []*functionSpec{&unit.module.Functions[0]}
	unit.addr, unit.size = unit.members[0].Addr, unit.members[0].Size
	return unit, image
}

func TestUnitTrialRodataRetainsAllOldSpansAndSectionChecks(t *testing.T) {
	unit, image := unitTrialRodataFixture()
	oldData := append([]rodataSpec(nil), unit.data...)
	oldRodata := append([]rodataSpec(nil), unit.rodata...)
	oldFunctions := append([]functionSpec(nil), unit.module.Functions...)
	oldUnits := append([]unitDef(nil), unit.module.Units...)
	base := unit.module.Load
	// Add before, between and after the old function and unit spans, out of order.
	additions := []unitTrialRangeYAML{{Addr: base + 28, Size: 4}, {Addr: base + 12, Size: 4}, {Addr: base + 20, Size: 4}}
	if err := extendUnitTrialRanges(unit, nil, additions, image); err != nil {
		t.Fatal(err)
	}
	if len(unit.rodata) != 5 || unit.rodata[1] != oldRodata[0] || unit.rodata[3] != oldRodata[1] ||
		!reflect.DeepEqual(unit.data, oldData) || !reflect.DeepEqual(unit.module.Functions, oldFunctions) || !reflect.DeepEqual(unit.module.Units, oldUnits) {
		t.Fatalf("lost an inherited contract: %+v", unit)
	}
	for _, index := range []int{0, 2, 4} {
		span := unit.rodata[index]
		start := unit.module.offset(span.Addr)
		if span.Hash != shortHash(image[start:start+span.Size]) {
			t.Fatalf("range does not hash original bytes: %+v", span)
		}
	}
	sections := map[string][]byte{".text": image[64:96], ".rodata": image[12:32], ".data": image[128:136]}
	results, problems := compareUnitSections(unit, sections)
	if len(results) != 1 || !results[0].ok || len(problems) != 0 {
		t.Fatalf("original sections rejected: %+v %v", results, problems)
	}
	for name, changed := range map[string]struct {
		section string
		data    []byte
	}{
		"missing rodata": {".rodata", nil},
		"short rodata":   {".rodata", image[12:31]},
		"extra rodata":   {".rodata", append(append([]byte(nil), image[12:32]...), 0)},
		"missing data":   {".data", nil},
		"stray bss":      {".bss", make([]byte, 4)},
		"stray sdata":    {".sdata", make([]byte, 4)},
	} {
		t.Run(name, func(t *testing.T) {
			trial := make(map[string][]byte)
			for section, data := range sections {
				trial[section] = data
			}
			trial[changed.section] = changed.data
			if _, problems := compareUnitSections(unit, trial); len(problems) == 0 {
				t.Fatal("accepted changed section contract")
			}
		})
	}
	// Every added span and both inherited read-only spans remain independently checked.
	for _, offset := range []int{0, 4, 8, 12, 16} {
		changed := append([]byte(nil), sections[".rodata"]...)
		changed[offset] ^= 1
		if problems := compareUnitRanges(".rodata", unit.rodata, changed); len(problems) != 1 {
			t.Fatalf("changed range at%d: %v", offset, problems)
		}
	}
}

func TestUnitTrialRodataRejectsInvalidOverlappingOrRemoteRanges(t *testing.T) {
	const base = uint32(0x80000000)
	for name, tc := range map[string]struct {
		rodata []unitTrialRangeYAML
		want   string
	}{
		"zero size":       {[]unitTrialRangeYAML{{Addr: base + 28}}, "invalid"},
		"negative size":   {[]unitTrialRangeYAML{{Addr: base + 28, Size: -1}}, "invalid"},
		"large size":      {[]unitTrialRangeYAML{{Addr: base + 28, Size: int(^uint(0) >> 1)}}, "invalid"},
		"below module":    {[]unitTrialRangeYAML{{Addr: base - 1, Size: 1}}, "outside"},
		"past module":     {[]unitTrialRangeYAML{{Addr: base + 255, Size: 2}}, "outside"},
		"text":            {[]unitTrialRangeYAML{{Addr: base + 64, Size: 4}}, "function"},
		"function rodata": {[]unitTrialRangeYAML{{Addr: base + 16, Size: 4}}, "rodata"},
		"unit rodata":     {[]unitTrialRangeYAML{{Addr: base + 24, Size: 4}}, "unit owner"},
		"unit data":       {[]unitTrialRangeYAML{{Addr: base + 128, Size: 4}}, "unit owner"},
		"duplicate":       {[]unitTrialRangeYAML{{Addr: base + 28, Size: 4}, {Addr: base + 28, Size: 4}}, "overlap"},
		"partial overlap": {[]unitTrialRangeYAML{{Addr: base + 28, Size: 8}, {Addr: base + 32, Size: 4}}, "overlap"},
		"eight byte gap":  {[]unitTrialRangeYAML{{Addr: base + 36, Size: 4}}, "not contiguous"},
	} {
		t.Run(name, func(t *testing.T) {
			unit, image := unitTrialRodataFixture()
			before := append([]rodataSpec(nil), unit.rodata...)
			if err := extendUnitTrialRanges(unit, nil, tc.rodata, image); err == nil || !strings.Contains(err.Error(), tc.want) {
				t.Fatalf("error=%v, want%q", err, tc.want)
			}
			if !reflect.DeepEqual(unit.rodata, before) {
				t.Fatal("failed trial mutated inherited rodata")
			}
		})
	}
	unit, image := unitTrialRodataFixture()
	if err := extendUnitTrialRanges(unit, nil, []unitTrialRangeYAML{{Addr: base + 35, Size: 4}}, image); err != nil {
		t.Fatalf("existing seven-byte alignment-gap allowance changed: %v", err)
	}
	for name, setup := range map[string]func(*unitInstance){
		"same added range in both sections": func(unit *unitInstance) {},
		"existing scratch rodata": func(unit *unitInstance) {
			unit.rodata = append(unit.rodata, rodataSpec{Addr: base + 124, Size: 4, Hash: "0000000000000001"})
		},
	} {
		t.Run(name, func(t *testing.T) {
			unit, image := unitTrialRodataFixture()
			setup(unit)
			beforeData := append([]rodataSpec(nil), unit.data...)
			beforeRodata := append([]rodataSpec(nil), unit.rodata...)
			addition := []unitTrialRangeYAML{{Addr: base + 124, Size: 4}}
			if err := extendUnitTrialRanges(unit, addition, addition, image); err == nil || !strings.Contains(err.Error(), "overlap") {
				t.Fatalf("accepted cross-section overlap: %v", err)
			}
			if !reflect.DeepEqual(unit.data, beforeData) || !reflect.DeepEqual(unit.rodata, beforeRodata) {
				t.Fatal("failed combined addition committed a partial section")
			}
		})
	}
}

func TestUnitTrialRodataRejectsDistantFunctionTable(t *testing.T) {
	unit, image := unitTrialRodataFixture()
	// A unit with no unit-level rodata still inherits its member's jump table.
	// A distant constant pool cannot become a separate section through additions.
	old := rodataSpec{Addr: unit.module.Load + 16, Size: 20, Hash: shortHash(image[16:36])}
	unit.module.Functions[0].Rodata = []rodataSpec{old}
	unit.module.Units[0].Rodata = nil
	unit.rodata = []rodataSpec{old}
	oldData := append([]rodataSpec(nil), unit.data...)
	addition := []unitTrialRangeYAML{{Addr: unit.module.Load + 160, Size: 68}}
	if err := extendUnitTrialRanges(unit, nil, addition, image); err == nil || !strings.Contains(err.Error(), "rodata ranges are not contiguous") {
		t.Fatalf("accepted pool remote from inherited function rodata: %v", err)
	}
	if !reflect.DeepEqual(unit.rodata, []rodataSpec{old}) || !reflect.DeepEqual(unit.data, oldData) || unit.module.Functions[0].Rodata[0] != old {
		t.Fatal("rejected remote pool changed inherited contracts")
	}
}

func TestUnitTrialRodataSchemaAndPerModuleOriginals(t *testing.T) {
	spec, err := parseUnitSpec("build/trial.yaml", []byte(`functions: [entry]
instances:
  - {module: battle, original_unit: owner, rodata: [{addr: 0x8000001c, size: 4}]}
  - {module: world, original_unit: owner, rodata: [{addr: 0x80000020, size: 4}]}
`))
	if err != nil {
		t.Fatal(err)
	}
	config := &projectConfig{byID: make(map[string]*moduleSpec)}
	p := project{filesRoot: t.TempDir()}
	for _, instance := range spec.instances {
		fixture, image := unitTrialDataFixture()
		m := fixture.module
		m.ID, m.File, m.Profile = instance.Module, strings.ToUpper(instance.Module)+".BIN", defaultProfile
		m.Functions[0].Unit = "owner"
		m.Units[0].Source = "src/" + m.ID + "/owner.c"
		if m.ID == "world" {
			image[32] ^= 0xff
		}
		m.SHA256 = fmt.Sprintf("%x", sha256.Sum256(image))
		config.Modules = append(config.Modules, m)
		config.byID[m.ID] = m
		if err := os.WriteFile(filepath.Join(p.filesRoot, m.File), image, 0o644); err != nil {
			t.Fatal(err)
		}
		unit, err := resolveUnitInstance(config, spec, instance)
		if err != nil {
			t.Fatal(err)
		}
		oldData := append([]rodataSpec(nil), unit.data...)
		if err := p.addUnitTrialRanges(unit, instance.Data, instance.Rodata); err != nil {
			t.Fatal(err)
		}
		added := unit.rodata[len(unit.rodata)-1]
		start := m.offset(instance.Rodata[0].Addr)
		if added.Addr != instance.Rodata[0].Addr || added.Hash != shortHash(image[start:start+4]) ||
			unit.profile.name != defaultProfile || !reflect.DeepEqual(unit.data, oldData) || len(unit.members) != 1 || unit.members[0].Name != "entry" {
			t.Fatalf("lost module-specific contracts: %+v", unit)
		}
		image[start] ^= 1
		if err := os.WriteFile(filepath.Join(p.filesRoot, m.File), image, 0o644); err != nil {
			t.Fatal(err)
		}
		unit, err = resolveUnitInstance(config, spec, instance)
		if err != nil {
			t.Fatal(err)
		}
		if err := p.addUnitTrialRanges(unit, nil, instance.Rodata); err == nil || !strings.Contains(err.Error(), "sha256") {
			t.Fatalf("accepted corrupt %s original: %v", m.ID, err)
		}
	}
	for _, extra := range []string{", hash: 0000000000000000", ", unexpected: 1"} {
		text := "functions: [entry]\ninstances: [{module: battle, rodata: [{addr: 0x8000001c, size: 4" + extra + "}]}]\n"
		if _, err := parseUnitSpec("build/bad.yaml", []byte(text)); err == nil {
			t.Fatal("accepted caller-supplied rodata hash or unknown field")
		}
	}
}

// This original is a synthetic fixture, never candidate-derived game metadata.
// Compile its two constant pools and mutable data once, retain only the first
// pool in the target, then add the second through hash-free instance rodata.
func TestUnitTrialRodataScratchCompileRejectsNewAndInheritedCorruption(t *testing.T) {
	requirePinnedToolchain(t)
	t.Setenv("TOOLS_CACHE", "0")
	p, config := unitCheckTestProject(t)
	source := "int g_values[2] = { 3, 7 };\n" +
		"const int g_old[2] = { 5, 9 };\nconst int g_new[2] = { 11, 13 };\n" +
		"int unit_a(int n) { return g_values[n & 1] + g_old[n & 1]; }\n" +
		"int unit_b(int n) { return g_values[n & 1] + g_new[n & 1]; }\n"
	originalSource := "src/battle/original_ab.c"
	writeTestFile(t, filepath.Join(p.root, originalSource), []byte(source))
	m := config.byID["battle"]
	m.Units = []unitDef{{Name: "original_ab", Source: originalSource, Profile: "gcc-2.6.3_O1_aspsx-2.34",
		Data:   []rodataSpec{{Addr: 0x80067600, Size: 8, Hash: shortHash([]byte{3, 0, 0, 0, 7, 0, 0, 0})}},
		Rodata: []rodataSpec{{Addr: 0x80067500, Size: 8, Hash: shortHash([]byte{5, 0, 0, 0, 9, 0, 0, 0})}},
	}}
	a, b := m.function("unit_a"), m.function("unit_b")
	a.Unit, b.Unit = "original_ab", "original_ab"
	m.Data = append(m.Data, symbolSpec{Addr: 0x80067600, Name: "g_values"}, symbolSpec{Addr: 0x80067500, Name: "g_old"}, symbolSpec{Addr: 0x80067508, Name: "g_new"})
	unit, err := m.unitInstance(config, "original_ab")
	if err != nil {
		t.Fatal(err)
	}
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
	if err != nil {
		linked.Close()
		t.Fatal(err)
	}
	found := false
	for _, symbol := range symbols {
		if symbol.Name == b.Name {
			b.Addr, found = uint32(symbol.Value), true
		}
	}
	if !found {
		linked.Close()
		t.Fatal("synthetic original omitted unit_b")
	}
	image := make([]byte, m.Size)
	for _, section := range linked.Sections {
		if section.Flags&elf.SHF_ALLOC == 0 || section.Size == 0 {
			continue
		}
		data, err := section.Data()
		if err != nil {
			linked.Close()
			t.Fatal(err)
		}
		copy(image[m.offset(uint32(section.Addr)):], data)
	}
	linked.Close()
	if !bytes.Equal(sections[".rodata"], []byte{5, 0, 0, 0, 9, 0, 0, 0, 11, 0, 0, 0, 13, 0, 0, 0}) {
		t.Fatalf("unexpected synthetic pool layout: %x", sections[".rodata"])
	}
	a.Size = int(b.Addr - a.Addr)
	b.Size = len(sections[".text"]) - a.Size
	a.Hash, b.Hash = shortHash(sections[".text"][:a.Size]), shortHash(sections[".text"][a.Size:])
	// The bootstrap fixture's unrelated entries must not impose their old
	// addresses on this independently sized synthetic original.
	m.Functions = []functionSpec{*a, *b}
	m.Overrides = nil
	m.SHA256 = fmt.Sprintf("%x", sha256.Sum256(image))
	writeTestFile(t, filepath.Join(p.filesRoot, m.File), image)
	for path, data := range renderConfigFiles(config) {
		writeTestFile(t, filepath.Join(p.root, filepath.FromSlash(path)), data)
	}
	spec := "build/extra_rodata.yaml"
	writeTestFile(t, filepath.Join(p.root, spec), []byte("name: extra_rodata\nsource: build/copy.c\nfunctions: [unit_a, unit_b]\ninstances:\n  - module: battle\n    original_unit: original_ab\n    rodata: [{addr: 0x80067508, size: 8}]\n"))
	writeTestFile(t, filepath.Join(p.root, "build/copy.c"), []byte(source))
	if err := p.unitCheckCommand([]string{"--spec", spec}); err != nil {
		t.Fatal(err)
	}
	for name, change := range map[string][2]string{
		"new rodata":       {"{ 11, 13 }", "{ 12, 13 }"},
		"inherited rodata": {"{ 5, 9 }", "{ 6, 9 }"},
		"inherited data":   {"{ 3, 7 }", "{ 3, 8 }"},
	} {
		t.Run(name, func(t *testing.T) {
			changed := strings.Replace(source, change[0], change[1], 1)
			if changed == source {
				t.Fatal("initializer mutation did not apply")
			}
			writeTestFile(t, filepath.Join(p.root, "build/copy.c"), []byte(changed))
			if err := p.unitCheckCommand([]string{"--spec", spec}); err == nil ||
				!strings.Contains(err.Error(), "unit-level problems") || !strings.Contains(err.Error(), "0 of 2 functions differ") {
				t.Fatalf("changed initializer accepted: %v", err)
			}
		})
	}
}
