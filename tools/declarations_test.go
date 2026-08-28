package main

import (
	"path/filepath"
	"reflect"
	"strings"
	"testing"
)

func scanOne(t *testing.T, source string) []scannedDeclaration {
	t.Helper()
	declarations, _, err := scanFileScopeDeclarations(source)
	if err != nil {
		t.Fatalf("scan %q: %v", source, err)
	}
	return declarations
}

func TestDeclarationSignatureRendering(t *testing.T) {
	for _, test := range []struct{ source, symbol, want string }{
		{"extern u32 g_flags;", "g_flags", "u32 @"},
		{"extern battle_stats_t* g_unit;", "g_unit", "battle_stats_t * @"},
		{"extern u8 D_800BF790[32];", "D_800BF790", "u8 @ []"},
		{"extern u8 D_800BF790[4][32];", "D_800BF790", "u8 @ [] []"},
		{"s32 battle_check(battle_stats_t* unit);", "battle_check", "s32 @ (battle_stats_t *)"},
		{"void battle_end(void) { register s32 i asm(\"$17\"); i = 0; }", "battle_end", "void @ (void)"},
		{"s32 func_801342CC();", "func_801342CC", "s32 @ (...)"},
		{"extern s32 (*g_cb)(s32);", "g_cb", "s32 ( * @ ) (s32)"},
		{"extern s32 (*g_threads)[256];", "g_threads", "s32 ( * @ ) []"},
		{"extern u8* (*g_handlers[])(u8*);", "g_handlers", "u8 * ( * @ [] ) (u8 *)"},
		{"extern const volatile unsigned int g_q;", "g_q", "u32 @"},
		{"extern int CdSync(int, unsigned char*);", "CdSync", "s32 @ (s32, u8 *)"},
		{"void f(u8 buffer[4], s32 (*callback)(void));", "f", "void @ (u8 *, s32 ( * ) (void))"},
		{"func_80012345();", "func_80012345", "s32 @ (...)"},
		{"extern u32 g_main_game_options_raw __asm__(\"g_main_game_options\");", "g_main_game_options", "u32 @"},
		{"s32 g_table[] = { 1, 2, 3 };", "g_table", "s32 @ []"},
	} {
		declarations := scanOne(t, test.source)
		if len(declarations) != 1 {
			t.Errorf("%q: got %d declarations, want 1", test.source, len(declarations))
			continue
		}
		got := declarations[0]
		if got.symbol != test.symbol || strings.Join(got.signature, " ") != test.want {
			t.Errorf("%q: got %s %q, want %s %q", test.source, got.symbol, strings.Join(got.signature, " "), test.symbol, test.want)
		}
	}
}

func TestDeclarationScanSkipsNonBindingStatements(t *testing.T) {
	source := `#include "fft/battle.h"
typedef struct battle_stats { u8 id; } battle_stats_t;
static s32 g_local;
struct loose { s32 a; };
enum { A, B };
DEFINE_PRIMITIVE_SETTER(SetTile16, 2, 0x78)
extern u8 g_a, *g_b, g_c[2];
void body(void) {
    register s32 i asm("$17");
    extern u16 g_inner;
}
`
	var got []string
	for _, declaration := range scanOne(t, source) {
		got = append(got, declaration.symbol+"="+strings.Join(declaration.signature, " "))
	}
	want := []string{"g_a=u8 @", "g_b=u8 * @", "g_c=u8 @ []", "body=void @ (void)"}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("got %q, want %q", got, want)
	}
}

func TestDeclarationTagAliasFolding(t *testing.T) {
	root := t.TempDir()
	// The alias repeats the tag, so `struct event_instr_t*` and
	// `event_instr_t*` must compare equal.
	writeTestFile(t, filepath.Join(root, "include", "fft", "event.h"), []byte("typedef struct event_instr_t { s32 op; } event_instr_t;\nextern event_instr_t* g_instr;\n"))
	writeTestFile(t, filepath.Join(root, "src", "battle", "a.c"), []byte("extern struct event_instr_t* g_instr;\n"))
	groups, err := collectDeclarationGroups(root, declarationScanDirs)
	if err != nil {
		t.Fatal(err)
	}
	for _, group := range groups {
		if group.symbol == "g_instr" && group.conflicting() {
			t.Fatalf("struct tag and its identically named typedef should agree, got %v", group.spellings)
		}
	}
}

func TestDeclarationAgreement(t *testing.T) {
	split := func(text string) []string {
		declarations, _, err := scanFileScopeDeclarations(text)
		if err != nil || len(declarations) != 1 {
			t.Fatalf("scan %q: %v", text, err)
		}
		return declarations[0].signature
	}
	unprototyped := split("s32 f();")
	if !signaturesAgree(unprototyped, split("s32 f(battle_stats_t* unit);")) {
		t.Error("unprototyped declaration should agree with a prototype of the same return type")
	}
	if signaturesAgree(unprototyped, split("void f(s32 a);")) {
		t.Error("return types must still agree")
	}
	if signaturesAgree(split("extern u8 g[];"), split("extern u8 g[][32];")) {
		t.Error("array dimensionality must be preserved")
	}
}

func TestValidateDeclarationsRequiresNoConflicts(t *testing.T) {
	root := t.TempDir()
	writeTestFile(t, filepath.Join(root, "include", "fft", "unit.h"), []byte("typedef struct unit { u8 id; } unit_t;\nextern unit_t g_units[];\n"))
	writeTestFile(t, filepath.Join(root, "src", "battle", "a.c"), []byte("extern struct unit g_units[];\n"))
	writeTestFile(t, filepath.Join(root, "src", "battle", "b.c"), []byte("extern u8 g_flags;\n"))
	writeTestFile(t, filepath.Join(root, "src", "world", "c.c"), []byte("extern s8 g_flags;\n"))

	if err := validateDeclarations(root); err == nil || !strings.Contains(err.Error(), "g_flags") {
		t.Fatalf("expected a conflict for g_flags, got %v", err)
	}
	groups, err := collectDeclarationGroups(root, declarationScanDirs)
	if err != nil {
		t.Fatal(err)
	}
	conflicts := conflictingDeclarationGroups(groups)
	if len(conflicts) != 1 || conflicts[0].symbol != "g_flags" || conflicts[0].minority != 1 {
		t.Fatalf("conflicts = %+v, want only g_flags with one declaration to reconcile", conflicts)
	}
	writeTestFile(t, filepath.Join(root, "src", "world", "c.c"), []byte("extern u8 g_flags;\n"))
	if err := validateDeclarations(root); err != nil {
		t.Fatalf("resolved tree: %v", err)
	}
}
