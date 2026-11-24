package main

import (
	"bytes"
	"reflect"
	"strings"
	"testing"
)

func TestNamingReportsOwnersOutsideTheirHeader(t *testing.T) {
	root := t.TempDir()
	for path, body := range map[string]string{
		"include/fft/game/item.h": "void item_get_name(void);\nint item;\nvoid equipment_sort(void);\nvoid equipment_clear(void);\n" +
			"static void helper(void) {}\nvoid world_entrypoint(void);\n#ifdef FFT_WORLD\nvoid equipment_sort(void);\n#endif\n",
		"include/fft/menu/menu_window.h": "void menu_open_window(void);\nvoid window_close(void);\n",
		"include/fft/world/menu_stack.h": "void menu_stack_push(void);\nvoid menu_draw(void);\n",
		"include/fft/world/bar.h":        "void bar_open(void);\nvoid rumor_read(void);\n",
		"include/fft/sys/memory.h":       "void heap_alloc(void);\nvoid mem_copy(void);\nvoid rumor_read(void);\n",
		"include/fft/gfx/gfx_sprite.h":   "void sprite_draw(void);\nvoid fade_in(void);\n",
	} {
		writeSymbolRenameFixture(t, root, path, body)
	}
	mismatches, err := namingMismatches(root)
	if err != nil {
		t.Fatal(err)
	}
	got := map[string][]string{}
	for _, mismatch := range mismatches {
		got[mismatch.header] = mismatch.functions
	}
	want := map[string][]string{
		"include/fft/game/item.h":        {"equipment_clear", "equipment_sort"},
		"include/fft/menu/menu_window.h": {"window_close"},
		"include/fft/world/menu_stack.h": {"menu_draw"},
		"include/fft/sys/memory.h":       {"rumor_read"},
	}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("mismatches = %v, want %v", got, want)
	}
	var out bytes.Buffer
	writeNamingReport(&out, mismatches)
	if text := out.String(); !strings.HasPrefix(text, "naming: 5 public functions in 4 headers") ||
		!strings.Contains(text, "include/fft/game/item.h") || !strings.Contains(text, "equipment_clear equipment_sort") {
		t.Fatalf("report:\n%s", text)
	}
}
