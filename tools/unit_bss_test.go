package main

import (
	"bytes"
	"crypto/sha256"
	"debug/elf"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
)

func bssEvidenceFixture() (*unitInstance, *historicalResult, []byte) {
	const base = uint32(0x80010000)
	m := &moduleSpec{ID: "fixture", Load: base, Size: 256, Data: []symbolSpec{{Name: "g_current", Addr: base + 0xc8}}}
	span := rodataSpec{Addr: base + 0xc8, Size: 4, Hash: shortHash(make([]byte, 4))}
	m.Units = []unitDef{{Name: "state", Bss: []rodataSpec{span}}}
	m.Functions = []functionSpec{{Name: "entry", Addr: base, Size: 4, Unit: "state"}}
	unit := &unitInstance{spec: &unitSpec{name: "state"}, module: m, bss: []rodataSpec{span}, addr: base, size: 4, members: []*functionSpec{&m.Functions[0]}}
	result := &historicalResult{Sections: map[string][]byte{".text": {0, 0, 0, 0}, ".bss": nil}, Definitions: []historicalDefinition{{Name: "entry", Address: uint64(base), Section: ".text"}, {Name: "g_current", Address: uint64(span.Addr), Section: ".bss"}}, NoCommon: true, Allocations: map[string]historicalAllocation{
		".text": {Address: uint64(base), Size: 4, Type: elf.SHT_PROGBITS, Flags: elf.SHF_ALLOC | elf.SHF_EXECINSTR},
		".bss":  {Address: uint64(span.Addr), Size: 4, Type: elf.SHT_NOBITS, Flags: elf.SHF_ALLOC | elf.SHF_WRITE}}}
	return unit, result, make([]byte, m.Size)
}

func TestBSSAllocationEvidenceAndCachedMetadata(t *testing.T) {
	for _, name := range []string{"valid", "address", "size", "type", "flags", "payload", "missing section", "missing definition", "ABS alias", "wrong membership", "swapped zero name", "unknown object", "COMMON proof", "old metadata", "extra metadata", "missing metadata", "unclaimed section", "unclaimed tail"} {
		t.Run(name, func(t *testing.T) {
			unit, result, _ := bssEvidenceFixture()
			allocation := result.Allocations[".bss"]
			switch name {
			case "address":
				allocation.Address += 4
			case "size":
				allocation.Size += 4
			case "type":
				allocation.Type = elf.SHT_PROGBITS
			case "flags":
				allocation.Flags |= elf.SHF_EXECINSTR
			case "payload":
				result.Sections[".bss"] = make([]byte, 4)
			case "missing section":
				delete(result.Sections, ".bss")
				delete(result.Allocations, ".bss")
			case "missing definition", "ABS alias":
				result.Definitions = result.Definitions[:1]
			case "wrong membership":
				result.Definitions[1].Section = ".text"
			case "swapped zero name":
				unit.module.Data = append(unit.module.Data, symbolSpec{Name: "g_other", Addr: unit.bss[0].Addr + 4})
				result.Definitions[1].Name = "g_other"
			case "unknown object":
				result.Definitions[1].Name = "g_unknown"
			case "COMMON proof":
				result.NoCommon = false
			case "old metadata":
				result.Allocations = nil
			case "extra metadata":
				result.Allocations[".extra"] = allocation
			case "missing metadata":
				delete(result.Allocations, ".text")
			case "unclaimed section":
				result.Sections[".sbss"] = nil
				result.Allocations[".sbss"] = allocation
			case "unclaimed tail":
				allocation.Size = 8
			}
			if name != "missing section" && name != "old metadata" {
				result.Allocations[".bss"] = allocation
			}
			// Exercise the serialized representation used by a hit, not just cold structs.
			data, err := json.Marshal(result)
			if err != nil {
				t.Fatal(err)
			}
			var hit historicalResult
			if err = json.Unmarshal(data, &hit); err != nil {
				t.Fatal(err)
			}
			sections, err := hit.checkedUnitSections(unit)
			if name == "valid" {
				if err != nil || !bytes.Equal(sections[".bss"], make([]byte, 4)) {
					t.Fatalf("valid BSS: %v", err)
				}
				if hit.Sections[".bss"] != nil {
					t.Fatal("cached payload mutated")
				}
				return
			}
			if name == "unclaimed section" && err == nil {
				_, problems := compareUnitSections(unit, sections)
				if len(problems) > 0 {
					return
				}
			}
			if err == nil {
				t.Fatal("invalid allocation evidence accepted")
			}
		})
	}
}

func TestBSSEnvelopeOriginalZeroAndGapRejection(t *testing.T) {
	unit, _, image := bssEvidenceFixture()
	if err := validateBSSOriginal(unit.module, unit.bss, image, true); err != nil {
		t.Fatal(err)
	}
	for _, offset := range []int{0, 2, 3} {
		image[0xc8+offset] = 1
		for _, hashes := range []bool{false, true} {
			if err := validateBSSOriginal(unit.module, unit.bss, image, hashes); err == nil {
				t.Fatalf("accepted nonzero original %d", offset)
			}
		}
		image[0xc8+offset] = 0
	}
	if p := compareUnitRanges(".bss", unit.bss, make([]byte, 5)); len(p) == 0 {
		t.Fatal("BSS inherited data tail leniency")
	}
	unit.bss[0].Hash = "wrong"
	if err := validateBSSOriginal(unit.module, unit.bss, image, true); err == nil {
		t.Fatal("bad hash")
	}
	if err := validateBSSOriginal(unit.module, unit.bss, image, false); err != nil {
		t.Fatal(err)
	}
	unit.bss[0].Size = 1
	unit.bss[0].Hash = shortHash([]byte{0})
	unit.bss = append(unit.bss, rodataSpec{Addr: unit.bss[0].Addr + 4, Size: 4, Hash: shortHash(make([]byte, 4))})
	if _, _, err := bssEnvelope(unit.module, unit.bss); err == nil {
		t.Fatal("unhashed BSS gap accepted")
	}
}

func TestBSSLayoutAndTrialEnvelopeGuards(t *testing.T) {
	for _, name := range []string{"data", "rodata", "text", "literal", "other BSS", "gap data", "gap other BSS", "remote", "duplicate", "overflow", "missing label"} {
		t.Run(name, func(t *testing.T) {
			unit, _, image := bssEvidenceFixture()
			m := unit.module
			gap := rodataSpec{Addr: unit.bss[0].Addr + 1, Size: 1}
			switch name {
			case "data":
				m.Units[0].Data = []rodataSpec{unit.bss[0]}
			case "rodata":
				m.Units[0].Rodata = []rodataSpec{unit.bss[0]}
			case "text":
				m.Functions[0].Addr = unit.bss[0].Addr
			case "literal":
				m.Functions[0].Rodata = []rodataSpec{unit.bss[0]}
			case "other BSS":
				m.Units = append(m.Units, unitDef{Name: "other", Bss: unit.bss})
			case "gap data", "gap other BSS":
				unit.bss[0].Size = 1
				unit.bss[0].Hash = shortHash([]byte{0})
				unit.bss = append(unit.bss, rodataSpec{Addr: unit.bss[0].Addr + 4, Size: 4, Hash: shortHash(make([]byte, 4))})
				m.Units[0].Bss = unit.bss
				if name == "gap data" {
					m.Units = append(m.Units, unitDef{Name: "other", Data: []rodataSpec{gap}})
				} else {
					m.Units = append(m.Units, unitDef{Name: "other", Bss: []rodataSpec{gap}})
				}
			case "remote":
				m.Units[0].Bss = append(m.Units[0].Bss, rodataSpec{Addr: m.Load + 0xe0, Size: 4})
			case "duplicate":
				m.Units[0].Bss = append(m.Units[0].Bss, unit.bss[0])
			case "overflow":
				m.Units[0].Bss[0].Size = int(^uint(0) >> 1)
			case "missing label":
				m.Data = nil
			}
			if name == "missing label" {
				if err := validateBSSLabels(m, m.Units[0].Bss); err == nil {
					t.Fatal("missing base label")
				}
				return
			}
			if err := validateUnitOwnedLayout(m); err == nil {
				t.Fatal("BSS layout conflict accepted")
			}
			_ = image
		})
	}
	// A scratch addition on either side may not cover a conflicting gap.
	unit, _, image := bssEvidenceFixture()
	unit.bss = nil
	unit.module.Units = nil
	addr := unit.module.Load + 0xc8
	unit.module.Units = []unitDef{{Name: "other", Data: []rodataSpec{{Addr: addr + 2, Size: 1}}}}
	before := append([]rodataSpec(nil), unit.bss...)
	err := extendUnitTrialRanges(unit, nil, nil, image, []unitTrialRangeYAML{{Addr: addr, Size: 1}, {Addr: addr + 4, Size: 4}})
	if err == nil || !reflect.DeepEqual(before, unit.bss) {
		t.Fatalf("scratch gap conflict: %v", err)
	}
}

func TestBSSInstallExplicitZerosAndRejectAtomically(t *testing.T) {
	for _, name := range []string{"valid", "missing", "extra", "address", "section", "short", "long", "nonzero", "original", "gap original", "hash"} {
		t.Run(name, func(t *testing.T) {
			m, unit, sections, original := unitDataOutputFixture()
			span := rodataSpec{Addr: m.Load + 0xc8, Size: 4, Hash: shortHash(make([]byte, 4))}
			unit.bss = []rodataSpec{span}
			m.Units[0].Bss = unit.bss
			sections[".bss"] = make([]byte, 4)
			copy(original[0xc8:0xcc], sections[".bss"])
			compiled := sliceUnitOwnedData(unit, sections)
			last := len(compiled) - 1
			switch name {
			case "missing":
				compiled = compiled[:last]
			case "extra":
				compiled = append(compiled, compiled[last])
			case "address":
				compiled[last].addr++
			case "section":
				compiled[last].section = ".data"
			case "short":
				compiled[last].data = make([]byte, 3)
			case "long":
				compiled[last].data = make([]byte, 5)
			case "nonzero":
				compiled[last].data[0] = 1
			case "original":
				original[0xc8] = 1
			case "gap original":
				m.Units[0].Bss[0].Size = 1
				m.Units[0].Bss[0].Hash = shortHash([]byte{0})
				m.Units[0].Bss = append(m.Units[0].Bss, rodataSpec{Addr: m.Load + 0xcc, Size: 4, Hash: shortHash(make([]byte, 4))})
				original[0xca] = 1
				compiled[last].data = make([]byte, 1)
				compiled = append(compiled, compiledDataSpan{".bss", m.Load + 0xcc, make([]byte, 4)})
			case "hash":
				m.Units[0].Bss[0].Hash = "wrong"
			}
			output := bytes.Repeat([]byte{0xcc}, m.Size)
			before := append([]byte(nil), output...)
			err := writeUnitOwnedData(m, &m.Units[0], compiled, original, output)
			if name != "valid" {
				if err == nil || !bytes.Equal(before, output) {
					t.Fatalf("invalid install not atomic: %v", err)
				}
				return
			}
			if err != nil {
				t.Fatal(err)
			}
			expected := append([]byte(nil), before...)
			for _, span := range compiled {
				copy(expected[m.offset(span.addr):], span.data)
			}
			if !bytes.Equal(output, expected) || !bytes.Equal(output[0xc8:0xcc], make([]byte, 4)) {
				t.Fatal("BSS zeros were not explicitly installed")
			}
		})
	}
}

// Synthetic originals are made only for these tooling fixtures; no production
// source, metadata, hash or game input is changed by the test.
func compiledBSSFixture(t *testing.T) (project, *projectConfig, *unitInstance, string, []byte) {
	t.Helper()
	p := validTestProject(t)
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	m := config.byID["event-card"]
	m.Profile = "gcc-2.6.3_O2_aspsx-2.34"
	f := &m.Functions[0]
	f.Unit = "state"
	source := "src/event/card_entry.c"
	raw := []byte{3, 0, 0, 0, 7, 0, 0, 0}
	m.Data = []symbolSpec{{Name: "g_values", Addr: m.Load + 0x84}, {Name: "g_current", Addr: m.Load + 0xc8}, {Name: "g_second", Addr: m.Load + 0xcc}}
	m.Units = []unitDef{{Name: "state", Source: source, Data: []rodataSpec{{Addr: m.Load + 0x84, Size: 8, Hash: shortHash(raw)}}, Bss: []rodataSpec{{Addr: m.Load + 0xc8, Size: 4, Hash: shortHash(make([]byte, 4))}, {Addr: m.Load + 0xcc, Size: 4, Hash: shortHash(make([]byte, 4))}}}}
	unit, err := m.unitInstance(config, "state")
	if err != nil {
		t.Fatal(err)
	}
	writeTestFile(t, filepath.Join(p.root, "include", "objects.h"), []byte("int g_values[2] = {3,7};\nint g_current;\nint g_second;\n"))
	source = filepath.Join(p.root, source)
	writeTestFile(t, source, []byte("#include \"objects.h\"\nvoid card_entry(void) {g_current = g_values[0]; g_second = g_values[1];}\n"))
	image := bytes.Repeat([]byte{0xa5}, m.Size)
	copy(image[0x84:], raw)
	copy(image[0xc8:], make([]byte, 8))
	saveBSSFixtureOriginal(t, p, m, image)
	return p, config, unit, source, image
}

func saveBSSFixtureOriginal(t *testing.T, p project, m *moduleSpec, image []byte) {
	t.Helper()
	m.SHA256 = fmt.Sprintf("%x", sha256.Sum256(image))
	writeTestFile(t, filepath.Join(p.filesRoot, m.File), image)
}

func TestBSSMixedUnitColdFillHitAndBuild(t *testing.T) {
	requirePinnedToolchain(t)
	p, config, unit, source, image := compiledBSSFixture(t)
	t.Setenv("TOOLS_CACHE", "0")
	baseline, err := p.compileUnitInstance(newSymbolResolver(config), unit, source, filepath.Join(p.root, "bootstrap"))
	if err != nil {
		t.Fatal(err)
	}
	f := unit.members[0]
	f.Size = len(baseline[".text"])
	f.Hash = shortHash(baseline[".text"])
	unit.size = f.Size
	copy(image[unit.module.offset(f.Addr):], baseline[".text"])
	saveBSSFixtureOriginal(t, p, unit.module, image)
	if problems := func() []string { _, p := compareUnitSections(unit, baseline); return p }(); len(problems) > 0 {
		t.Fatal(problems)
	}
	for _, mode := range []string{"cold", "fill", "hit"} {
		t.Run(mode, func(t *testing.T) {
			cache := "1"
			if mode == "cold" {
				cache = "0"
			}
			t.Setenv("TOOLS_CACHE", cache)
			work := filepath.Join(p.root, mode)
			sections, err := p.compileUnitInstance(newSymbolResolver(config), unit, source, work)
			if err != nil {
				t.Fatal(err)
			}
			if !reflect.DeepEqual(sections, baseline) {
				t.Fatal("cold/hit section mismatch")
			}
			object := filepath.Join(work, unit.module.ID, unit.profile.name+".o")
			if mode == "hit" {
				if _, err := os.Stat(object); !os.IsNotExist(err) {
					t.Fatalf("not cache hit: %v", err)
				}
			} else {
				linked, err := elf.Open(filepath.Join(work, unit.module.ID, unit.profile.name+".elf"))
				if err != nil {
					t.Fatal(err)
				}
				defer linked.Close()
				bss := linked.Section(".bss")
				if bss == nil || bss.Addr != uint64(unit.bss[0].Addr) || bss.Size != 8 || bss.Type != elf.SHT_NOBITS {
					t.Fatalf("bad allocation: %+v", bss)
				}
				t.Logf("exact-profile %s: .bss NOBITS 0x%x+%d; initialized data at0x%x", unit.profile.name, bss.Addr, bss.Size, linked.Section(".data").Addr)
			}
			output := bytes.Repeat([]byte{0xcc}, unit.module.Size)
			if err := writeUnitOwnedData(unit.module, &unit.module.Units[0], sliceUnitOwnedData(unit, sections), image, output); err != nil {
				t.Fatal(err)
			}
			if !bytes.Equal(output[0xc8:0xd0], make([]byte, 8)) {
				t.Fatal("missing zero installation")
			}
		})
	}
	var log bytes.Buffer
	p.out = &log
	path, err := p.buildModule(config, newSymbolResolver(config), unit.module)
	if err != nil {
		t.Fatal(err)
	}
	output, err := os.ReadFile(path)
	if err != nil || !bytes.Equal(output, image) {
		t.Fatalf("build mismatch: %v", err)
	}
	if strings.Count(log.String(), "matching C zero allocation:") != 2 {
		t.Fatal(log.String())
	}
	// Build repeats original proof even with a valid synthetic-image SHA and
	// a supplied zero hash. Configured check instead needs no original input.
	image[0xc8] = 1
	saveBSSFixtureOriginal(t, p, unit.module, image)
	if _, err := p.buildModule(config, newSymbolResolver(config), unit.module); err == nil || !strings.Contains(err.Error(), "nonzero") {
		t.Fatalf("nonzero original accepted: %v", err)
	}
	// Configured compile/check is intentionally independent of extracted inputs.
	if err := os.RemoveAll(p.filesRoot); err != nil {
		t.Fatal(err)
	}
	work := filepath.Join(p.root, "without-original")
	sections, err := p.compileUnitInstance(newSymbolResolver(config), unit, source, work)
	if err != nil {
		t.Fatal(err)
	}
	if _, problems := compareUnitSections(unit, sections); len(problems) > 0 {
		t.Fatal(problems)
	}
	writeUnitContractConfig(t, p, config)
	if err := p.checkCommand([]string{"--module", unit.module.ID}); err != nil {
		t.Fatalf("configured check requires original: %v", err)
	}
}

func TestBSSNamedIdentityColdFillHitAndBindingChange(t *testing.T) {
	requirePinnedToolchain(t)
	p, config, unit, source, _ := compiledBSSFixture(t)
	original := []byte("int g_values[2]={3,7};\nint g_current;\nint g_second;\n")
	header := filepath.Join(p.root, "include", "objects.h")
	for _, swapped := range []bool{false, true} {
		body := original
		if swapped {
			body = []byte("int g_values[2]={3,7};\nint g_second;\nint g_current;\n")
		}
		writeTestFile(t, header, body)
		for _, mode := range []string{"cold", "fill", "hit"} {
			t.Run(fmt.Sprintf("swap%t/%s", swapped, mode), func(t *testing.T) {
				cache := "1"
				if mode == "cold" {
					cache = "0"
				}
				t.Setenv("TOOLS_CACHE", cache)
				work := filepath.Join(p.root, fmt.Sprintf("swap%t", swapped), mode)
				_, err := p.compileUnitInstance(newSymbolResolver(config), unit, source, work)
				if swapped {
					if err == nil || !strings.Contains(err.Error(), "target binds") {
						t.Fatalf("equal zeros exchanged identity: %v", err)
					}
				} else if err != nil {
					t.Fatal(err)
				}
				if mode == "hit" {
					if _, err := os.Stat(filepath.Join(work, unit.module.ID, unit.profile.name+".o")); !os.IsNotExist(err) {
						t.Fatal("not hit")
					}
				}
			})
		}
	}
	// Header definitions do not enter lexical linker bindings, so target edits
	// must be detected from cached named definitions with an unchanged key.
	writeTestFile(t, header, original)
	writeTestFile(t, source, []byte("#include \"objects.h\"\nvoid card_entry(void) {}\n"))
	t.Setenv("TOOLS_CACHE", "1")
	if _, err := p.compileUnitInstance(newSymbolResolver(config), unit, source, filepath.Join(p.root, "binding-fill")); err != nil {
		t.Fatal(err)
	}
	unit.module.Data[1].Addr, unit.module.Data[2].Addr = unit.module.Data[2].Addr, unit.module.Data[1].Addr
	work := filepath.Join(p.root, "binding-change")
	_, err := p.compileUnitInstance(newSymbolResolver(config), unit, source, work)
	if err == nil || !strings.Contains(err.Error(), "target binds") {
		t.Fatalf("cache binding change accepted: %v", err)
	}
	if _, err := os.Stat(filepath.Join(work, unit.module.ID, unit.profile.name+".o")); !os.IsNotExist(err) {
		t.Fatal("binding check did not hit cache")
	}
}

func TestBSSCacheMissingEvidenceRecompiles(t *testing.T) {
	requirePinnedToolchain(t)
	for _, name := range []string{"metadata", "COMMON", "payload keys"} {
		t.Run(name, func(t *testing.T) {
			p, config, unit, source, _ := compiledBSSFixture(t)
			t.Setenv("TOOLS_CACHE", "1")
			if _, err := p.compileUnitInstance(newSymbolResolver(config), unit, source, filepath.Join(p.root, "fill")); err != nil {
				t.Fatal(err)
			}
			paths, err := filepath.Glob(filepath.Join(p.root, "build", "cache", "tools", "*.json"))
			if err != nil || len(paths) != 1 {
				t.Fatalf("cache files %v: %v", paths, err)
			}
			data, err := os.ReadFile(paths[0])
			if err != nil {
				t.Fatal(err)
			}
			var result historicalResult
			if err = json.Unmarshal(data, &result); err != nil {
				t.Fatal(err)
			}
			switch name {
			case "metadata":
				result.Allocations = nil
			case "COMMON":
				result.NoCommon = false
			case "payload keys":
				result.Sections[".unexpected"] = nil
			}
			data, err = json.Marshal(result)
			if err != nil {
				t.Fatal(err)
			}
			writeTestFile(t, paths[0], data)
			work := filepath.Join(p.root, "retry")
			if _, err := p.compileUnitInstance(newSymbolResolver(config), unit, source, work); err != nil {
				t.Fatal(err)
			}
			if _, err := os.Stat(filepath.Join(work, unit.module.ID, unit.profile.name+".o")); err != nil {
				t.Fatalf("bad cache entry not recompiled: %v", err)
			}
		})
	}
}

func TestBSSRejectsELFCommonBeforeLink(t *testing.T) {
	requirePinnedToolchain(t)
	root := t.TempDir()
	source := filepath.Join(root, "common.s")
	object := filepath.Join(root, "common.o")
	writeTestFile(t, source, []byte(".comm g_common,4,4\n"))
	if err := runInDir(root, "mipsel-linux-gnu-as", "-EL", "-march=r3000", "-o", object, source); err != nil {
		t.Fatal(err)
	}
	if err := rejectHistoricalCommon(object); err == nil || !strings.Contains(err.Error(), "COMMON") {
		t.Fatalf("ELF COMMON accepted: %v", err)
	}
}

func TestBSSSchemaRoundTripAndOwnershipRefusals(t *testing.T) {
	p, config := unitContractTestProject(t)
	for _, name := range []string{"unit_c.c", "unit_d.c", "unit_e.c"} {
		if err := os.Remove(filepath.Join(p.root, "src/battle", name)); err != nil {
			t.Fatal(err)
		}
	}
	m := config.byID["battle"]
	span := rodataSpec{Addr: 0x80067708, Size: 4, Hash: shortHash(make([]byte, 4))}
	m.Units[0].Bss = []rodataSpec{span}
	m.Data = append(m.Data, symbolSpec{Name: "g_current", Addr: span.Addr})
	writeUnitContractConfig(t, p, config)
	after, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(after.byID["battle"].Units[0].Bss, []rodataSpec{span}) {
		t.Fatal("BSS roundtrip failed")
	}
	if err := p.validateProjectConfig(after); err != nil {
		t.Fatal(err)
	}
	before := renderConfigFiles(after)
	if err := p.unitSymbolsCommand([]string{"--name", "merged", "--source", "src/battle/merged.c", "battle:unit_a", "battle:unit_b"}); err == nil || !strings.Contains(err.Error(), "owns bss") {
		t.Fatalf("BSS absorption not refused: %v", err)
	}
	if err := p.splitSymbolsCommand([]string{"--unit", "original_a"}); err == nil || !strings.Contains(err.Error(), "bss") {
		t.Fatalf("BSS split not refused: %v", err)
	}
	current, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(before, renderConfigFiles(current)) {
		t.Fatal("refused operation changed metadata")
	}
	if _, err := os.Stat(filepath.Join(p.root, "src/battle/merged.c")); !os.IsNotExist(err) {
		t.Fatal("refused merge wrote source")
	}
	for _, name := range []string{"hash", "label", "bounds"} {
		t.Run(name, func(t *testing.T) {
			bad := *current.byID["battle"]
			bad.Units = append([]unitDef(nil), bad.Units...)
			bad.Units[0].Bss = append([]rodataSpec(nil), bad.Units[0].Bss...)
			switch name {
			case "hash":
				bad.Units[0].Bss[0].Hash = "0123456789abcdef"
			case "label":
				bad.Units[0].Bss[0].Addr += 4
			case "bounds":
				bad.Units[0].Bss[0].Size = int(^uint(0) >> 1)
			}
			for i, original := range current.Modules {
				if original.ID == "battle" {
					current.Modules[i] = &bad
					defer func() { current.Modules[i] = original }()
					break
				}
			}
			if err := p.validateProjectConfig(current); err == nil {
				t.Fatal("invalid BSS config accepted")
			}
		})
	}
}

func TestBSSChecksumsUseOnlyOriginalZeros(t *testing.T) {
	for _, nonzero := range []bool{false, true} {
		t.Run(fmt.Sprintf("nonzero%t", nonzero), func(t *testing.T) {
			p, config, unit, source, image := compiledBSSFixture(t)
			// checksums must not consult edited C or a candidate's compiled output.
			writeTestFile(t, source, []byte("int g_current = 123; this is deliberately invalid C\n"))
			unit.module.Units[0].Bss[0].Hash = ""
			for _, m := range config.Modules {
				original := make([]byte, m.Size)
				if m == unit.module {
					original = append([]byte(nil), image...)
					if nonzero {
						original[0xc8] = 1
					}
				}
				saveBSSFixtureOriginal(t, p, m, original)
			}
			writeUnitContractConfig(t, p, config)
			before := renderConfigFiles(config)
			err := p.checksumsCommand(nil)
			if nonzero {
				if err == nil || !strings.Contains(err.Error(), "nonzero") {
					t.Fatalf("nonzero original with matching SHA accepted: %v", err)
				}
				current, loadErr := p.loadProjectConfig()
				if loadErr != nil {
					t.Fatal(loadErr)
				}
				if !reflect.DeepEqual(before, renderConfigFiles(current)) {
					t.Fatal("failed checksums changed target metadata")
				}
				return
			}
			if err != nil {
				t.Fatal(err)
			}
			current, err := p.loadProjectConfig()
			if err != nil {
				t.Fatal(err)
			}
			bss := current.byID[unit.module.ID].Units[0].Bss
			if bss[0].Hash != shortHash(make([]byte, 4)) || bss[1].Hash != shortHash(make([]byte, 4)) {
				t.Fatalf("hashes did not come from original zeros: %+v", bss)
			}
			if current.byID[unit.module.ID].Units[0].Data[0].Hash != shortHash(image[0x84:0x8c]) {
				t.Fatal("initialized data changed")
			}
		})
	}
}

func TestBSSTrialDataAndRodataCannotClaimOtherBSSGap(t *testing.T) {
	for _, section := range []string{"data", "rodata", "literal"} {
		t.Run(section, func(t *testing.T) {
			unit, _, image := bssEvidenceFixture()
			m := unit.module
			first := unit.bss[0]
			second := first
			second.Addr += 8
			m.Units[0].Bss = append(m.Units[0].Bss, second)
			unit.bss = nil
			if section == "literal" {
				m.Functions[0].Rodata = []rodataSpec{{Addr: first.Addr + 4, Size: 4}}
				if err := validateUnitOwnedLayout(m); err == nil {
					t.Fatal("invalid BSS gap with original literal accepted")
				}
				return
			}
			var data, rodata []unitTrialRangeYAML
			addition := []unitTrialRangeYAML{{Addr: first.Addr + 4, Size: 4}}
			if section == "data" {
				data = addition
			} else {
				rodata = addition
			}
			if err := extendUnitTrialRanges(unit, data, rodata, image); err == nil {
				t.Fatal("scratch span accepted inside other owner's invalid BSS gap")
			}
			if len(unit.data) != 0 || len(unit.rodata) != 0 {
				t.Fatal("failed addition changed candidate")
			}
		})
	}
}

func TestBSSMissingTextStillRejectsConfiguredUnit(t *testing.T) {
	unit, result, _ := bssEvidenceFixture()
	delete(result.Sections, ".text")
	delete(result.Allocations, ".text")
	result.Definitions = result.Definitions[1:]
	// Low-level compilation records only real ELF allocations. The existing
	// mixed-unit function contracts independently require their original text.
	sections, err := result.checkedUnitSections(unit)
	if err != nil {
		t.Fatal(err)
	}
	results, problems := compareUnitSections(unit, sections)
	if len(problems) == 0 || len(results) != 1 || results[0].ok {
		t.Fatal("missing text accepted as a mixed configured unit")
	}
}

func TestBSSSingleScalarExactHistoricalAllocation(t *testing.T) {
	requirePinnedToolchain(t)
	t.Setenv("TOOLS_CACHE", "0")
	p, config, unit, source, _ := compiledBSSFixture(t)
	unit.bss = unit.bss[:1]
	unit.module.Units[0].Bss = unit.module.Units[0].Bss[:1]
	writeTestFile(t, filepath.Join(p.root, "include/objects.h"), []byte("int g_values[2]={3,7};\nint g_current;\n"))
	writeTestFile(t, source, []byte("#include \"objects.h\"\nvoid card_entry(void){g_current=g_values[0];}\n"))
	work := filepath.Join(p.root, "single")
	sections, err := p.compileUnitInstance(newSymbolResolver(config), unit, source, work)
	if err != nil {
		t.Fatal(err)
	}
	linked, err := elf.Open(filepath.Join(work, unit.module.ID, unit.profile.name+".elf"))
	if err != nil {
		t.Fatal(err)
	}
	defer linked.Close()
	bss := linked.Section(".bss")
	if bss == nil || bss.Type != elf.SHT_NOBITS || bss.Addr != uint64(unit.bss[0].Addr) || bss.Size != 4 || !bytes.Equal(sections[".bss"], make([]byte, 4)) {
		t.Fatal("single scalar allocation differs")
	}
	if !bytes.Equal(sections[".data"], []byte{3, 0, 0, 0, 7, 0, 0, 0}) {
		t.Fatal("initialized data changed")
	}
	t.Logf("%s: exact scalar SHT_NOBITS 0x%x+%d with real g_current definition; initialized data unchanged", unit.profile.name, bss.Addr, bss.Size)
}

func TestBSSTrialInheritedRangesRequireVerifiedOriginal(t *testing.T) {
	p, config, unit, _, image := compiledBSSFixture(t)
	writeUnitContractConfig(t, p, config)
	path := "build/inherited.yaml"
	writeTestFile(t, filepath.Join(p.root, path), []byte("name: inherited\nsource: build/inherited.c\nfunctions: [card_entry]\ninstances: [{module: event-card, original_unit: state}]\n"))
	// This path must reject before attempting candidate compilation.
	if err := os.RemoveAll(p.filesRoot); err != nil {
		t.Fatal(err)
	}
	if err := p.unitCheckCommand([]string{"--spec", path}); err != errNotExtracted {
		t.Fatalf("scratch inherited BSS skipped original proof: %v", err)
	}
	image[0xc8] = 1
	saveBSSFixtureOriginal(t, p, unit.module, image)
	writeUnitContractConfig(t, p, config)
	if err := p.unitCheckCommand([]string{"--spec", path}); err == nil || !strings.Contains(err.Error(), "nonzero") {
		t.Fatalf("scratch inherited BSS accepted nonzero original: %v", err)
	}
}
