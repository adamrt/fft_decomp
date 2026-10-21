package main

import "testing"

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
