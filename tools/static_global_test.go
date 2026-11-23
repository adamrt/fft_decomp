package main

import (
	"strings"
	"testing"
)

func TestStaticTypeSize(t *testing.T) {
	headers := "enum { UNIT_SLOT_COUNT = 21, ROWS = 0x4 };\ntypedef char thing_size_must_be_12[1];\n"
	cases := []struct {
		typeText, dimensions string
		count, element       int
	}{
		{"u32", "", 1, 4},
		{"volatile u16", "[8]", 8, 2},
		{"thing_t", "[UNIT_SLOT_COUNT]", 21, 12},
		{"s16", "[ROWS * 2 + 1]", 9, 2},
		{"u8*", "", 1, 4},
		{"mystery_t", "", 1, 0},
		{"u8", "[]", 0, 1},
	}
	for _, c := range cases {
		count, element := staticTypeSize(c.typeText, c.dimensions, headers, "")
		if count != c.count || element != c.element {
			t.Errorf("%s%s = %d x %d, want %d x %d", c.typeText, c.dimensions, count, element, c.count, c.element)
		}
	}
}

func TestStaticInitializer(t *testing.T) {
	cases := []struct {
		bytes                []byte
		typeText, dimensions string
		count, element       int
		want                 string
	}{
		{[]byte{0, 0, 0, 0}, "u32", "", 1, 4, " = 0"},
		{[]byte{0, 0, 0}, "rgb24_t", "", 1, 3, " = { 0 }"},
		{[]byte{0, 0, 0, 0}, "u16", "[2]", 2, 2, " = { 0 }"},
		{[]byte{1}, "u8", "", 1, 1, " = 1"},
		{[]byte{0xff, 0xff}, "s16", "", 1, 2, " = -1"},
		{[]byte{0x10, 0, 2, 0}, "u16", "[2]", 2, 2, " = { 0x10, 2 }"},
	}
	for _, c := range cases {
		got, err := staticInitializer(c.bytes, c.typeText, c.dimensions, c.count, c.element)
		if err != nil || got != c.want {
			t.Errorf("%s%s % x = %q (%v), want %q", c.typeText, c.dimensions, c.bytes, got, err, c.want)
		}
	}
	if _, err := staticInitializer([]byte{1, 2, 3}, "rgb24_t", "", 1, 3); err == nil {
		t.Error("a nonzero record got a generated initializer")
	}
}

func TestStaticExternDeclarationKeepsTrailingComment(t *testing.T) {
	p := validTestProject(t)
	writeSymbolRenameFixture(t, p.root, "src/main/main_first.c", "void main_first(void) {}\n\nextern u8 g_main_flags[4]; /* flags */\n")
	found, err := staticExternDeclarations(p.root, []string{"src/main/main_first.c"}, "g_main_flags")
	if err != nil || len(found) != 1 {
		t.Fatalf("declarations = %v, %v", found, err)
	}
	if d := found[0]; d.typeText != "u8" || d.dimensions != "[4]" || d.trailing != " /* flags */" {
		t.Fatalf("declaration = %+v", d)
	}
}

func TestStaticCodeBlanksComments(t *testing.T) {
	source := "u8 a; /* g_x is\n used */ u8 b; // g_y\nchar* s = \"/* g_z */\";\n"
	got := string(staticCode([]byte(source)))
	for _, name := range []string{"g_x", "g_y"} {
		if strings.Contains(got, name) {
			t.Errorf("comment mention %s survived: %q", name, got)
		}
	}
	if !strings.Contains(got, "g_z") || strings.Count(got, "\n") != 3 {
		t.Errorf("string literal or newlines changed: %q", got)
	}
}

func TestStaticPlace(t *testing.T) {
	data := []rodataSpec{{Addr: 0x1000, Size: 8}}
	bss := []rodataSpec{{Addr: 0x2000, Size: 4}}
	cases := []struct {
		name               string
		addr               uint32
		size, element      int
		zero               bool
		data, bss          []rodataSpec
		prevEnd            uint32
		prevData, nextData bool
		kind, anchor       string
		refused            bool
	}{
		{"append data", 0x1008, 4, 4, false, data, bss, 0, false, false, "data", "after", false},
		{"prepend data", 0x0ff8, 8, 4, true, data, bss, 0, false, true, "data", "before", false},
		{"prepend byte before a byte row", 0x0ffc, 1, 1, true, []rodataSpec{{Addr: 0x1000, Size: 1}}, bss, 0, false, true, "", "", true},
		{"prepend byte before a word row", 0x0ffc, 1, 1, true, data, bss, 0, false, true, "data", "before", false},
		{"first data after another unit", 0x0ff8, 4, 4, false, nil, bss, 0x0ff6, true, false, "data", "", false},
		{"first data, zero, data follows", 0x0ff8, 4, 4, true, nil, nil, 0x0ff8, true, true, "data", "", false},
		{"append unaligned byte to bss", 0x2004 + 1, 1, 1, true, data, []rodataSpec{{Addr: 0x2000, Size: 5}}, 0, false, false, "bss", "after", false},
		{"append halfword to bss", 0x2004, 2, 2, true, data, bss, 0, false, false, "bss", "after", false},
		{"prepend bss aligned", 0x1ffc, 4, 4, true, data, bss, 0, false, false, "bss", "before", false},
		{"prepend bss unaligned", 0x1ffe, 2, 2, true, data, bss, 0, false, false, "", "", true},
		{"first bss unaligned", 0x3001, 1, 1, true, data, nil, 0, false, false, "", "", true},
		{"first bss aligned", 0x3000, 1, 1, true, data, nil, 0, false, false, "bss", "", false},
		{"misaligned halfword append", 0x2005, 2, 2, true, data, []rodataSpec{{Addr: 0x2000, Size: 5}}, 0, false, false, "", "", true},
	}
	for _, c := range cases {
		got := staticPlace(c.addr, c.size, c.element, c.zero, c.data, c.bss, c.prevEnd, c.prevData, c.nextData)
		if c.refused != (got.reason != "") || (!c.refused && (got.kind != c.kind || got.anchor != c.anchor)) {
			t.Errorf("%s: got %+v", c.name, got)
		}
	}
}

func TestStaticDefinedBelow(t *testing.T) {
	text := "static u8 s_first;\n\ntypedef struct thing {\n    u8 a;\n} thing_t;\nenum { THING_COUNT = 4 };\n\nextern thing_t g_things[THING_COUNT];\n"
	start := strings.Index(text, "extern")
	declaration := &staticDeclaration{text: text, start: start, end: len(text), typeText: "thing_t", dimensions: "[THING_COUNT]"}
	if got := staticDefinedBelow(declaration, []string{"s_first"}, true); got != "thing_t" {
		t.Errorf("next to s_first: got %q, want thing_t", got)
	}
	if got := staticDefinedBelow(declaration, nil, true); got != "" {
		t.Errorf("in place of the extern: got %q, want none", got)
	}
}
