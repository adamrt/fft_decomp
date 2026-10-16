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
	if flags := strings.Join(battle.defineFlags(), " "); flags != "-DFFT_BATTLE=1 -DVARIANT_B=0x10 -DVARIANT_A=16" && flags != "-DFFT_BATTLE=1 -DVARIANT_A=16 -DVARIANT_B=0x10" {
		t.Fatalf("flags = %q", flags)
	}
	rendered := string(renderModuleConfig(battle))
	if !strings.Contains(rendered, "\ndefines: {VARIANT_A: 16, VARIANT_B: 0x10}\n") {
		t.Fatalf("rendered:\n%s", rendered)
	}
	if flags := strings.Join(battle.defineFlags(), " "); flags != "-DFFT_BATTLE=1 -DVARIANT_A=16 -DVARIANT_B=0x10" {
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
			want:  "come only from the build",
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

func TestModuleMacrosAndConditionalRules(t *testing.T) {
	for id, want := range map[string][]string{"battle": {"FFT_BATTLE"}, "event-bunit": {"FFT_EVENT_BUNIT", "FFT_EVENT"}, "effect-e000": {"FFT_EFFECT_E000", "FFT_EFFECT"}} {
		m := &moduleSpec{ID: id}
		if got := strings.Join(m.macroNames(), " "); got != strings.Join(want, " ") {
			t.Fatalf("%s macros = %q, want %q", id, got, want)
		}
	}
	m := &moduleSpec{ID: "world", Defines: []defineSpec{{Name: "VARIANT_X", Value: "17"}}}
	if macros := m.macros(); macros["FFT_WORLD"] != "1" || macros["VARIANT_X"] != "17" || len(macros) != 2 {
		t.Fatalf("macros = %v", macros)
	}
	known := map[string]bool{"FFT_BATTLE": true, "FFT_WORLD": true}
	source := "#ifdef FFT_BATTLE\nint a;\n#elif defined(FFT_WORLD) || VARIANT_X > 1\nint b;\n#else\nint c;\n#endif\n#if DEBUG\n#endif\n#ifdef FFT_NOWHERE\n#endif\n"
	problems := conditionalProblems("src/x.c", []byte(source), known, false)
	if len(problems) != 2 || !strings.Contains(problems[0], "tests DEBUG") || !strings.Contains(problems[1], "FFT_NOWHERE, which names no module") {
		t.Fatalf("source problems = %v", problems)
	}
	header := "#ifndef X_H\n#define X_H\n#ifdef FFT_BATTLE\n#endif\n#if VARIANT_X\n#endif\n#endif\n"
	problems = conditionalProblems("include/x.h", []byte(header), known, true)
	if len(problems) != 1 || !strings.Contains(problems[0], "tests VARIANT_X; headers may not") {
		t.Fatalf("header problems = %v", problems)
	}
	if match := preprocessorDefine.FindSubmatch([]byte("#define FFT_BATTLE 1\n")); match == nil {
		t.Fatal("a source defining a program macro must be caught")
	}
}
