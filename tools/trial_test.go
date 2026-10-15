package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestTrialBatchParsesAndValidates(t *testing.T) {
	specs, err := parseTrialBatch([]byte(`trials:
  - {module: battle, entry: battle_helper, source: src/event/attack_entry.c, symbol: attack_entry, subst: {g_main_value: g_battle_value}, defines: {VARIANT_N: 16}}
  - module: main
    entry: main_first
    source: src/main/main_second.c
`))
	if err != nil {
		t.Fatal(err)
	}
	if len(specs) != 2 || specs[0].Subst["g_main_value"] != "g_battle_value" || specs[0].Defines["VARIANT_N"] != "16" || specs[1].Symbol != "" {
		t.Fatalf("specs = %+v", specs)
	}
	if specs[1].label() != "main main_first <- src/main/main_second.c" {
		t.Fatalf("label = %q", specs[1].label())
	}
	for name, text := range map[string]string{
		"unknown key":     "trials:\n  - {module: main, entry: main_first, source: a.c, profile: x}\n",
		"missing source":  "trials:\n  - {module: main, entry: main_first}\n",
		"empty":           "trials: []\n",
		"bad define":      "trials:\n  - {module: main, entry: main_first, source: a.c, defines: {PAGES: 1}}\n",
		"bad subst":       "trials:\n  - {module: main, entry: main_first, source: a.c, subst: {\"a b\": c}}\n",
		"define no value": "trials:\n  - {module: main, entry: main_first, source: a.c, defines: {VARIANT_X: many}}\n",
	} {
		if _, err := parseTrialBatch([]byte(text)); err == nil {
			t.Errorf("%s: accepted", name)
		}
	}
}

func TestSubstituteIdentifiersRenamesCodeOnlyInOnePass(t *testing.T) {
	source := []byte("#include \"fft/a.h\"\n/* a stays, b stays */\nint a(void) { return b + ab + \"a b\"[0] + 'a'; }\n")
	got := string(substituteIdentifiers(source, map[string]string{"a": "b", "b": "a"}))
	want := "#include \"fft/a.h\"\n/* a stays, b stays */\nint b(void) { return a + ab + \"a b\"[0] + 'a'; }\n"
	if got != want {
		t.Fatalf("got:\n%s\nwant:\n%s", got, want)
	}
	if string(substituteIdentifiers(source, nil)) != string(source) {
		t.Fatal("empty substitution changed the source")
	}
}

func TestSourceDefinitionSymbolFindsTheSinglePublicFunction(t *testing.T) {
	symbol, err := sourceDefinitionSymbol([]byte("static int helper(void) { return 1; }\nextern int other(void);\nint entry(int n) { return helper() + n; }\n"))
	if err != nil || symbol != "entry" {
		t.Fatalf("symbol = %q, err = %v", symbol, err)
	}
	if _, err := sourceDefinitionSymbol([]byte("int a(void) { return 1; }\nint b(void) { return 2; }\n")); err == nil {
		t.Fatal("two definitions accepted")
	}
}

func TestTrialRunsWithoutTheDiscAndReportsMisses(t *testing.T) {
	p := validTestProject(t)
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	for _, test := range []struct {
		spec trialSpec
		want string
	}{
		{trialSpec{Module: "nowhere", Entry: "x", Source: "src/main/main_first.c"}, "unknown module"},
		{trialSpec{Module: "main", Entry: "missing", Source: "src/main/main_first.c"}, "no function missing"},
		{trialSpec{Module: "main", Entry: "main_first", Source: "../outside.c"}, "inside the project"},
		{trialSpec{Module: "main", Entry: "main_first", Source: "src/main/nowhere.c"}, "no such file"},
	} {
		result := p.runTrial(config, newSymbolResolver(config), map[string][]byte{}, test.spec, 0, 4)
		if result.Status != "error" || !strings.Contains(result.Error, test.want) {
			t.Errorf("%+v: status %s error %q, want %q", test.spec, result.Status, result.Error, test.want)
		}
	}
	if err := p.trialCommand([]string{"--batch=build/missing.yaml"}); err == nil {
		t.Fatal("missing batch accepted")
	}
	if err := p.trialCommand([]string{"--module=main"}); err == nil || !strings.Contains(err.Error(), "usage") {
		t.Fatalf("partial flags: %v", err)
	}
}

func TestTrialDifferencesCountsAlignedRows(t *testing.T) {
	// addiu v0,zero,16 ; jr ra ; nop  versus  addiu v0,zero,17 ; jr ra ; nop
	expected := []byte{0x10, 0x00, 0x02, 0x24, 0x08, 0x00, 0xe0, 0x03, 0, 0, 0, 0}
	compiled := []byte{0x11, 0x00, 0x02, 0x24, 0x08, 0x00, 0xe0, 0x03, 0, 0, 0, 0}
	differing, _, lines := trialDifferences(expected, compiled, 4, nil)
	if differing != 1 || len(lines) != 1 || !strings.Contains(lines[0], "+0x0: li v0,16 | li v0,17") {
		t.Fatalf("differing = %d, lines = %v", differing, lines)
	}
}

// The define reaches the preprocessor: a body compiled with VARIANT_N=16
// matches its own bytes, 17 differs in one instruction.
func TestTrialDefinesReachThePreprocessor(t *testing.T) {
	requirePinnedToolchain(t)
	p := validTestProject(t)
	body := "int main_first(void) { return VARIANT_N; }\n"
	writeTestFile(t, filepath.Join(p.root, "src", "main", "main_first.c"), []byte(body))
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	m, _ := config.module("main")
	m.Defines = []defineSpec{{Name: "VARIANT_N", Value: "16"}}
	t.Setenv("TOOLS_CACHE", "0")
	resolver := newSymbolResolver(config)
	sections, err := p.compileFunction(resolver, m, &m.Functions[0], filepath.Join(p.root, "build", "learn"))
	if err != nil {
		t.Fatal(err)
	}
	text := sections[".text"]
	m.Functions[0].Size, m.Functions[0].Hash = len(text), shortHash(text)
	images := map[string][]byte{}
	same := p.runTrial(config, resolver, images, trialSpec{Module: "main", Entry: "main_first", Source: "src/main/main_first.c"}, 0, 4)
	if same.Status != "match" {
		t.Fatalf("same define: %+v", same)
	}
	other := p.runTrial(config, resolver, images, trialSpec{Module: "main", Entry: "main_first", Source: "src/main/main_first.c", Defines: map[string]string{"VARIANT_N": "17"}}, 1, 4)
	if other.Status != "differ" || len(other.Lines) != 1 || !strings.Contains(other.Lines[0], "extract the disc") {
		t.Fatalf("other define without image: %+v", other)
	}
	// With the image present the differing instruction is named.
	image := make([]byte, m.Size)
	copy(image[m.offset(m.Functions[0].Addr):], text)
	images["main"] = image
	other = p.runTrial(config, resolver, images, trialSpec{Module: "main", Entry: "main_first", Source: "src/main/main_first.c", Defines: map[string]string{"VARIANT_N": "17"}}, 2, 4)
	if other.Status != "differ" || other.Differing != 1 || !strings.Contains(strings.Join(other.Lines, "\n"), "0x10 | ") {
		t.Fatalf("other define with image: %+v", other)
	}
	if _, err := os.Stat(filepath.Join(p.root, "build", "trial", "main", "main_first", "t2", "main_first.c")); err != nil {
		t.Fatalf("trial copy missing: %v", err)
	}
}
