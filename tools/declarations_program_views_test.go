package main

import (
	"path/filepath"
	"strings"
	"testing"
)

var declarationProgramTestModules = []*moduleSpec{
	{ID: "main"}, {ID: "battle"}, {ID: "world"},
	{ID: "event-card"}, {ID: "event-jobstts"}, {ID: "event-bunit"},
	{ID: "effect-e000"}, {ID: "effect-e001"},
}

func TestDeclarationsSeparateProgramRepresentations(t *testing.T) {
	root := t.TempDir()
	header := `#ifndef CONTEXT_H
#define CONTEXT_H
typedef struct menu_context menu_context_t;
typedef struct status_context status_context_t;
typedef struct save_context save_context_t;
#if defined(FFT_EVENT_JOBSTTS)
extern struct status_context* g_context;
#elif defined(FFT_EVENT_CARD)
extern struct save_context* g_context;
#else
extern menu_context_t* g_context;
#endif
#endif
`
	writeTestFile(t, filepath.Join(root, "include/fft/context.h"), []byte(header))
	writeTestFile(t, filepath.Join(root, "src/shared.c"), []byte(`#ifdef FFT_EVENT_JOBSTTS
extern status_context_t* g_context;
#elif defined(FFT_EVENT_CARD)
extern save_context_t* g_context;
#else
extern menu_context_t* g_context;
#endif
`))
	if err := validateDeclarations(root, declarationProgramTestModules...); err != nil {
		t.Fatal(err)
	}
	groups, err := collectDeclarationGroups(root, declarationScanDirs, declarationProgramTestModules...)
	if err != nil || len(groups) != 1 {
		t.Fatalf("groups = %+v, err = %v", groups, err)
	}
	group := groups[0]
	if len(group.sites) != 6 || len(group.spellings) != 3 || group.minority != 0 {
		t.Fatalf("program-specific spellings must remain visible without a conflict: %+v", group)
	}
	for _, site := range group.sites {
		if site.line == 0 || site.path == "" {
			t.Fatalf("declaration diagnostic lost its location: %+v", site)
		}
	}
	// A wrong local extern must still disagree with the active header view.
	writeTestFile(t, filepath.Join(root, "src/shared.c"), []byte(`#ifdef FFT_EVENT_JOBSTTS
extern menu_context_t* g_context;
#endif
`))
	if err := validateDeclarations(root, declarationProgramTestModules...); err == nil || !strings.Contains(err.Error(), "g_context") {
		t.Fatalf("overlapping cross-file mismatch was hidden: %v", err)
	}
}

func TestDeclarationProgramConditionOverlap(t *testing.T) {
	for _, test := range []struct {
		name, source string
		conflict     bool
	}{
		{"separate programs", `#ifdef FFT_BATTLE
extern u8 g_value;
#endif
#ifdef FFT_WORLD
extern u16 g_value;
#endif
`, false},
		{"nested family", `#ifdef FFT_EVENT
#if defined(FFT_EVENT_CARD)
extern u8 g_value;
#elif defined(FFT_EVENT_JOBSTTS)
extern u16 g_value;
#else
extern u32 g_value;
#endif
#else
extern s32 g_value;
#endif
`, false},
		{"family complement", `#ifndef FFT_EVENT
extern u8 g_value;
#endif
#if defined(FFT_EVENT_CARD) || defined(FFT_EVENT_JOBSTTS)
extern u16 g_value;
#endif
`, false},
		{"effect family", `#if defined(FFT_EFFECT) && !defined(FFT_EFFECT_E000)
extern u8 g_value;
#endif
#ifdef FFT_EFFECT_E000
extern u16 g_value;
#endif
`, false},
		{"unknown outer guard", `#ifndef PRIVATE_H
#define PRIVATE_H
#ifdef FFT_WORLD
extern u8 g_value;
#else
extern u16 g_value;
#endif
#endif
`, false},
		{"commented directives", `/*
#ifdef FFT_WORLD
*/
extern u8 g_value;
/* #endif */
#ifdef FFT_BATTLE
extern u16 g_value;
#endif
`, true},
		{"same active branch", `#ifdef FFT_WORLD
extern u8 g_value;
extern u16 g_value;
#endif
`, true},
		{"overlapping union", `#if defined(FFT_WORLD) || defined(FFT_BATTLE)
extern u8 g_value;
#endif
#ifdef FFT_BATTLE
extern u16 g_value;
#endif
`, true},
		{"family overlaps child", `#ifdef FFT_EVENT
extern u8 g_value;
#endif
#ifdef FFT_EVENT_CARD
extern u16 g_value;
#endif
`, true},
		{"unmentioned overlay still possible", `#if defined(FFT_EVENT) && !defined(FFT_EVENT_CARD)
extern u8 g_value;
#endif
#ifdef FFT_EVENT
extern u16 g_value;
#endif
`, true},
		{"unguarded disagreement", `extern u8 g_value;
#ifdef FFT_WORLD
extern u16 g_value;
#endif
`, true},
		{"unknown condition", `#ifdef FEATURE
extern u8 g_value;
#else
extern u16 g_value;
#endif
`, true},
		{"mixed unknown condition", `#if defined(FFT_WORLD) && FEATURE
extern u8 g_value;
#else
extern u16 g_value;
#endif
`, true},
		{"unknown first alternative", `#if FEATURE
extern u8 g_value;
#elif defined(FFT_WORLD)
extern u16 g_value;
#endif
`, true},
		{"unsupported arithmetic", `#if FFT_WORLD + 1
extern u8 g_value;
#else
extern u16 g_value;
#endif
`, true},
		{"macro redefinition", `#define FFT_WORLD 1
#ifdef FFT_WORLD
extern u8 g_value;
#else
extern u16 g_value;
#endif
`, true},
		{"macro undefinition", `#undef FFT_WORLD
#ifdef FFT_WORLD
extern u8 g_value;
#else
extern u16 g_value;
#endif
`, true},
		{"unterminated conditional", `#ifdef FFT_WORLD
extern u8 g_value;
#else
extern u16 g_value;
`, true},
		{"repeated else", `#ifdef FFT_WORLD
extern u8 g_value;
#else
extern u16 g_value;
#else
#endif
`, true},
		{"continued directive", "#if defined(FFT_WORLD) \\\n || defined(FFT_BATTLE)\nextern u8 g_value;\n#else\nextern u16 g_value;\n#endif\n", true},
	} {
		t.Run(test.name, func(t *testing.T) {
			root := t.TempDir()
			writeTestFile(t, filepath.Join(root, "include/fft/value.h"), []byte(test.source))
			groups, err := collectDeclarationGroups(root, declarationScanDirs, declarationProgramTestModules...)
			if err != nil || len(groups) != 1 {
				t.Fatalf("groups = %+v, error = %v", groups, err)
			}
			if got := groups[0].conflicting(); got != test.conflict {
				t.Fatalf("conflicting = %v, want %v; sites = %+v", got, test.conflict, groups[0].sites)
			}
			if err := validateDeclarations(root, declarationProgramTestModules...); (err != nil) != test.conflict {
				t.Fatalf("validateDeclarations error = %v, want conflict %v", err, test.conflict)
			}
		})
	}
}
