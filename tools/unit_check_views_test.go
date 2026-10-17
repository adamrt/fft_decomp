package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

const unitViewsSource = `#ifdef FFT_BATTLE
int unit_a(void) { return 3; }
int unit_b(void) { return 4; }
#endif
#ifdef FFT_WORLD
int unit_a(void) { return 5; }
#endif
`

const unitViewsSpec = `name: unit_views
source: build/unit_views.c
instances:
  - {module: battle, functions: [unit_a, unit_b]}
  - {module: world, functions: [unit_a]}
`

func writeUnitViewFile(t *testing.T, p project, path, text string) {
	t.Helper()
	full := filepath.Join(p.root, filepath.FromSlash(path))
	if err := os.MkdirAll(filepath.Dir(full), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(full, []byte(text), 0o644); err != nil {
		t.Fatal(err)
	}
}

func unitViewsTestProject(t *testing.T) (project, *projectConfig) {
	t.Helper()
	p, _ := unitCheckTestProject(t)
	writeUnitViewFile(t, p, "target/world.yaml", `module: world
file: WORLD/WORLD.BIN
lba: 1000
size: 4096
sha256: 4444444444444444444444444444444444444444444444444444444444444444
load: 0x800e0000
source_dir: src/world
profile: gcc-2.6.3_O2_aspsx-2.34
functions:
  - {addr: 0x800e0000, size: 32, name: unit_a, hash: 0000000000000a01}
`)
	writeUnitViewFile(t, p, "src/battle/unit_a.c", "int unit_a(void) { return 3; }\n")
	writeUnitViewFile(t, p, "src/battle/unit_b.c", "int unit_b(void) { return 4; }\n")
	writeUnitViewFile(t, p, "src/world/unit_a.c", "int unit_a(void) { return 5; }\n")
	writeUnitViewFile(t, p, "build/unit_views.c", unitViewsSource)
	writeUnitViewFile(t, p, "build/unit_views.yaml", unitViewsSpec)
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	return p, config
}

func TestUnitCheckRejectsWrongActiveMembersBeforeCompiling(t *testing.T) {
	p, _ := unitViewsTestProject(t)
	for name, tc := range map[string]struct {
		source string
		module string
	}{
		"missing member": {
			strings.Replace(unitViewsSource, "int unit_b(void) { return 4; }\n", "", 1), "battle",
		},
		"reordered members": {
			strings.Replace(unitViewsSource, "int unit_a(void) { return 3; }\nint unit_b(void) { return 4; }",
				"int unit_b(void) { return 4; }\nint unit_a(void) { return 3; }", 1), "battle",
		},
		"extra member in other view": {
			strings.Replace(unitViewsSource, "int unit_a(void) { return 5; }", "int unit_a(void) { return 5; }\nint unit_b(void) { return 6; }", 1), "world",
		},
		"duplicate member in other view": {
			strings.Replace(unitViewsSource, "int unit_a(void) { return 5; }", "int unit_a(void) { return 5; }\nint unit_a(void) { return 6; }", 1), "world",
		},
		"wrong program guard": {
			strings.Replace(unitViewsSource, "#ifdef FFT_WORLD", "#ifdef FFT_WLDCORE", 1), "world",
		},
	} {
		t.Run(name, func(t *testing.T) {
			writeUnitViewFile(t, p, "build/unit_views.c", tc.source)
			err := p.unitCheckCommand([]string{"--spec", "build/unit_views.yaml"})
			if err == nil || !strings.Contains(err.Error(), "for "+tc.module+":") || !strings.Contains(err.Error(), "in that order") {
				t.Fatalf("error = %v; want exact active-member rejection for %s", err, tc.module)
			}
		})
	}
}

func TestUnitCheckProgramViewsCompileEachTarget(t *testing.T) {
	requirePinnedToolchain(t)
	t.Setenv("TOOLS_CACHE", "0")
	p, config := unitViewsTestProject(t)
	// Learn synthetic targets from their independent original bodies, then
	// compile the common source with each module's real FFT_ defines.
	for _, original := range []struct {
		module string
		names  []string
	}{{"battle", []string{"unit_a", "unit_b"}}, {"world", []string{"unit_a"}}} {
		m := config.byID[original.module]
		addr := m.Load
		for _, name := range original.names {
			f := m.function(name)
			f.Addr = addr
			sections, err := p.compileFunction(newSymbolResolver(config), m, f, filepath.Join(p.root, "build", "original", m.ID))
			if err != nil {
				t.Fatal(err)
			}
			f.Size, f.Hash = len(sections[".text"]), shortHash(sections[".text"])
			addr += uint32(f.Size)
		}
	}
	for path, data := range renderConfigFiles(config) {
		writeUnitViewFile(t, p, path, string(data))
	}
	if err := p.unitCheckCommand([]string{"--spec", "build/unit_views.yaml"}); err != nil {
		t.Fatal(err)
	}
	changed := strings.Replace(unitViewsSource, "return 5", "return 6", 1)
	writeUnitViewFile(t, p, "build/unit_views.c", changed)
	if err := p.unitCheckCommand([]string{"--spec", "build/unit_views.yaml"}); err == nil || !strings.Contains(err.Error(), "1 of 3 functions differ") {
		t.Fatalf("changed WORLD body escaped the original-byte check: %v", err)
	}
	// Module selection checks only the selected source view. The bad BATTLE
	// view must still be rejected when the whole spec is requested.
	changed = strings.Replace(unitViewsSource, "int unit_b(void) { return 4; }\n", "", 1)
	writeUnitViewFile(t, p, "build/unit_views.c", changed)
	if err := p.unitCheckCommand([]string{"--spec", "build/unit_views.yaml", "--module", "world"}); err != nil {
		t.Fatal(err)
	}
	if err := p.unitCheckCommand([]string{"--spec", "build/unit_views.yaml"}); err == nil || !strings.Contains(err.Error(), "for battle:") {
		t.Fatalf("unselected bad view was accepted by the full check: %v", err)
	}
}

func TestUnitCheckMergeRequiresCompatibleSourcesAndPreservesCandidate(t *testing.T) {
	for _, mode := range []string{"different members", "different sources", "invalid active view"} {
		t.Run(mode, func(t *testing.T) {
			p, config := unitViewsTestProject(t)
			want := "--merge requires identical members"
			if mode != "different members" {
				writeUnitViewFile(t, p, "build/unit_views.yaml", strings.Replace(unitViewsSpec, "[unit_a, unit_b]", "[unit_a]", 1))
				want = "compile from different sources"
			}
			if mode == "invalid active view" {
				shared := "src/shared_view.c"
				config.byID["battle"].function("unit_a").Source = shared
				config.byID["world"].function("unit_a").Source = shared
				for path, data := range renderConfigFiles(config) {
					writeUnitViewFile(t, p, path, string(data))
				}
				writeUnitViewFile(t, p, shared, "#ifdef FFT_BATTLE\nint unit_a(void) { return 3; }\n#endif\n")
				want = "for world:"
			}
			writeUnitViewFile(t, p, "build/unit_views.c", "preserve the existing candidate\n")
			err := p.unitCheckCommand([]string{"--spec", "build/unit_views.yaml", "--merge"})
			if err == nil || !strings.Contains(err.Error(), want) {
				t.Fatalf("error = %v; want %q", err, want)
			}
			candidate, err := os.ReadFile(filepath.Join(p.root, "build", "unit_views.c"))
			if err != nil || string(candidate) != "preserve the existing candidate\n" {
				t.Fatalf("failed automatic merge replaced the candidate: %q, %v", candidate, err)
			}
		})
	}
}
