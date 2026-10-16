package main

import (
	"strings"
	"testing"
)

func TestDeclCheckViewsListEachSourceOncePerFlagSet(t *testing.T) {
	_, config := unitCheckTestProject(t)
	views := declCheckViews(config)
	seen := map[string]int{}
	for _, view := range views {
		seen[view.source+" "+strings.Join(view.defines, " ")]++
		if !strings.HasSuffix(view.source, ".c") {
			t.Fatalf("non-C view %+v", view)
		}
	}
	for key, count := range seen {
		if count != 1 {
			t.Fatalf("view %q listed %d times", key, count)
		}
	}
	// unit_e.c compiles unit_e and the alias unit_f with the same flags: one view.
	count := 0
	for _, view := range views {
		if view.source == "src/battle/unit_e.c" {
			count++
		}
	}
	if count != 1 {
		t.Fatalf("unit_e.c views = %d", count)
	}
}
