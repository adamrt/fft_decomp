package main

import (
	"bytes"
	"path/filepath"
	"strings"
	"testing"
)

func TestStandaloneConfigRequiresTheActivePublicDefinition(t *testing.T) {
	for name, test := range map[string]struct {
		source string
		valid  bool
	}{
		"ordinary":         {"void main_first(void) {}", true},
		"trailing body":    {"void main_first(void) {}\nvoid trailing_body(void) {}", true},
		"old-style body":   {"main_first(value) int value; { return value; }", true},
		"wrong name":       {"void unrelated_role(void) {}", false},
		"prototype only":   {"void main_first(void);\nvoid unrelated_role(void) {}", false},
		"static body":      {"static void main_first(void) {}", false},
		"static prototype": {"static void main_first(void);\nvoid main_first(void) {}", false},
		"active branch":    {"#ifdef FFT_MAIN\nvoid main_first(void) {}\n#else\nvoid unrelated_role(void) {}\n#endif", true},
		"inactive branch":  {"#ifdef FFT_BATTLE\nvoid main_first(void) {}\n#else\nvoid unrelated_role(void) {}\n#endif", false},
	} {
		t.Run(name, func(t *testing.T) {
			p := validTestProject(t)
			config, err := p.loadProjectConfig()
			if err != nil {
				t.Fatal(err)
			}
			writeTestFile(t, filepath.Join(p.root, "src/main/main_first.c"), []byte(test.source+"\n"))
			err = p.validateProjectConfig(config)
			if (err == nil) != test.valid {
				t.Fatalf("valid=%t, error=%v", test.valid, err)
			}
		})
	}
}

func TestStandaloneLinkRequiresTheNamedEntryAtItsAddress(t *testing.T) {
	requirePinnedToolchain(t)
	t.Setenv("TOOLS_CACHE", "0")
	p := validTestProject(t)
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	m := config.byID["main"]
	f := &m.Functions[0]
	source := filepath.Join(p.root, m.sourceOf(f))
	resolver := newSymbolResolver(config)
	original, err := p.compileFunction(resolver, m, f, filepath.Join(p.root, "original"))
	if err != nil {
		t.Fatal(err)
	}
	for name, body := range map[string]string{
		"wrong name":     "void unrelated_role(void) {}",
		"prototype only": "void main_first(void);\nvoid unrelated_role(void) {}",
		"wrong address":  "void preceding_body(void) {}\nvoid main_first(void) {}",
	} {
		t.Run(name, func(t *testing.T) {
			writeTestFile(t, source, []byte(body+"\n"))
			sections, err := p.compileFunction(resolver, m, f, filepath.Join(p.root, name))
			if err == nil {
				if len(sections[".text"]) < len(original[".text"]) || !bytes.Equal(sections[".text"][:len(original[".text"])], original[".text"]) {
					t.Fatal("fixture no longer preserves the original entry bytes")
				}
				t.Fatal("accepted the expected bytes without the expected entry definition")
			}
		})
	}
}

func TestStandaloneSelfSymbolCannotComeFromAnAbsoluteBinding(t *testing.T) {
	for _, symbol := range []string{"", "shared_body"} {
		f := &functionSpec{Name: "entry", SourceSymbol: symbol, Addr: 0x80010000}
		script, err := linkerScript(f, map[string]uint32{f.sourceSymbol(): f.Addr, "external_function": 0x80020000})
		if err != nil {
			t.Fatal(err)
		}
		if strings.Contains(script, "PROVIDE("+f.sourceSymbol()+" =") || !strings.Contains(script, "ASSERT("+f.sourceSymbol()+" ==") {
			t.Fatalf("entry definition can be supplied by an alias:\n%s", script)
		}
		if !strings.Contains(script, "PROVIDE(external_function =") {
			t.Fatal("unrelated fixed-address extern lost its binding")
		}
	}
}
