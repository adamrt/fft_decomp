package main

import (
	"strings"
	"testing"
)

func TestActiveConditionalTextResolvesProgramBlocks(t *testing.T) {
	source := strings.Join([]string{
		"#include \"a.h\"",
		"int shared(void) { return 1; }",
		"#ifdef FFT_BATTLE",
		"int only_battle(void) { return 2; }",
		"#endif",
		"#if defined(FFT_WORLD) || defined(FFT_EVENT_BUNIT)",
		"int world_or_bunit(void) { return 3; }",
		"#elif VARIANT_N == 16 && !defined(FFT_MAIN)",
		"int sixteen(void) { return 4; }",
		"#else",
		"int other(void) { return 5; }",
		"#endif",
		"#ifndef FFT_BATTLE",
		"int not_battle(void) { return 6; }",
		"#endif",
	}, "\n")
	cases := map[string]struct {
		macros map[string]string
		want   []string
		absent []string
	}{
		"battle":  {map[string]string{"FFT_BATTLE": "1", "VARIANT_N": "16"}, []string{"shared", "only_battle", "sixteen"}, []string{"world_or_bunit", "other", "not_battle"}},
		"world":   {map[string]string{"FFT_WORLD": "1", "VARIANT_N": "17"}, []string{"shared", "world_or_bunit", "not_battle"}, []string{"only_battle", "sixteen", "other"}},
		"main":    {map[string]string{"FFT_MAIN": "1", "VARIANT_N": "16"}, []string{"shared", "other", "not_battle"}, []string{"only_battle", "world_or_bunit", "sixteen"}},
		"nothing": {map[string]string{}, []string{"shared", "other", "not_battle"}, []string{"only_battle", "sixteen"}},
	}
	for name, test := range cases {
		active, err := activeConditionalText(source, test.macros)
		if err != nil {
			t.Fatalf("%s: %v", name, err)
		}
		if strings.Count(active, "\n") != strings.Count(source, "\n") || !strings.HasPrefix(active, "#include \"a.h\"") {
			t.Fatalf("%s: line count or include changed:\n%s", name, active)
		}
		for _, symbol := range test.want {
			if !strings.Contains(active, "int "+symbol+"(") {
				t.Fatalf("%s: %s missing:\n%s", name, symbol, active)
			}
		}
		for _, symbol := range test.absent {
			if strings.Contains(active, "int "+symbol+"(") {
				t.Fatalf("%s: %s present:\n%s", name, symbol, active)
			}
		}
	}
	for _, bad := range []string{"#endif\n", "#ifdef A\n", "#else\n#endif\n", "#if (1\n#endif\n", "#ifdef\n#endif\n"} {
		if _, err := activeConditionalText(bad, nil); err == nil {
			t.Fatalf("%q should fail", bad)
		}
	}
	if names := conditionalIdentifiers("#if defined(FFT_A) || VARIANT_B > 1 /* c */"); strings.Join(names, " ") != "FFT_A VARIANT_B" {
		t.Fatalf("identifiers = %v", names)
	}
	if names := conditionalIdentifiers("#  ifndef   X_H"); strings.Join(names, " ") != "X_H" {
		t.Fatalf("identifiers = %v", names)
	}
}
