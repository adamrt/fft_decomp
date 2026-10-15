package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestDefinesRenderCanonicallyAndReachCompileFlags(t *testing.T) {
	p := validTestProject(t)
	edited := strings.Replace(testBattleYAML, "profile: gcc-2.6.3_O2_aspsx-2.34\n", "profile: gcc-2.6.3_O2_aspsx-2.34\ndefines: {VARIANT_B: 0x10, VARIANT_A: 16}\n", 1)
	if err := os.WriteFile(filepath.Join(p.root, "target", "battle.yaml"), []byte(edited), 0o644); err != nil {
		t.Fatal(err)
	}
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	battle, _ := config.module("battle")
	if flags := strings.Join(battle.defineFlags(), " "); flags != "-DVARIANT_B=0x10 -DVARIANT_A=16" && flags != "-DVARIANT_A=16 -DVARIANT_B=0x10" {
		t.Fatalf("flags = %q", flags)
	}
	rendered := string(renderModuleConfig(battle))
	if !strings.Contains(rendered, "\ndefines: {VARIANT_A: 16, VARIANT_B: 0x10}\n") {
		t.Fatalf("rendered:\n%s", rendered)
	}
	if flags := strings.Join(battle.defineFlags(), " "); flags != "-DVARIANT_A=16 -DVARIANT_B=0x10" {
		t.Fatalf("normalized flags = %q", flags)
	}
}

func TestValidateDefinesEnforcesTheVariantContract(t *testing.T) {
	for name, test := range map[string]struct {
		yaml   string
		files  map[string]string
		want   string
		accept bool
	}{
		"used define passes": {
			yaml:   "defines: {VARIANT_PAGES: 53}\n",
			files:  map[string]string{"src/battle/battle_helper.c": "void battle_helper(void) { int n = VARIANT_PAGES; }\n"},
			accept: true,
		},
		"key without prefix": {
			yaml:  "defines: {PAGES: 53}\n",
			files: map[string]string{"src/battle/battle_helper.c": "void battle_helper(void) { int n = PAGES; }\n"},
			want:  "must be a unique VARIANT_ name",
		},
		"non-integer value": {
			yaml:  "defines: {VARIANT_PAGES: many}\n",
			files: map[string]string{"src/battle/battle_helper.c": "void battle_helper(void) { int n = VARIANT_PAGES; }\n"},
			want:  "non-integer value",
		},
		"unused define": {
			yaml: "defines: {VARIANT_PAGES: 53}\n",
			want: "used by no source",
		},
		"use without define": {
			files: map[string]string{"src/battle/battle_helper.c": "void battle_helper(void) { int n = VARIANT_PAGES; }\n"},
			want:  "which this module does not define",
		},
		"source defines the macro": {
			files: map[string]string{"src/battle/battle_helper.c": "#define VARIANT_PAGES 53\nvoid battle_helper(void) { int n = VARIANT_PAGES; }\n"},
			want:  "come only from target defines",
		},
		"header uses the macro": {
			yaml:  "defines: {VARIANT_PAGES: 53}\n",
			files: map[string]string{"src/battle/battle_helper.c": "void battle_helper(void) { int n = VARIANT_PAGES; }\n", "include/fft/menu.h": "extern unsigned char g_map[VARIANT_PAGES];\n"},
			want:  "headers may not use VARIANT_ names",
		},
		"comment mention is not a use": {
			files:  map[string]string{"src/battle/battle_helper.c": "/* VARIANT_PAGES differs */\nvoid battle_helper(void) {}\n"},
			accept: true,
		},
	} {
		t.Run(name, func(t *testing.T) {
			p := validTestProject(t)
			yaml := strings.Replace(testBattleYAML, "profile: gcc-2.6.3_O2_aspsx-2.34\n", "profile: gcc-2.6.3_O2_aspsx-2.34\n"+test.yaml, 1)
			if err := os.WriteFile(filepath.Join(p.root, "target", "battle.yaml"), []byte(yaml), 0o644); err != nil {
				t.Fatal(err)
			}
			for path, content := range test.files {
				full := filepath.Join(p.root, filepath.FromSlash(path))
				if err := os.MkdirAll(filepath.Dir(full), 0o755); err != nil {
					t.Fatal(err)
				}
				if err := os.WriteFile(full, []byte(content), 0o644); err != nil {
					t.Fatal(err)
				}
			}
			config, err := p.loadProjectConfig()
			if err != nil {
				t.Fatal(err)
			}
			problems := validateDefines(p.root, config)
			if test.accept {
				if len(problems) != 0 {
					t.Fatalf("problems = %v", problems)
				}
				return
			}
			if !strings.Contains(strings.Join(problems, "\n"), test.want) {
				t.Fatalf("problems = %v, want %q", problems, test.want)
			}
		})
	}
}

func TestValidateVariantNamesRequiresTheSuffixForPerModuleBodies(t *testing.T) {
	p := validTestProject(t)
	// battle_helper gains a second body under src/event through the card entry.
	event := strings.Replace(testEventYAML, "name: card_entry, hash: 00000000000000bb}", "name: card_entry, hash: 00000000000000bb, source: src/event/battle_helper.c, source_symbol: battle_helper}", 1)
	if err := os.WriteFile(filepath.Join(p.root, "target", "event.yaml"), []byte(event), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(p.root, "src", "event", "battle_helper.c"), []byte("void battle_helper(void) {}\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	problems := validateVariantNames(config)
	if len(problems) != 1 || !strings.Contains(problems[0], "battle_helper has a body under src/battle and src/event") {
		t.Fatalf("problems = %v", problems)
	}
	// A lone _variant body is the opposite mistake.
	main := strings.Replace(testMainYAML, "name: main_second,", "name: main_second_variant,", 1)
	if err := os.WriteFile(filepath.Join(p.root, "target", "main.yaml"), []byte(main), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.Rename(filepath.Join(p.root, "src", "main", "main_second.c"), filepath.Join(p.root, "src", "main", "main_second_variant.c")); err != nil {
		t.Fatal(err)
	}
	if config, err = p.loadProjectConfig(); err != nil {
		t.Fatal(err)
	}
	problems = validateVariantNames(config)
	if len(problems) != 2 || !strings.Contains(problems[1], "main_second_variant has one body (src/main)") {
		t.Fatalf("problems = %v", problems)
	}
	// Shared and library trees never count as module folders.
	for source, want := range map[string]string{"src/shared/menu/x.c": "src/shared/menu", "src/lib/psyq/libgpu/x.c": "", "src/battle/x.c": "src/battle", "src/event/x.c": "src/event"} {
		if got := moduleSourceDir(source); got != want {
			t.Errorf("moduleSourceDir(%s) = %q, want %q", source, got, want)
		}
	}
}
