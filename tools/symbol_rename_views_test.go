package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestUnifySourceDefinitionsUseProgramViews(t *testing.T) {
	battle := &moduleSpec{ID: "battle", Defines: []defineSpec{{Name: "VARIANT_ROLE", Value: "1"}}}
	world := &moduleSpec{ID: "world", Defines: []defineSpec{{Name: "VARIANT_ROLE", Value: "2"}}}
	for _, test := range []struct {
		name    string
		files   map[string]string
		views   map[string][]*moduleSpec
		wantErr string
	}{
		{
			name:  "same_file_exclusive_programs",
			files: map[string]string{"src/lib/menu/selection.c": "#ifdef FFT_BATTLE\nvoid old_role(void) {}\n#elif defined(FFT_WORLD)\nvoid shared_role(void) {}\n#endif\n"},
			views: map[string][]*moduleSpec{"src/lib/menu/selection.c": {battle, world}},
		},
		{
			name:  "same_file_exclusive_variants",
			files: map[string]string{"src/lib/menu/selection.c": "#if VARIANT_ROLE == 1\nvoid old_role(void) {}\n#else\nvoid shared_role(void) {}\n#endif\n"},
			views: map[string][]*moduleSpec{"src/lib/menu/selection.c": {battle, world}},
		},
		{
			name: "same_directory_separate_programs",
			files: map[string]string{
				"src/lib/menu/first.c":  "void old_role(void) {}\n",
				"src/lib/menu/second.c": "void shared_role(void) {}\n",
			},
			views: map[string][]*moduleSpec{"src/lib/menu/first.c": {battle}, "src/lib/menu/second.c": {world}},
		},
		{
			name:    "same_file_real_coexistence",
			files:   map[string]string{"src/lib/menu/selection.c": "void old_role(void) {}\nvoid shared_role(void) {}\n"},
			views:   map[string][]*moduleSpec{"src/lib/menu/selection.c": {battle, world}},
			wantErr: "source definitions collide in battle",
		},
		{
			name:    "static_collision_only_in_second_view",
			files:   map[string]string{"src/lib/menu/selection.c": "#ifdef FFT_BATTLE\nvoid old_role(void) {}\n#endif\n#ifdef FFT_WORLD\nstatic void old_role(void) {}\nvoid shared_role(void) {}\n#endif\n"},
			views:   map[string][]*moduleSpec{"src/lib/menu/selection.c": {battle, world}},
			wantErr: "source definitions collide in world",
		},
		{
			name: "different_directories_same_program",
			files: map[string]string{
				"src/lib/menu/first.c": "void old_role(void) {}\n",
				"src/battle/second.c":  "static void shared_role(void) {}\n",
			},
			views:   map[string][]*moduleSpec{"src/lib/menu/first.c": {battle}, "src/battle/second.c": {battle}},
			wantErr: "source definitions collide in battle",
		},
		{
			name: "unbound_source_is_not_assumed_disjoint",
			files: map[string]string{
				"src/lib/menu/first.c": "void old_role(void) {}\n",
				"src/unused/second.c":  "void shared_role(void) {}\n",
			},
			views:   map[string][]*moduleSpec{"src/lib/menu/first.c": {battle}},
			wantErr: "has no target program view",
		},
		{
			name:    "unknown_macro_cannot_hide_definition",
			files:   map[string]string{"src/lib/menu/selection.c": "#define LOCAL_ROLE 1\nvoid old_role(void) {}\n#if LOCAL_ROLE\nstatic void shared_role(void) {}\n#endif\n"},
			views:   map[string][]*moduleSpec{"src/lib/menu/selection.c": {battle}},
			wantErr: "non-target macro LOCAL_ROLE",
		},
		{
			name:    "local_target_macro_change_cannot_hide_definition",
			files:   map[string]string{"src/lib/menu/selection.c": "#define FFT_WORLD 1\nvoid old_role(void) {}\n#ifdef FFT_WORLD\nstatic void shared_role(void) {}\n#endif\n"},
			views:   map[string][]*moduleSpec{"src/lib/menu/selection.c": {battle}},
			wantErr: "local target macro change",
		},
		{
			name:    "malformed_conditional_is_rejected",
			files:   map[string]string{"src/lib/menu/selection.c": "#ifdef FFT_BATTLE\nvoid old_role(void) {}\n#else\nvoid shared_role(void) {}\n"},
			views:   map[string][]*moduleSpec{"src/lib/menu/selection.c": {battle}},
			wantErr: "unterminated #if block",
		},
	} {
		t.Run(test.name, func(t *testing.T) {
			root := t.TempDir()
			views := make(map[string][]*moduleSpec)
			for path, body := range test.files {
				full := writeSymbolRenameFixture(t, root, path, body)
				views[full] = test.views[path]
			}
			plan, err := planSourceIdentifierRenamesUnify(root, []sourceIdentifierRename{{"old_role", "shared_role"}}, true, views)
			if test.wantErr != "" {
				if err == nil || !strings.Contains(err.Error(), test.wantErr) {
					t.Fatalf("got %v, want %s", err, test.wantErr)
				}
			} else if err != nil {
				t.Fatal(err)
			} else if len(plan.changes) == 0 {
				t.Fatal("no rename planned")
			}
			for path, body := range test.files {
				if got, err := os.ReadFile(filepath.Join(root, filepath.FromSlash(path))); err != nil || string(got) != body {
					t.Fatalf("planning changed %s: %s (%v)", path, got, err)
				}
			}
		})
	}
}

func TestUnifyConditionalUnitUsesOriginalTargetViewsWithoutPartialWrites(t *testing.T) {
	for _, collision := range []bool{false, true} {
		name := "exclusive"
		if collision {
			name = "static_collision"
		}
		t.Run(name, func(t *testing.T) {
			p := unifyFixture(t)
			config, err := p.loadProjectConfig()
			if err != nil {
				t.Fatal(err)
			}
			const source = "src/lib/menu/selection.c"
			for id, name := range map[string]string{"battle": "battle_helper", "event-attack": "attack_entry"} {
				module := config.byID[id]
				entry := module.function(name)
				if err := os.Remove(filepath.Join(p.root, module.sourceOf(entry))); err != nil {
					t.Fatal(err)
				}
				module.Units = []unitDef{{Name: "selection", Source: source}}
				entry.Unit = "selection"
			}
			body := "#include \"fft/menu.h\"\n#ifdef FFT_BATTLE\nvoid battle_helper(void) {}\n#endif\n#if defined(FFT_EVENT) && defined(FFT_EVENT_ATTACK)\n"
			if collision {
				body += "static void battle_helper(void) {}\n"
			}
			body += "void attack_entry(void) {}\n#endif\n"
			writeSymbolRenameFixture(t, p.root, source, body)
			if _, err := p.writeProjectConfig(config); err != nil {
				t.Fatal(err)
			}
			before := make(map[string]string)
			for _, path := range []string{"target/battle.yaml", "target/event.yaml", source, "include/fft/menu.h", "include/fft/battle_menu.h", "include/fft/event_attack.h"} {
				before[path] = readTestFile(t, p, path)
			}
			for _, dryRun := range []bool{true, false} {
				args := []string{"unify", "--new", "attack_entry", "--header", "fft/menu.h"}
				if dryRun {
					args = append(args, "--dry-run")
				}
				err := p.symbolsCommand(append(args, "battle_helper"))
				if collision {
					if err == nil || !strings.Contains(err.Error(), "source definitions collide in event-attack") {
						t.Fatalf("dryRun=%v: collision was not rejected: %v", dryRun, err)
					}
				} else if err != nil {
					t.Fatalf("dryRun=%v: exclusive bodies rejected: %v", dryRun, err)
				}
				if dryRun || collision {
					for path, want := range before {
						if readTestFile(t, p, path) != want {
							t.Fatalf("dryRun=%v: changed %s", dryRun, path)
						}
					}
				}
			}
			if collision {
				return
			}
			if got, want := readTestFile(t, p, source), strings.ReplaceAll(body, "battle_helper", "attack_entry"); got != want {
				t.Fatalf("source = %q, want %q", got, want)
			}
			config, err = p.loadCheckedConfig()
			if err != nil {
				t.Fatal(err)
			}
			for _, id := range []string{"battle", "event-attack"} {
				module := config.byID[id]
				entry := module.function("attack_entry")
				if entry == nil || entry.Unit != "selection" || module.sourceOf(entry) != source {
					t.Fatalf("%s lost original unit binding: %+v", id, entry)
				}
			}
		})
	}
}
