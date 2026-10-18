package main

import (
	"strings"
	"testing"
)

func TestUnitFunctionReferencesStayInActiveBody(t *testing.T) {
	p, config := unitViewsTestProject(t)
	config.byID["battle"].Functions = config.byID["battle"].Functions[:2]
	writeUnitViewFile(t, p, "include/fft/ownership.h", `
extern int g_declared_only;
#ifdef FFT_BATTLE
#define VALUE (g_battle / 3)
#else
#define VALUE g_world
#endif
`)
	const source = "src/lib/merged.c"
	writeUnitViewFile(t, p, source, `
#include "fft/ownership.h"
extern int g_initializer_only;
int *table[] = { &g_initializer_only };
struct record { int g_member; };
#ifdef FFT_BATTLE
int implementation(void) { return VALUE; }
int unit_b(struct record *p) {
    /* g_comment / 3 */
    const char *text = "g_literal % 3";
    return p->g_member;
}
#else
int implementation(void) { return VALUE; }
#endif
`)
	cache := unitSourceCache{}
	for _, module := range []string{"battle", "world", "battle"} {
		m := config.byID[module]
		for i := range m.Functions {
			m.Functions[i].Source = source
			if m.Functions[i].Name == "unit_a" {
				m.Functions[i].SourceSymbol = "implementation"
			}
		}
		var diagnostics unitsReportDiagnostic
		functions := p.loadUnitFunctions(m, cache, &diagnostics)
		if len(diagnostics.UnreadableSources) != 0 {
			t.Fatalf("%s: %v", module, diagnostics.UnreadableSources)
		}
		for _, fn := range functions {
			for _, name := range []string{"g_declared_only", "g_initializer_only", "g_comment", "g_literal", "g_member"} {
				if _, present := fn.idents[name]; present {
					t.Errorf("%s:%s incorrectly references %s", module, fn.f.Name, name)
				}
			}
			want := ""
			if fn.f.Name == "unit_a" {
				want = "g_" + module
			}
			for _, name := range []string{"g_battle", "g_world"} {
				if _, present := fn.idents[name]; present != (name == want) {
					t.Errorf("%s:%s references %s = %t, want %t", module, fn.f.Name, name, present, name == want)
				}
			}
			if wantDivision := module == "battle" && fn.f.Name == "unit_a"; fn.hasDivision != wantDivision {
				t.Errorf("%s:%s division = %t, want %t", module, fn.f.Name, fn.hasDivision, wantDivision)
			}
		}
	}
}

func TestUnitFunctionUnknownViewDoesNotFallBackToWholeSource(t *testing.T) {
	p, config := unitViewsTestProject(t)
	writeUnitViewFile(t, p, "src/world/unit_a.c", "#error unresolved program view\nint unit_a(void) { return g_unproven / 2; }\n")
	var diagnostics unitsReportDiagnostic
	functions := p.loadUnitFunctions(config.byID["world"], unitSourceCache{}, &diagnostics)
	if len(diagnostics.UnreadableSources) != 1 || !strings.Contains(diagnostics.UnreadableSources[0], "unresolved program view") {
		t.Fatalf("missing preprocessing diagnostic: %+v", diagnostics)
	}
	if len(functions) != 1 || len(functions[0].idents) != 0 || functions[0].hasDivision {
		t.Fatalf("unproven source facts were retained: %+v", functions)
	}
}

func TestUnitFunctionViewCacheIncludesVariantValues(t *testing.T) {
	p, config := unitViewsTestProject(t)
	m := config.byID["world"]
	writeUnitViewFile(t, p, "src/world/unit_a.c", `
#if VARIANT_SELECT == 1
int unit_a(void) { return g_first; }
#else
int unit_a(void) { return g_second % 7; }
#endif
`)
	cache := unitSourceCache{}
	for _, value := range []string{"1", "2", "1"} {
		m.Defines = []defineSpec{{Name: "VARIANT_SELECT", Value: value}}
		var diagnostics unitsReportDiagnostic
		functions := p.loadUnitFunctions(m, cache, &diagnostics)
		if len(diagnostics.UnreadableSources) != 0 || len(functions) != 1 {
			t.Fatalf("view %s: %+v, %+v", value, diagnostics, functions)
		}
		if _, first := functions[0].idents["g_first"]; first != (value == "1") || functions[0].hasDivision != (value == "2") {
			t.Fatalf("cached the wrong variant %s: %+v", value, functions[0])
		}
	}
}

func TestUnitFunctionScanHandlesPrivateAndOldStyleBodies(t *testing.T) {
	functions, err := scanUnitFunctionSources(`
static int helper(int index) { int values[] = {1, 2}; { index /= 3; } return values[index]; }
entry(value) int value; { return helper(value) + g_value; }
`)
	if err != nil || len(functions) != 2 {
		t.Fatalf("functions: %v, %v", functions, err)
	}
	if !functions["helper"].hasDivision || functions["entry"].hasDivision {
		t.Fatal("nested-body division leaked between definitions")
	}
	for _, name := range []string{"helper", "g_value"} {
		if _, found := functions["entry"].idents[name]; !found {
			t.Fatalf("old-style function lost reference to %s", name)
		}
	}
	if _, err := scanUnitFunctionSources("int entry(void) {}\nint entry(void) {}\n"); err == nil {
		t.Fatal("duplicate active definitions were silently merged")
	}
}
