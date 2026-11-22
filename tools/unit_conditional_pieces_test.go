package main

import (
	"strings"
	"testing"
)

func pieceOwners(battle, world []string) []unitPieceOwner {
	return []unitPieceOwner{
		{id: "battle", macros: map[string]string{"FFT_BATTLE": "1", "VARIANT_SLOTS": "16"}, members: battle, listed: true},
		{id: "world", macros: map[string]string{"FFT_WORLD": "1", "VARIANT_SLOTS": "17"}, members: world, listed: true},
	}
}

func TestCutConditionalUnitResolvesOnlyBlocksThatWrapMembers(t *testing.T) {
	source := `/* Shared menu code. */

#include "fft/menu.h"

static int s_count;
static int later(
    int value);

int first(void) {
#ifdef FFT_BATTLE
    s_count++;
#endif
    return s_count;
}

/* Only BATTLE walks units. */
#ifdef FFT_BATTLE
static int s_walks;
int walk(void) { return s_walks; }
#endif

#ifdef FFT_WORLD
/* WORLD keeps a wider table. */
extern int g_table[];
#endif
int middle(void) { return 1; }

#if VARIANT_SLOTS == 16
int last(void) { return 16; }
#else
int last(void) { return g_table[0]; }
#endif

static int later(
    int value) {
    return value;
}
`
	cut, err := cutConditionalUnit(source, pieceOwners([]string{"first", "walk", "middle", "last", "later"}, []string{"first", "middle", "last", "later"}))
	if err != nil {
		t.Fatal(err)
	}
	if len(cut.includes) != 1 || cut.includes[0] != `#include "fft/menu.h"` || len(cut.preludes) != 0 {
		t.Fatalf("includes = %v, preludes = %v", cut.includes, cut.preludes)
	}
	// A conditional inside a body stays; the prototype of later is not its
	// definition.
	first := cut.variants["first"]
	if len(first) != 1 || !first[0].modules["battle"] || !first[0].modules["world"] ||
		!strings.HasPrefix(first[0].text, "static int s_count;\nstatic int later(\n    int value);") || strings.Count(first[0].text, "#ifdef FFT_BATTLE") != 1 {
		t.Fatalf("first = %+v", first)
	}
	walk := cut.variants["walk"]
	if len(walk) != 1 || !walk[0].modules["battle"] || walk[0].modules["world"] ||
		walk[0].text != "/* Only BATTLE walks units. */\nstatic int s_walks;\nint walk(void) { return s_walks; }" {
		t.Fatalf("walk = %+v", walk)
	}
	// The directives of the block that selects a body are gone.
	last := cut.variants["last"]
	if len(last) != 2 || !last[0].modules["battle"] || last[0].modules["world"] || !last[1].modules["world"] || last[1].modules["battle"] ||
		last[0].text != "int last(void) { return 16; }" || last[1].text != "int last(void) { return g_table[0]; }" {
		t.Fatalf("last = %+v", last)
	}
	// A declaration-only block stays as written, with the member after it.
	middle := cut.variants["middle"]
	if len(middle) != 1 || len(middle[0].modules) != 2 ||
		middle[0].text != "#ifdef FFT_WORLD\n/* WORLD keeps a wider table. */\nextern int g_table[];\n#endif\nint middle(void) { return 1; }" {
		t.Fatalf("middle = %+v", middle)
	}
	if later := cut.variants["later"]; len(later) != 1 || !strings.HasPrefix(later[0].text, "static int later(\n    int value) {") {
		t.Fatalf("later = %+v", later)
	}
}

func TestCutConditionalUnitKeepsTextOtherModulesCompileAsPreludes(t *testing.T) {
	source := `extern int g_shared;

/* BATTLE walks units. */
#ifdef FFT_BATTLE
int walk(void) { return g_shared; }
static int s_tail;
#endif
int last(void) { return 0; }
`
	cut, err := cutConditionalUnit(source, pieceOwners([]string{"walk", "last"}, []string{"last"}))
	if err != nil {
		t.Fatal(err)
	}
	// The declaration every module compiles precedes a member only BATTLE has.
	walk := cut.preludes["walk"]
	if len(walk) != 1 || walk[0].text != "extern int g_shared;" || len(walk[0].modules) != 2 {
		t.Fatalf("walk preludes = %+v", walk)
	}
	if text := cut.variants["walk"][0].text; text != "/* BATTLE walks units. */\nint walk(void) { return g_shared; }" {
		t.Fatalf("walk = %q", text)
	}
	// A declaration left inside the block follows walk for BATTLE alone.
	last := cut.preludes["last"]
	if len(last) != 1 || last[0].text != "static int s_tail;" || !last[0].modules["battle"] || last[0].modules["world"] {
		t.Fatalf("last preludes = %+v", last)
	}
	// Cut for WORLD alone, the BATTLE member and its trailing text drop out
	// and the shared declaration moves to the next member.
	owners := pieceOwners([]string{"walk", "last"}, []string{"last"})
	owners[0].listed = false
	alone, err := cutConditionalUnit(source, owners)
	if err != nil {
		t.Fatal(err)
	}
	if len(alone.variants["walk"]) != 0 || len(alone.preludes) != 0 ||
		alone.variants["last"][0].text != "extern int g_shared;\nint last(void) { return 0; }" || alone.variants["last"][0].modules["battle"] {
		t.Fatalf("world alone = %+v / %+v", alone.variants, alone.preludes)
	}
}

func TestCutConditionalUnitRefusesWhatItCannotPlace(t *testing.T) {
	for name, test := range map[string]struct{ source, want string }{
		"members the unit does not list": {
			"int walk(void) { return 0; }\nint last(void) { return 0; }\n",
			"world compiles [walk last]",
		},
		"member under an ordinary guard": {
			"#ifdef DEBUG\nint walk(void) { return 0; }\n#endif\nint last(void) { return 0; }\n",
			"tests no program macro",
		},
		"trailing text": {
			"#ifdef FFT_BATTLE\nint walk(void) { return 0; }\n#endif\nint last(void) { return 0; }\nstatic int s_tail;\n",
			"text after the last member",
		},
		"text between two bodies of one member": {
			"#ifdef FFT_BATTLE\nint last(void) { return 0; }\nint walk(void) { return 0; }\n#endif\nextern int g_x;\n#ifdef FFT_WORLD\nint last(void) { return 1; }\n#endif\n",
			"between two definitions of last",
		},
	} {
		_, err := cutConditionalUnit(test.source, pieceOwners([]string{"walk", "last"}, []string{"last"}))
		if err == nil || !strings.Contains(err.Error(), test.want) {
			t.Errorf("%s: error = %v, want %q", name, err, test.want)
		}
	}
}

func TestProgramGuardedNamesTheSmallerSide(t *testing.T) {
	var order []*moduleSpec
	for _, id := range []string{"battle", "world", "event-bunit", "event-equip", "event-card"} {
		order = append(order, &moduleSpec{ID: id})
	}
	for _, test := range []struct {
		modules []*moduleSpec
		want    string
	}{
		{order, "text"},
		{order[:1], "#ifdef FFT_BATTLE\ntext\n#endif"},
		{order[:2], "#if defined(FFT_BATTLE) || defined(FFT_WORLD)\ntext\n#endif"},
		{order[:3], "#if !defined(FFT_EVENT_EQUIP) && !defined(FFT_EVENT_CARD)\ntext\n#endif"},
		{order[1:], "#ifndef FFT_BATTLE\ntext\n#endif"},
	} {
		if got := programGuarded("text", test.modules, order); got != test.want {
			t.Errorf("%d modules: %q, want %q", len(test.modules), got, test.want)
		}
	}
}
