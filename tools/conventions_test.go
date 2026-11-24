package main

import (
	"strings"
	"testing"
)

func conventionProblems(t *testing.T, files map[string]string, check func(string) ([]string, error)) string {
	t.Helper()
	root := t.TempDir()
	for path, body := range files {
		writeSymbolRenameFixture(t, root, path, body)
	}
	problems, err := check(root)
	if err != nil {
		t.Fatal(err)
	}
	return strings.Join(problems, "\n")
}

func TestHeaderLayersAllowOwnAndLowerLayers(t *testing.T) {
	got := conventionProblems(t, map[string]string{
		"include/fft/game/item.h":     "#include \"fft/sys/memory.h\"\n#include \"fft/menu/menu.h\"\n#include \"fft/game/job.h\"\n#include \"psx/types.h\"\n",
		"include/fft/menu/menu.h":     "#include \"fft/gfx/gfx.h\"\n#include \"fft/text/text.h\"\n",
		"include/fft/sys/memory.h":    "#include \"fft/gfx/gfx.h\"\n",
		"include/fft/party/shop.h":    "#include \"fft/world/world_map.h\"\n#include \"fft/battle/battle.h\"\n",
		"include/fft/game/job.h":      "#include \"fft/battle/battle.h\"\n",
		"include/fft/extra/odd.h":     "",
		"include/fft/map/map_mesh.h":  "#include \"fft/odd/x.h\"\n",
		"src/battle/includes_above.c": "#include \"fft/battle/battle.h\"\n",
	}, checkHeaderLayers)
	for _, want := range []string{
		"include/fft/sys/memory.h:1: sys (layer 0) includes fft/gfx/gfx.h (layer 1)",
		"include/fft/game/job.h:1: game (layer 2) includes fft/battle/battle.h (layer 3)",
		"include/fft/extra/odd.h: header is outside the layer folders",
		"include/fft/map/map_mesh.h:1: includes odd from no known layer",
	} {
		if !strings.Contains(got, want) {
			t.Errorf("missing %q in:\n%s", want, got)
		}
	}
	for _, unwanted := range []string{"item.h", "menu.h", "shop.h", "includes_above"} {
		if strings.Contains(got, unwanted) {
			t.Errorf("reported an allowed include (%s):\n%s", unwanted, got)
		}
	}
}

func TestStorageNamesFollowStorage(t *testing.T) {
	got := conventionProblems(t, map[string]string{
		"src/battle/names.c": "static int s_count;\nstatic int counter;\nint g_total;\nint total = 1;\nextern int elsewhere;\n" +
			"static void s_helper(void) {}\nstatic void g_helper(void) {}\nstatic void helper(void) { static int local; }\n" +
			"void public_function(void) {}\nregister u32 stack_pointer __asm__(\"$29\");\nint g_aliased __asm__(\"other\");\n" +
			"#ifdef FFT_BATTLE\nstatic int also_bad;\n#endif\n",
		"src/platform/psyq/libc/memset.c": "static int counter;\nint errno;\n",
		"src/lib/suzuki/driver.c":         "int suzuki_channel_count;\n",
	}, checkStorageNames)
	want := []string{
		"src/battle/names.c:13: also_bad: static variable must start with s_",
		"src/battle/names.c:2: counter: static variable must start with s_",
		"src/battle/names.c:4: total: global variable must start with g_",
		"src/battle/names.c:6: s_helper: static function must not start with s_ or g_",
		"src/battle/names.c:7: g_helper: static function must not start with s_ or g_",
	}
	lines := strings.Split(got, "\n")
	if len(lines) != len(want) {
		t.Fatalf("problems:\n%s", got)
	}
	for _, line := range want {
		if !strings.Contains(got, line) {
			t.Errorf("missing %q in:\n%s", line, got)
		}
	}
}

func TestSizeAssertsNameAndFollowTheirType(t *testing.T) {
	got := conventionProblems(t, map[string]string{
		"include/fft/game/good.h": "typedef struct item { u8 id; } item_t;\n" +
			"typedef char item_size_must_be_1[sizeof(item_t) == 1 ? 1 : -1];\n" +
			"/* A size code. */\ntypedef struct sprite_size { u8 w; } sprite_size_t;\n" +
			"typedef char sprite_size_must_be_1[(sizeof(sprite_size_t) == 1) ? 1 : -1];\n" +
			"typedef enum result { RESULT_OK } result_e;\ntypedef char result_size_must_be_4[sizeof(result_e) == 4 ? 1 : -1];\n" +
			"struct map_unit {\n    u8 x;\n};\ntypedef char map_unit_size_must_be_1[sizeof(map_unit_t) == 1 ? 1 : -1];\n" +
			"typedef struct { u8 b[4]; } record_t;\ntypedef char record_size_must_be_4[sizeof(record_t) == RECORD_BYTES ? 1 : -1];\n" +
			"void function(void) {}\n",
		"include/psx/libgs.h": "typedef struct { u8 v[4]; } GsOT_TAG;\ntypedef char gs_ot_tag_size_must_be_4[(sizeof(GsOT_TAG) == 4) ? 1 : -1];\n",
		"src/battle/bad.c": "typedef struct { u8 a; } first_t;\ntypedef struct { u8 b; } second_t;\n" +
			"typedef char first_size_must_be_1[sizeof(first_t) == 1 ? 1 : -1];\n" +
			"typedef char wrong_size_must_be_1[sizeof(second_t) == 1 ? 1 : -1];\n" +
			"typedef struct { u8 c[2]; } third_t;\ntypedef char third_size_must_be_3[sizeof(third_t) == 2 ? 1 : -1];\n" +
			"typedef struct { u8 d; } fourth_t;\ntypedef char assert_fourth_size[sizeof(fourth_t) == 1 ? 1 : -1];\n",
		"src/lib/suzuki/driver.c": "typedef char storage_size_must_be_4[sizeof(g_table) == 4 ? 1 : -1];\n",
	}, checkSizeAsserts)
	want := []string{
		"src/battle/bad.c:3: first_size_must_be_1 does not directly follow the declaration of first_t",
		"src/battle/bad.c:4: wrong_size_must_be_1 checks second_t; name it second_size_must_be_1",
		"src/battle/bad.c:4: wrong_size_must_be_1 does not directly follow the declaration of second_t",
		"src/battle/bad.c:6: third_size_must_be_3 asserts size 2",
		"src/battle/bad.c:8: assert_fourth_size is not named X_size_must_be_N",
	}
	lines := strings.Split(got, "\n")
	if len(lines) != len(want) {
		t.Fatalf("problems:\n%s", got)
	}
	for _, line := range want {
		if !strings.Contains(got, line) {
			t.Errorf("missing %q in:\n%s", line, got)
		}
	}
}
