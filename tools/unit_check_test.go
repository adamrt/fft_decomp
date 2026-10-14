package main

import (
	"debug/elf"
	"encoding/binary"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

const unitCheckTestYAML = `module: battle
file: BATTLE.BIN
lba: 300
size: 4096
sha256: 33333333333333333333333333333333333333333333333333333333333333dd
load: 0x80067000
source_dir: src/battle
profile: gcc-2.6.3_O2_aspsx-2.34

functions:
  - {addr: 0x80067000, size: 32, name: unit_a, hash: 0000000000000a01}
  - {addr: 0x80067020, size: 32, name: unit_b, hash: 0000000000000b01}
  - {addr: 0x80067040, size: 32, name: unit_c, hash: 0000000000000c01, profile: gcc-2.6.3_O1_aspsx-2.34}
  - {addr: 0x80067070, size: 32, name: unit_d, hash: 0000000000000d01, rodata: [{addr: 0x80067200, size: 16, hash: 0000000000000e01}]}
  - {addr: 0x80067090, size: 32, name: unit_e, hash: 0000000000000e02, rodata: [{addr: 0x80067220, size: 16, hash: 0000000000000e03}]}
  - {addr: 0x800670b0, size: 32, name: unit_f, hash: 0000000000000f01, source: src/battle/unit_e.c, source_symbol: unit_e}

data:
  - {addr: 0x80067300, name: g_value}
  - {addr: 0x80067304, name: g_other}

overrides:
  - {function: unit_a, name: g_other, addr: 0x80067400}
  - {function: unit_b, name: g_other, addr: 0x80067400}
  - {function: unit_c, name: g_other, addr: 0x80067500}
`

func unitCheckTestProject(t *testing.T) (project, *projectConfig) {
	t.Helper()
	p := testProject(t, map[string]string{
		"target/main.yaml":       testMainYAML,
		"target/battle.yaml":     unitCheckTestYAML,
		"src/main/main_first.c":  "void main_first(void) {}\n",
		"src/main/main_second.c": "void main_second(void) {}\n",
		"src/battle/unit_a.c":    "#include \"fft/unit.h\"\nextern int g_value;\n/* a */\nint unit_a(void) { return g_value; }\n",
		"src/battle/unit_b.c":    "#include \"fft/unit.h\"\n#include \"fft/other.h\"\nextern int g_other;\n/* b */\nint unit_b(void) { return unit_a() + g_other; }\n",
		"src/battle/unit_c.c":    "int unit_c(void) { return 3; }\n",
		"src/battle/unit_d.c":    "int unit_d(void) { return 4; }\n",
		"src/battle/unit_e.c":    "int unit_e(void) { return 5; }\n",
	})
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	return p, config
}

func TestUnitSpecParsingAndDefaults(t *testing.T) {
	spec, err := parseUnitSpec("build/units/thread.yaml", []byte("functions: [a, b]\ninstances:\n  - {module: battle}\n"))
	if err != nil {
		t.Fatal(err)
	}
	if spec.name != "thread" || spec.source != "build/units/thread/thread.c" || len(spec.instances) != 1 {
		t.Fatalf("defaults: %+v", spec)
	}
	for _, text := range []string{
		"functions: [a]\ninstances: []\n",
		"functions: [a]\nsource: src/shared/thread/thread.c\ninstances:\n  - {module: battle}\n",
		"functions: [a, a]\ninstances:\n  - {module: battle}\n",
		"functions: [a]\nunknown: 1\ninstances:\n  - {module: battle}\n",
		"instances:\n  - {module: battle}\n",
		"functions: [a]\ninstances:\n  - {functions: [a]}\n",
	} {
		if _, err := parseUnitSpec("build/units/x.yaml", []byte(text)); err == nil {
			t.Errorf("accepted spec:\n%s", text)
		}
	}
}

func TestResolveUnitInstanceChecksOrderProfileAndRodata(t *testing.T) {
	_, config := unitCheckTestProject(t)
	spec := &unitSpec{name: "ab", source: "build/units/ab/ab.c"}
	unit, err := resolveUnitInstance(config, spec, unitInstanceYAML{Module: "battle", Functions: []string{"unit_a", "unit_b"}})
	if err != nil {
		t.Fatal(err)
	}
	if unit.addr != 0x80067000 || unit.size != 64 || unit.profile.name != "gcc-2.6.3_O2_aspsx-2.34" || strings.Join(unit.symbols(), " ") != "unit_a unit_b" {
		t.Fatalf("resolved unit: %+v", unit)
	}
	if _, err := resolveUnitInstance(config, spec, unitInstanceYAML{Module: "battle", Functions: []string{"unit_a", "unit_b"}, Profile: "gcc-2.6.3_O2_aspsx-2.34"}); err != nil {
		t.Fatal(err)
	}
	for name, instance := range map[string]unitInstanceYAML{
		"misorder":            {Module: "battle", Functions: []string{"unit_b", "unit_a"}},
		"gap":                 {Module: "battle", Functions: []string{"unit_c", "unit_d"}},
		"profile":             {Module: "battle", Functions: []string{"unit_b", "unit_c"}},
		"profile assert":      {Module: "battle", Functions: []string{"unit_a"}, Profile: "gcc-2.6.3_O1_aspsx-2.34"},
		"unknown name":        {Module: "battle", Functions: []string{"unit_a", "nowhere"}},
		"unknown module":      {Module: "nowhere", Functions: []string{"unit_a"}},
		"rodata not adjacent": {Module: "battle", Functions: []string{"unit_d", "unit_e"}},
	} {
		if _, err := resolveUnitInstance(config, spec, instance); err == nil {
			t.Errorf("%s: accepted %+v", name, instance)
		}
	}
	alias, err := resolveUnitInstance(config, spec, unitInstanceYAML{Module: "battle", Functions: []string{"unit_e", "unit_f"}})
	if err != nil {
		t.Fatal(err)
	}
	if strings.Join(alias.symbols(), " ") != "unit_e unit_e" {
		t.Fatalf("source symbols: %v", alias.symbols())
	}
}

func TestUnitLinkerBindingsAndScript(t *testing.T) {
	_, config := unitCheckTestProject(t)
	resolver := newSymbolResolver(config)
	spec := &unitSpec{name: "ab"}
	unit, err := resolveUnitInstance(config, spec, unitInstanceYAML{Module: "battle", Functions: []string{"unit_a", "unit_b"}})
	if err != nil {
		t.Fatal(err)
	}
	used := map[string]struct{}{"g_value": {}, "g_other": {}, "unit_a": {}, "unit_b": {}, "memset": {}}
	bindings, err := unitLinkerBindings(resolver, unit, used)
	if err != nil {
		t.Fatal(err)
	}
	if bindings["g_value"] != 0x80067300 || bindings["g_other"] != 0x80067400 || bindings["memset"] != 0x80010200 {
		t.Fatalf("bindings: %v", bindings)
	}
	if _, member := bindings["unit_a"]; member {
		t.Fatal("unit members must not be PROVIDEd")
	}
	script := unitLinkerScript(unit, bindings, true)
	want := "OUTPUT_ARCH(mips)\n" +
		"PROVIDE(g_other = 0x80067400);\n" +
		"PROVIDE(g_value = 0x80067300);\n" +
		"PROVIDE(memset = 0x80010200);\n" +
		"SECTIONS {\n" +
		"  .text 0x80067000 : SUBALIGN(4) { *(.text) }\n" +
		"  /DISCARD/ : { *(.reginfo) *(.pdr) *(.comment) *(.gnu.attributes) }\n}\n" +
		"ASSERT(unit_a == 0x80067000, \"unit_a must begin at 0x80067000\");\n" +
		"ASSERT(unit_b == 0x80067020, \"unit_b must begin at 0x80067020\");\n"
	if script != want {
		t.Fatalf("script:\n%s\nwant:\n%s", script, want)
	}
	if plain := unitLinkerScript(unit, bindings, false); strings.Contains(plain, "ASSERT") {
		t.Fatal("diagnostic script must not assert")
	}
	conflicting, err := resolveUnitInstance(config, spec, unitInstanceYAML{Module: "battle", Functions: []string{"unit_b", "unit_c"}})
	if err == nil {
		// unit_b and unit_c disagree on g_other; they also disagree on profile, so build the pair by hand.
		t.Fatal("expected profile disagreement")
	}
	conflicting = &unitInstance{module: unit.module, members: []*functionSpec{unit.module.function("unit_b"), unit.module.function("unit_c")}}
	if _, err := unitLinkerBindings(resolver, conflicting, used); err == nil || !strings.Contains(err.Error(), "g_other") {
		t.Fatalf("conflicting overrides: %v", err)
	}
	rodataUnit := &unitInstance{module: unit.module, addr: 0x80067070, members: []*functionSpec{unit.module.function("unit_d")},
		rodata: []rodataSpec{{Addr: 0x80067200, Size: 16}}}
	if script := unitLinkerScript(rodataUnit, nil, false); !strings.Contains(script, ".rodata 0x80067200 : SUBALIGN(4)") {
		t.Fatalf("rodata placement:\n%s", script)
	}
}

func TestCompareUnitSectionsSplitsText(t *testing.T) {
	first := []byte(strings.Repeat("a", 32))
	second := []byte(strings.Repeat("b", 32))
	members := []*functionSpec{
		{Addr: 0x80067000, Size: 32, Name: "unit_a", Hash: shortHash(first)},
		{Addr: 0x80067020, Size: 32, Name: "unit_b", Hash: shortHash(second)},
	}
	unit := &unitInstance{addr: 0x80067000, size: 64, members: members}
	text := append(append([]byte{}, first...), second...)
	results, problems := compareUnitSections(unit, map[string][]byte{".text": text})
	if len(results) != 2 || !results[0].ok || !results[1].ok || len(problems) != 0 {
		t.Fatalf("clean unit: %+v %v", results, problems)
	}
	corrupted := append([]byte{}, text...)
	corrupted[40] ^= 1
	results, problems = compareUnitSections(unit, map[string][]byte{".text": corrupted, ".data": {1, 2, 3, 4}})
	if !results[0].ok || results[1].ok || len(problems) != 1 || !strings.Contains(problems[0], ".data") {
		t.Fatalf("corrupted unit: %+v %v", results, problems)
	}
	results, problems = compareUnitSections(unit, map[string][]byte{".text": text[:48]})
	if !results[0].ok || results[1].ok || len(problems) != 1 {
		t.Fatalf("truncated unit: %+v %v", results, problems)
	}
	results, problems = compareUnitSections(unit, map[string][]byte{".text": append(text, 0, 0, 0, 0)})
	if !results[0].ok || !results[1].ok || len(problems) != 1 {
		t.Fatalf("trailing bytes: %+v %v", results, problems)
	}
}

func TestMergeAndValidateUnitSource(t *testing.T) {
	p, _ := unitCheckTestProject(t)
	merged, err := mergeUnitSources(p.root, []string{"src/battle/unit_a.c", "src/battle/unit_b.c"})
	if err != nil {
		t.Fatal(err)
	}
	want := "#include \"fft/unit.h\"\n#include \"fft/other.h\"\n\n" +
		"extern int g_value;\n/* a */\nint unit_a(void) { return g_value; }\n\n" +
		"extern int g_other;\n/* b */\nint unit_b(void) { return unit_a() + g_other; }\n"
	if string(merged) != want {
		t.Fatalf("merged:\n%s", merged)
	}
	path := filepath.Join(p.root, "build", "units", "ab", "ab.c")
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(path, merged, 0o644); err != nil {
		t.Fatal(err)
	}
	if err := validateUnitSource(p.root, "build/units/ab/ab.c", []string{"unit_a", "unit_b"}); err != nil {
		t.Fatal(err)
	}
	for _, symbols := range [][]string{{"unit_b", "unit_a"}, {"unit_a"}, {"unit_a", "unit_b", "unit_c"}} {
		if err := validateUnitSource(p.root, "build/units/ab/ab.c", symbols); err == nil {
			t.Errorf("accepted order %v", symbols)
		}
	}
}

// The real proof: two functions, the second calling the first, compiled
// separately to learn their bytes, then merged and compiled once.
func TestUnitCompileMatchesSeparateCompilesAndResolvesInternalCalls(t *testing.T) {
	requirePinnedToolchain(t)
	t.Setenv("TOOLS_CACHE", "0")
	p := testProject(t, map[string]string{
		"target/main.yaml":       testMainYAML,
		"target/battle.yaml":     unitCheckTestYAML,
		"src/battle/unit_a.c":    "extern int g_value;\nint unit_a(void) { return g_value; }\n",
		"src/battle/unit_b.c":    "extern int unit_a(void);\nint unit_b(int n) { return unit_a() + n; }\n",
		"src/battle/unit_c.c":    "int unit_c(void) { return 3; }\n",
		"src/battle/unit_d.c":    "int unit_d(void) { return 4; }\n",
		"src/battle/unit_e.c":    "int unit_e(void) { return 5; }\n",
		"src/main/main_first.c":  "void main_first(void) {}\n",
		"src/main/main_second.c": "void main_second(void) {}\n",
	})
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	m, _ := config.module("battle")
	resolver := newSymbolResolver(config)
	work := filepath.Join(p.root, "build", "separate")
	a, b := m.function("unit_a"), m.function("unit_b")
	sections, err := p.compileFunction(resolver, m, a, work)
	if err != nil {
		t.Fatal(err)
	}
	a.Size, a.Hash = len(sections[".text"]), shortHash(sections[".text"])
	b.Addr = a.Addr + uint32(a.Size)
	resolver = newSymbolResolver(config)
	sections, err = p.compileFunction(resolver, m, b, work)
	if err != nil {
		t.Fatal(err)
	}
	b.Size, b.Hash = len(sections[".text"]), shortHash(sections[".text"])
	spec := &unitSpec{name: "ab", source: "build/units/ab/ab.c"}
	unit, err := resolveUnitInstance(config, spec, unitInstanceYAML{Module: "battle", Functions: []string{"unit_a", "unit_b"}})
	if err != nil {
		t.Fatal(err)
	}
	merged, err := mergeUnitSources(p.root, []string{"src/battle/unit_a.c", "src/battle/unit_b.c"})
	if err != nil {
		t.Fatal(err)
	}
	source := filepath.Join(p.root, filepath.FromSlash(spec.source))
	if err := os.MkdirAll(filepath.Dir(source), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(source, merged, 0o644); err != nil {
		t.Fatal(err)
	}
	workDir := filepath.Join(p.root, "build", "units", "work", "ab")
	sections, err = p.compileUnitInstance(resolver, unit, source, workDir)
	if err != nil {
		t.Fatal(err)
	}
	results, problems := compareUnitSections(unit, sections)
	if len(problems) != 0 || !results[0].ok || !results[1].ok {
		t.Fatalf("merged unit: %+v %v", results, problems)
	}
	text := sections[".text"]
	called := false
	for offset := int(b.Addr - unit.addr); offset+4 <= len(text); offset += 4 {
		word := binary.LittleEndian.Uint32(text[offset:])
		if word>>26 == 3 && uint32((uint64(unit.addr)+uint64(offset)+4)&0xf0000000)|(word&0x03ffffff)<<2 == a.Addr {
			called = true
		}
	}
	if !called {
		t.Fatal("unit_b does not call unit_a at its placed address")
	}
	linked, err := elf.Open(filepath.Join(workDir, "battle", unit.profile.name+".elf"))
	if err != nil {
		t.Fatal(err)
	}
	symbols, err := linked.Symbols()
	linked.Close()
	if err != nil {
		t.Fatal(err)
	}
	for _, symbol := range symbols {
		if symbol.Name == "unit_b" && symbol.Value != uint64(b.Addr) {
			t.Fatalf("unit_b placed at %#x, want %#x", symbol.Value, b.Addr)
		}
	}
	// Reversing the bodies breaks the order validation first; growing the
	// first body shifts the second and trips the address assertion.
	grown := strings.Replace(string(merged), "return g_value;", "if (g_value > 7) { return g_value * 9 + 1; } return g_value;", 1)
	if err := os.WriteFile(source, []byte(grown), 0o644); err != nil {
		t.Fatal(err)
	}
	if _, err := p.compileUnitInstance(resolver, unit, source, workDir); err == nil || !strings.Contains(err.Error(), "placed at") {
		t.Fatalf("grown first body: %v", err)
	}
}
