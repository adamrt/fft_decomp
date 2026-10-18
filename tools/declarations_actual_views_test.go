package main

import (
	"path/filepath"
	"testing"
)

func TestDeclarationsUseConfiguredProgramViews(t *testing.T) {
	main := &moduleSpec{ID: "main"}
	bunit := &moduleSpec{ID: "event-bunit", Defines: []defineSpec{{Name: "VARIANT_WIDTH", Value: "1"}}}
	card := &moduleSpec{ID: "event-card", Defines: []defineSpec{{Name: "VARIANT_WIDTH", Value: "2"}}}
	for _, test := range []struct {
		name     string
		source   string
		modules  []*moduleSpec
		conflict bool
	}{
		{"no family-only program", `#if defined(FFT_EVENT) && !defined(FFT_EVENT_BUNIT)
extern u8 g_value;
#endif
#ifdef FFT_EVENT
extern u16 g_value;
#endif
`, []*moduleSpec{main, bunit}, false},
		{"no empty program", `#if !defined(FFT_MAIN) && !defined(FFT_EVENT_BUNIT)
extern u8 g_value;
#endif
extern u16 g_value;
`, []*moduleSpec{main, bunit}, false},
		{"real sibling overlaps", `#if defined(FFT_EVENT) && !defined(FFT_EVENT_BUNIT)
extern u8 g_value;
#endif
#ifdef FFT_EVENT
extern u16 g_value;
#endif
`, []*moduleSpec{main, bunit, card}, true},
		{"real child overlaps family", `#ifdef FFT_EVENT_BUNIT
extern u8 g_value;
#endif
#ifdef FFT_EVENT
extern u16 g_value;
#endif
`, []*moduleSpec{main, bunit}, true},
		{"variant values select separate declarations", `#if defined(FFT_EVENT) && VARIANT_WIDTH == 1
extern u8 g_value;
#endif
#if defined(FFT_EVENT) && VARIANT_WIDTH == 2
extern u16 g_value;
#endif
`, []*moduleSpec{main, bunit, card}, false},
		{"variant overlap remains conflict", `#if VARIANT_WIDTH == 1
extern u8 g_value;
#endif
#ifdef FFT_EVENT_BUNIT
extern u16 g_value;
#endif
`, []*moduleSpec{main, bunit, card}, true},
		{"unknown condition stays conservative", `#ifdef FEATURE
extern u8 g_value;
#else
extern u16 g_value;
#endif
`, []*moduleSpec{main, bunit}, true},
		{"unknown build name stays conservative", `#ifdef FFT_NOT_CONFIGURED
extern u8 g_value;
#else
extern u16 g_value;
#endif
`, []*moduleSpec{main, bunit}, true},
		{"unknown variant stays conservative", `#if VARIANT_NOT_CONFIGURED == 1
extern u8 g_value;
#else
extern u16 g_value;
#endif
`, []*moduleSpec{main, bunit}, true},
		{"redefined variant stays conservative", `#define VARIANT_WIDTH 1
#if VARIANT_WIDTH == 1
extern u8 g_value;
#else
extern u16 g_value;
#endif
`, []*moduleSpec{main, bunit}, true},
		{"missing configuration stays conservative", `#ifdef FFT_WORLD
extern u8 g_value;
#endif
#ifdef FFT_BATTLE
extern u16 g_value;
#endif
`, nil, true},
	} {
		t.Run(test.name, func(t *testing.T) {
			root := t.TempDir()
			writeTestFile(t, filepath.Join(root, "src", "value.c"), []byte(test.source))
			groups, err := collectDeclarationGroups(root, declarationScanDirs, test.modules...)
			if err != nil || len(groups) != 1 {
				t.Fatalf("groups = %+v, error = %v", groups, err)
			}
			if got := groups[0].conflicting(); got != test.conflict {
				t.Fatalf("conflicting = %v, want %v; sites = %+v", got, test.conflict, groups[0].sites)
			}
			if err := validateDeclarations(root, test.modules...); (err != nil) != test.conflict {
				t.Fatalf("validateDeclarations error = %v, want conflict %v", err, test.conflict)
			}
		})
	}
}
