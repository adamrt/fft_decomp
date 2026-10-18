// compile_commands.go writes compile_commands.json for editors. Each C source
// gets one host clang command with the preprocessor view of the program that
// links it first (its FFT_ program macros and VARIANT_ values), so clangd sees
// the code that program builds instead of treating every program block as
// inactive.
package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"sort"
	"strings"
)

// editorBaseFlags are the host clang flags shared by every entry: the
// historical target's ABI, no host headers and no host predefines.
var editorBaseFlags = []string{
	"-target", "mipsel-none-elf", "-march=mips1", "-mabi=32", "-msoft-float",
	"-std=gnu89", "-fsyntax-only", "-undef", "-nostdinc", "-Iinclude", "-Wno-everything",
}

type compileCommand struct {
	Directory string   `json:"directory"`
	File      string   `json:"file"`
	Arguments []string `json:"arguments"`
}

// editorModuleRank orders the programs whose view an editor shows for a
// shared source: the two programs that link most shared code first.
func editorModuleRank(id string) int {
	for index, program := range []string{"battle", "world", "wldcore", "open", "main"} {
		if id == program {
			return index
		}
	}
	if strings.HasPrefix(id, "event-") {
		return 10
	}
	return 20
}

// editorSourceViews maps each C source to the module whose view the editor
// uses: the best-ranked module that compiles it.
func editorSourceViews(config *projectConfig) map[string]*moduleSpec {
	modules := append([]*moduleSpec(nil), config.Modules...)
	sort.SliceStable(modules, func(i, j int) bool {
		return editorModuleRank(modules[i].ID) < editorModuleRank(modules[j].ID)
	})
	views := map[string]*moduleSpec{}
	for _, m := range modules {
		for index := range m.Functions {
			f := &m.Functions[index]
			if f.Asm != "" {
				continue
			}
			source := m.sourceOf(f)
			if !strings.HasSuffix(source, ".c") {
				continue
			}
			if _, ok := views[source]; !ok {
				views[source] = m
			}
		}
	}
	return views
}

func (p project) compileCommandsCommand(args []string) error {
	flags := flag.NewFlagSet("compile-commands", flag.ContinueOnError)
	cc := flags.String("cc", "clang", "host compiler named in each entry")
	directory := flags.String("directory", "", "checkout path the editor sees (default: this checkout)")
	out := flags.String("out", "compile_commands.json", "output path inside the checkout")
	if err := flags.Parse(args); err != nil {
		return err
	}
	config, err := p.loadProjectConfig()
	if err != nil {
		return err
	}
	root := *directory
	if root == "" {
		root = p.root
	}
	views := editorSourceViews(config)
	var sources []string
	err = filepath.WalkDir(filepath.Join(p.root, "src"), func(path string, entry fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if !entry.IsDir() && strings.HasSuffix(path, ".c") {
			relative, err := filepath.Rel(p.root, path)
			if err != nil {
				return err
			}
			sources = append(sources, filepath.ToSlash(relative))
		}
		return nil
	})
	if err != nil {
		return err
	}
	sort.Strings(sources)
	commands := make([]compileCommand, 0, len(sources))
	for _, source := range sources {
		arguments := append([]string{*cc}, editorBaseFlags...)
		if m := views[source]; m != nil {
			arguments = append(arguments, m.defineFlags()...)
		}
		arguments = append(arguments, source)
		commands = append(commands, compileCommand{Directory: root, File: source, Arguments: arguments})
	}
	data, err := json.MarshalIndent(commands, "", "  ")
	if err != nil {
		return err
	}
	clean := filepath.Clean(filepath.FromSlash(*out))
	if filepath.IsAbs(clean) || clean == ".." || strings.HasPrefix(clean, ".."+string(filepath.Separator)) {
		return fmt.Errorf("--out must be a path inside the checkout: %s", *out)
	}
	if err := os.WriteFile(filepath.Join(p.root, clean), append(data, '\n'), 0o644); err != nil {
		return err
	}
	return nil
}
