package main

import (
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
)

func TestUnitConstantsRemainVisibleInExclusiveBodies(t *testing.T) {
	requirePinnedToolchain(t)
	p := project{root: t.TempDir()}
	battle, world := &moduleSpec{ID: "battle"}, &moduleSpec{ID: "world"}
	order := []*moduleSpec{battle, world}
	bodies := map[string]map[string]*unitMemberBody{"entry": {
		"battle": {text: "enum { MENU_LIMIT = 7 };\nint entry(void) { return MENU_LIMIT; }"},
		"world":  {text: "enum { MENU_LIMIT = 7 };\nint entry(void) { return MENU_LIMIT + 1; }"},
	}}
	owners := map[string]map[string][]*moduleSpec{"entry": {"battle": {battle}, "world": {world}}}
	merged, _, err := renderUnitSource(order, []string{"entry"}, bodies, owners, nil)
	if err != nil {
		t.Fatal(err)
	}
	source := filepath.Join(p.root, "merged.c")
	writeTestFile(t, source, merged)
	linker := filepath.Join(p.root, "layout.ld")
	writeTestFile(t, linker, []byte("SECTIONS { . = 0x80010000; .text : { *(.text) } /DISCARD/ : { *(.reginfo) } }\n"))
	profile, _ := compilerProfileNamed(defaultProfile)
	for _, module := range order {
		if _, err := p.compileHistoricalSections(source, linker, filepath.Join(p.root, module.ID), profile, module.defineFlags()...); err != nil {
			t.Errorf("rendered %s lost its local constant: %v\n%s", module.ID, err, merged)
		}
	}
}

func TestUnitRefusesPartialConstantVisibilityBeforeMutation(t *testing.T) {
	p := unitsTestProject(t)
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	config.byID["world"].function("menu_a1").Source = "src/world/menu_a1.c"
	writeUnitContractConfig(t, p, config)
	writeTestFile(t, filepath.Join(p.root, "src/menu/menu_a1.c"), []byte("enum { MENU_LIMIT = 7 };\nint menu_a1(void) { return MENU_LIMIT; }\n"))
	writeTestFile(t, filepath.Join(p.root, "src/world/menu_a1.c"), []byte("int menu_a1(void) { return 7; }\n"))
	writeTestFile(t, filepath.Join(p.root, "src/menu/menu_a2.c"), []byte("enum { MENU_LIMIT = 7 };\nint menu_a2(void) { return MENU_LIMIT + 1; }\n"))
	before := map[string][]byte{}
	for _, path := range []string{"target/battle.yaml", "target/world.yaml", "src/menu/menu_a1.c", "src/world/menu_a1.c", "src/menu/menu_a2.c"} {
		before[path], err = os.ReadFile(filepath.Join(p.root, path))
		if err != nil {
			t.Fatal(err)
		}
	}
	args := []string{"--name", "partial", "--source", "src/menu/partial.c", "battle:menu_a1", "battle:menu_a2", "world:menu_a1", "world:menu_a2"}
	for _, dry := range []bool{true, false} {
		invocation := append([]string(nil), args...)
		if dry {
			invocation = append([]string{"--dry-run"}, invocation...)
		}
		if err := p.unitSymbolsCommand(invocation); err == nil || !strings.Contains(err.Error(), "visibility") {
			t.Fatalf("partial visibility was not refused: %v", err)
		}
		for path, original := range before {
			after, err := os.ReadFile(filepath.Join(p.root, path))
			if err != nil || string(after) != string(original) {
				t.Fatalf("refused merge changed %s: %v", path, err)
			}
		}
		if _, err := os.Stat(filepath.Join(p.root, "src/menu/partial.c")); !os.IsNotExist(err) {
			t.Fatalf("refused merge created output: %v", err)
		}
	}
}

func TestUnitConstantDedupRefusesUnresolvedScopes(t *testing.T) {
	for name, text := range map[string]string{
		"program guard":            "#ifdef FFT_BATTLE\nenum { LIMIT = 7 };\n#endif\nint entry(void) { return LIMIT; }",
		"macro undef":              "#define LIMIT 7\n#undef LIMIT\n#define LIMIT 7\n",
		"continued macro":          "#define LIMIT \\\n7\n",
		"indented continued macro": "  #define LIMIT \\\n7\n",
		"block enum":               "int entry(void) {\nenum { LIMIT = 7 };\nreturn LIMIT;\n}",
	} {
		t.Run(name, func(t *testing.T) {
			if _, err := dropRepeatedConstants(text, map[string]map[string]bool{}, []*moduleSpec{{ID: "battle"}}); err == nil {
				t.Fatal("accepted a constant without proven file-scope visibility")
			}
		})
	}
}

func TestUnitConstantDedupRecognizesIndentation(t *testing.T) {
	modules := []*moduleSpec{{ID: "battle"}}
	for _, constant := range []string{"  enum { LIMIT = 7 };\n", "\t#define LIMIT 7\n", "  # define LIMIT 7\n"} {
		seen := map[string]map[string]bool{}
		if _, err := dropRepeatedConstants(constant+"int first(void) { return LIMIT; }", seen, modules); err != nil {
			t.Fatal(err)
		}
		second, err := dropRepeatedConstants(constant+"int second(void) { return LIMIT; }", seen, modules)
		if err != nil || strings.Contains(second, constant) {
			t.Fatalf("indented repeated constant was not handled: %v\n%s", err, second)
		}
	}
}

func TestUnitConstantDedupPreservesUnrelatedText(t *testing.T) {
	text := "/* π: documentation\nenum { LIMIT = 7 };\n*/\n#define LIMIT 7\nint first(void) { return LIMIT; }\n"
	seen := map[string]map[string]bool{}
	modules := []*moduleSpec{{ID: "battle"}, {ID: "world"}}
	first, err := dropRepeatedConstants(text, seen, modules)
	if err != nil || first != text {
		t.Fatalf("changed first declaration or unrelated text: %v\n%s", err, first)
	}
	second, err := dropRepeatedConstants("#define LIMIT 7\nint second(void) { return LIMIT; }", seen, modules)
	if err != nil || strings.Contains(second, "#define") {
		t.Fatalf("safe shared duplicate was retained: %v\n%s", err, second)
	}
}

func aliasMemberBodyFixture(t *testing.T, source string, overrides []overrideSpec) (*unitMemberBody, error) {
	t.Helper()
	root := t.TempDir()
	writeTestFile(t, filepath.Join(root, "src/input_a.c"), []byte(source))
	m := &moduleSpec{ID: "battle", SourceDir: "src", Functions: []functionSpec{
		{Name: "input_a", Addr: 0x80010000},
		{Name: "input_b", Addr: 0x80010020},
		{Name: "card_input_a", Addr: 0x80010040, SourceSymbol: "input_a"},
		{Name: "card_c", Addr: 0x80010060},
	}, Overrides: overrides}
	return memberBody(root, m, &m.Functions[2], nil)
}

func TestUnitAliasPreservesLiteralAndMemberNamespaces(t *testing.T) {
	for name, source := range map[string]string{
		"literal": "int input_b(char*);\nint input_a(void) { return input_b(\"input_a input_b\"); }\n",
		"member":  "struct record { int input_b; };\nint input_b(void);\nint input_a(struct record* record) { return record->input_b + input_b(); }\n",
	} {
		t.Run(name, func(t *testing.T) {
			body, err := aliasMemberBodyFixture(t, source, []overrideSpec{{Function: "card_input_a", Name: "input_b", Addr: 0x80010060}})
			if err != nil {
				if name == "literal" {
					t.Fatal(err)
				}
				t.Logf("conservatively refused: %v", err)
				return
			}
			if name == "literal" && !strings.Contains(body.text, "\"input_a input_b\"") || name == "member" && !strings.Contains(body.text, "record->input_b") {
				t.Fatalf("changed a non-linker namespace:\n%s", body.text)
			}
		})
	}
}

func TestUnitAliasPreservesCommentsAndStringsExactly(t *testing.T) {
	source := "/* input_a calls input_b; card_c is unrelated prose. */\nint input_b(char*);\nint input_a(void) { return input_b(\"input_a input_b card_c\"); }\n"
	body, err := aliasMemberBodyFixture(t, source, []overrideSpec{{Function: "card_input_a", Name: "input_b", Addr: 0x80010060}})
	if err != nil {
		t.Fatal(err)
	}
	for _, preserved := range []string{"/* input_a calls input_b; card_c is unrelated prose. */", "\"input_a input_b card_c\""} {
		if !strings.Contains(body.text, preserved) {
			t.Fatalf("changed non-code text:\n%s", body.text)
		}
	}
	if !strings.Contains(body.text, "int card_input_a(void) { return card_c(") {
		t.Fatalf("did not materialize the direct call:\n%s", body.text)
	}
}

func TestUnitAliasRefusesAmbiguousFunctionNamespaces(t *testing.T) {
	for name, source := range map[string]string{
		"local destination":     "int input_a(void) { int (*card_c)(void); return input_b(); }",
		"parameter destination": "int input_a(int (*card_c)(void)) { return input_b(); }",
		"parameter source":      "int input_a(int input_b(void)) { return input_b(); }",
		"pointer source":        "int input_a(void) { int (*input_b)(void); return input_b(); }",
		"member call":           "int input_a(record_t* record) { return record->input_b(); }",
		"address use":           "int input_a(void) { return (int)&input_b; }",
		"label":                 "int input_a(void) { input_b: return input_b(); }",
		"macro":                 "#define DISPATCH() input_b()\nint input_a(void) { return DISPATCH(); }",
		"hidden macro":          "int input_a(void) { return input_b() + DISPATCH(); }",
		"unused override":       "int input_b(void);\nint input_a(void) { return 7; }",
	} {
		t.Run(name, func(t *testing.T) {
			_, err := aliasMemberBodyFixture(t, source, []overrideSpec{{Function: "card_input_a", Name: "input_b", Addr: 0x80010060}})
			if err == nil || !strings.Contains(err.Error(), "materialize reviewed source manually") {
				t.Fatalf("ambiguous alias was not refused: %v", err)
			}
		})
	}
}

func TestUnitAliasOverridesAreSimultaneous(t *testing.T) {
	body, err := aliasMemberBodyFixture(t, "int input_b(void), card_c(void);\nint input_a(void) { return input_b() + card_c(); }\n", []overrideSpec{
		{Function: "card_input_a", Name: "input_b", Addr: 0x80010060},
		{Function: "card_input_a", Name: "card_c", Addr: 0x80010020},
	})
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(body.text, "return card_c() + input_b();") {
		t.Fatalf("sequential substitutions cascaded:\n%s", body.text)
	}
}

func TestUnitAliasBodyIdentityIncludesMaterializedBindings(t *testing.T) {
	source := "int input_b(void);\nint input_a(void) { return input_b(); }\n"
	first, err := aliasMemberBodyFixture(t, source, []overrideSpec{{Function: "card_input_a", Name: "input_b", Addr: 0x80010060}})
	if err != nil {
		t.Fatal(err)
	}
	second, err := aliasMemberBodyFixture(t, source, nil)
	if err != nil {
		t.Fatal(err)
	}
	if first.text == second.text || first.key == second.key {
		t.Fatalf("different materialized program bindings collapse into one body: %q / %q", first.key, second.key)
	}
	third, err := aliasMemberBodyFixture(t, source, nil)
	if err != nil || third.key != second.key {
		t.Fatalf("identical materialized bodies should still share: %v", err)
	}
}

func TestUnitAliasRefusesLocalCaptureBeforeMutation(t *testing.T) {
	p := unitsTestProject(t)
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	m := config.byID["battle"]
	m.function("menu_b2").SourceSymbol = "menu_b1"
	m.function("menu_b2").Source = "src/battle/menu_b1.c"
	m.Overrides = []overrideSpec{{Function: "menu_b2", Name: "g_b_scalar", Addr: 0x80067500}}
	writeUnitContractConfig(t, p, config)
	writeTestFile(t, filepath.Join(p.root, "src/battle/menu_b1.c"), []byte("extern int g_b_scalar;\nint menu_b1(void) { int g_late = 3; return g_b_scalar + g_late; }\n"))
	sourcePath, targetPath := filepath.Join(p.root, "src/battle/menu_b1.c"), filepath.Join(p.root, "target/battle.yaml")
	source, _ := os.ReadFile(sourcePath)
	target, _ := os.ReadFile(targetPath)
	err = p.unitSymbolsCommand([]string{"--name", "materialized", "--source", "src/battle/materialized.c", "battle:menu_b2"})
	if err == nil {
		t.Error("materialized an override into a shadowing local name")
	}
	for path, before := range map[string][]byte{sourcePath: source, targetPath: target} {
		after, readErr := os.ReadFile(path)
		if readErr != nil || string(before) != string(after) {
			t.Errorf("unsafe merge changed %s: %v", path, readErr)
		}
	}
	if _, err := os.Stat(filepath.Join(p.root, "src/battle/materialized.c")); !os.IsNotExist(err) {
		t.Errorf("unsafe merge created output: %v", err)
	}
}

func TestUnitAliasFunctionCaptureLeavesEverySourceAndTargetUntouched(t *testing.T) {
	for name, source := range map[string]string{
		"destination local": "int menu_b3(void);\nint menu_b1(void) { int (*menu_b4)(void); return menu_b3(); }\n",
		"source member":     "int menu_b1(record_t* record) { return record->menu_b3(); }\n",
		"unresolved macro":  "int menu_b1(void) { return menu_b3() + DISPATCH(); }\n",
	} {
		t.Run(name, func(t *testing.T) {
			p := unitsTestProject(t)
			config, err := p.loadProjectConfig()
			if err != nil {
				t.Fatal(err)
			}
			m := config.byID["battle"]
			m.function("menu_b2").SourceSymbol = "menu_b1"
			m.function("menu_b2").Source = "src/battle/menu_b1.c"
			m.Overrides = []overrideSpec{{Function: "menu_b2", Name: "menu_b3", Addr: m.function("menu_b4").Addr}}
			writeUnitContractConfig(t, p, config)
			writeTestFile(t, filepath.Join(p.root, "src/battle/menu_b1.c"), []byte(source))
			snapshot := func() map[string]string {
				files := map[string]string{}
				for _, root := range []string{"src", "target"} {
					err := filepath.WalkDir(filepath.Join(p.root, root), func(path string, entry os.DirEntry, err error) error {
						if err != nil {
							return err
						}
						if entry.IsDir() {
							return nil
						}
						data, err := os.ReadFile(path)
						if err != nil {
							return err
						}
						files[path] = string(data)
						return nil
					})
					if err != nil {
						t.Fatal(err)
					}
				}
				return files
			}
			before := snapshot()
			for _, dry := range []bool{true, false} {
				args := []string{"--name", "materialized", "--source", "src/battle/materialized.c", "battle:menu_b2"}
				if dry {
					args = append([]string{"--dry-run"}, args...)
				}
				if err := p.unitSymbolsCommand(args); err == nil || !strings.Contains(err.Error(), "materialize reviewed source manually") {
					t.Fatalf("unsafe alias was not refused: %v", err)
				}
				if !reflect.DeepEqual(snapshot(), before) {
					t.Fatal("refused alias changed source or target files")
				}
			}
		})
	}
}

func TestUnitAliasHeaderMacrosCannotChangeBindings(t *testing.T) {
	requirePinnedToolchain(t)
	for name, test := range map[string]struct{ header, body string }{
		"source macro":      {"#define menu_b3() 7\n", "return menu_b3();"},
		"destination macro": {"#define menu_b4() 7\n", "return menu_b3();"},
		"indirect macro":    {"#define menu_a1() menu_b3()\n", "return menu_b3() + menu_a1();"},
		"program macro":     {"#if defined(FFT_BATTLE) && VARIANT_ALIAS_MACRO == 7\n#define menu_b3() 7\n#endif\n", "return menu_b3();"},
	} {
		t.Run(name, func(t *testing.T) {
			p := unitsTestProject(t)
			config, err := p.loadProjectConfig()
			if err != nil {
				t.Fatal(err)
			}
			m := config.byID["battle"]
			f := m.function("menu_b2")
			m.Defines = []defineSpec{{Name: "VARIANT_ALIAS_MACRO", Value: "7"}}
			f.Source, f.SourceSymbol = "src/battle/menu_b1.c", "menu_b1"
			m.Overrides = []overrideSpec{{Function: f.Name, Name: "menu_b3", Addr: m.function("menu_b4").Addr}}
			writeUnitContractConfig(t, p, config)
			writeTestFile(t, filepath.Join(p.root, "include/macros.h"), []byte(test.header))
			writeTestFile(t, filepath.Join(p.root, f.Source), []byte("#include \"macros.h\"\nint menu_b1(void) { "+test.body+" }\n"))
			body, err := memberBody(p.root, m, f, nil)
			if err == nil {
				resolver := newSymbolResolver(config)
				before, compileErr := p.compileFunction(resolver, m, f, filepath.Join(p.root, "original"))
				if compileErr != nil {
					t.Fatal(compileErr)
				}
				candidate := *f
				candidate.SourceSymbol, candidate.Source = "", "src/battle/candidate.c"
				writeTestFile(t, filepath.Join(p.root, candidate.Source), []byte(strings.Join(body.includes, "\n")+"\n"+body.text))
				m.Overrides = nil
				after, compileErr := p.compileFunction(newSymbolResolver(config), m, &candidate, filepath.Join(p.root, "candidate"))
				if compileErr != nil {
					t.Fatal(compileErr)
				}
				if string(before[".text"]) == string(after[".text"]) {
					t.Fatal("fixture did not change the generated code")
				}
				t.Fatal("header macro changed the alias behavior without rejection")
			}
			if !strings.Contains(err.Error(), "macro") {
				t.Fatalf("unexpected refusal: %v", err)
			}
			originalSource, _ := os.ReadFile(filepath.Join(p.root, f.Source))
			originalTarget, _ := os.ReadFile(filepath.Join(p.root, "target/battle.yaml"))
			for _, dry := range []bool{true, false} {
				args := []string{"--name", "materialized", "--source", "src/battle/materialized.c", "battle:menu_b2"}
				if dry {
					args = append([]string{"--dry-run"}, args...)
				}
				if err := p.unitSymbolsCommand(args); err == nil || !strings.Contains(err.Error(), "macro") {
					t.Fatalf("header macro merge was not refused: %v", err)
				}
				sourceAfter, _ := os.ReadFile(filepath.Join(p.root, f.Source))
				targetAfter, _ := os.ReadFile(filepath.Join(p.root, "target/battle.yaml"))
				if string(sourceAfter) != string(originalSource) || string(targetAfter) != string(originalTarget) {
					t.Fatal("refused macro alias changed source or target")
				}
				if _, err := os.Stat(filepath.Join(p.root, "src/battle/materialized.c")); !os.IsNotExist(err) {
					t.Fatalf("refused macro alias created output: %v", err)
				}
			}
		})
	}
}

func TestUnitAliasWithOrdinaryHeaderDeclarationsStillMaterializes(t *testing.T) {
	requirePinnedToolchain(t)
	p := unitsTestProject(t)
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	m := config.byID["battle"]
	f := m.function("menu_b2")
	f.Source, f.SourceSymbol = "src/battle/menu_b1.c", "menu_b1"
	m.Overrides = []overrideSpec{{Function: f.Name, Name: "menu_b3", Addr: m.function("menu_b4").Addr}}
	writeUnitContractConfig(t, p, config)
	header := "#define UNUSED_LIMIT 7\n#ifdef FFT_WORLD\n#define menu_b3() 7\n#endif\nint menu_b3(void);\nint menu_b4(void);\n"
	writeTestFile(t, filepath.Join(p.root, "include/declarations.h"), []byte(header))
	writeTestFile(t, filepath.Join(p.root, f.Source), []byte("#include \"declarations.h\"\nint menu_b1(void) { return menu_b3(); }\n"))
	if err := p.unitSymbolsCommand([]string{"--name", "materialized", "--source", "src/battle/materialized.c", "battle:menu_b2"}); err != nil {
		t.Fatal(err)
	}
	merged, err := os.ReadFile(filepath.Join(p.root, "src/battle/materialized.c"))
	if err != nil || !strings.Contains(string(merged), "int menu_b2(void) { return menu_b4(); }") || !strings.Contains(string(merged), "#include \"declarations.h\"") {
		t.Fatalf("ordinary included alias was not materialized: %v\n%s", err, merged)
	}
	headerAfter, err := os.ReadFile(filepath.Join(p.root, "include/declarations.h"))
	if err != nil || string(headerAfter) != header {
		t.Fatalf("header was changed: %v", err)
	}
}
