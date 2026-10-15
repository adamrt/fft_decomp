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
		{"extern unit_stats_t* g_unit;", "g_unit", "unit_stats_t * @"},
		{"extern u8 D_800BF790[32];", "D_800BF790", "u8 @ []"},
		{"extern u8 D_800BF790[4][32];", "D_800BF790", "u8 @ [] []"},
		{"s32 battle_check(unit_stats_t* unit);", "battle_check", "s32 @ (unit_stats_t *)"},
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
		{"void f(a, b, c, d) u16 a; register unsigned char b; u8* c; { u16 a2; a2 = a; }", "f", "void @ (s32, s32, u8 *, s32)"},
		{"s16 f(unit, weight) unit_stats_t* unit; float weight; { return 0; }", "f", "s16 @ (unit_stats_t *, double)"},
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
typedef struct unit_stats { u8 id; } unit_stats_t;
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
		// A static declaration is scanned (a unit may keep a member private)
		// but marked, and it never binds a linker symbol.
		if declaration.static {
			if declaration.symbol != "g_local" {
				t.Fatalf("unexpected static declaration %+v", declaration)
			}
			continue
		}
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

func TestArchitecturalRegisterDeclarationsKeepLinkageChecks(t *testing.T) {
	root := t.TempDir()
	writeTestFile(t, filepath.Join(root, "include", "psx", "entry.h"), []byte(`
register u32 entry_stack __asm__("$29");
register u32 entry_result __asm__("$2");
register u32 linked_value __asm__("g_shared_value");
`))
	writeTestFile(t, filepath.Join(root, "include", "psx", "dispatch.h"), []byte(`
register dispatch_frame_t* dispatch_stack __asm__("$29");
register s32 dispatch_result asm("$2");
extern s32 g_shared_value;
`))
	groups, err := collectDeclarationGroups(root, declarationScanDirs)
	if err != nil {
		t.Fatal(err)
	}
	conflicts := conflictingDeclarationGroups(groups)
	if len(conflicts) != 1 || conflicts[0].symbol != "g_shared_value" {
		t.Fatalf("conflicts = %+v, want only the real linker-symbol conflict", conflicts)
	}
	writeTestFile(t, filepath.Join(root, "include", "psx", "dispatch.h"), []byte(`
register dispatch_frame_t* dispatch_stack __asm__("$29");
register s32 dispatch_result asm("$2");
extern u32 g_shared_value;
`))
	if err := validateDeclarations(root); err != nil {
		t.Fatalf("resolved linker declarations: %v", err)
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
	if !signaturesAgree(unprototyped, split("s32 f(unit_stats_t* unit);")) {
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

func TestStaticDefinitionsAreScannedButNotCompared(t *testing.T) {
	declarations := scanOne(t, "static s32 helper(s32 x) { return x; }\ns32 user(void) { return helper(1); }\n")
	if len(declarations) != 2 || declarations[0].symbol != "helper" || !declarations[0].static || !declarations[0].definition || declarations[1].static {
		t.Fatalf("declarations = %+v", declarations)
	}
	if err := validateUnitSourceData("x.c", []byte("static s32 helper(s32 x);\ns32 user(void) { return helper(1); }\nstatic s32 helper(s32 x) { return x; }\n"), []string{"user", "helper"}); err != nil {
		t.Fatalf("static member rejected: %v", err)
	}
	if err := validateUnitSourceData("x.c", []byte("static inline s32 twice(s32 x) { return x * 2; }\ns32 user(void) { return twice(1); }\n"), []string{"user"}); err != nil {
		t.Fatalf("inlined static helper counted: %v", err)
	}
	if _, err := sourceDefinitionSymbol([]byte("static s32 helper(void) { return 1; }\ns32 only(void) { return helper(); }\n")); err != nil {
		t.Fatalf("static helper counted as public: %v", err)
	}
	p := testProject(t, map[string]string{
		"include/fft/a.h": "s32 shared(void);\n",
		"src/main/a.c":    "#include \"fft/a.h\"\nstatic s32 local(void) { return 1; }\ns32 shared(void) { return local(); }\n",
		"src/main/b.c":    "static u8 local(void) { return 2; }\n",
	})
	groups, err := collectDeclarationGroups(p.root, declarationScanDirs)
	if err != nil {
		t.Fatal(err)
	}
	for _, group := range groups {
		if group.symbol == "local" {
			t.Fatalf("static symbols compared across files: %+v", group)
		}
	}
}
