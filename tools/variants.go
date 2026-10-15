// variants.go checks the two rules that let one source serve several
// programs: VARIANT_ build constants come only from a module's defines, and a
// function body that differs per module carries the _variant suffix.
package main

import (
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"
)

// variantSuffix marks a function whose body each module links separately
// under one shared name.
const variantSuffix = "_variant"

var preprocessorDefine = regexp.MustCompile(`(?m)^[ \t]*#[ \t]*define[ \t]+(VARIANT_[A-Za-z0-9_]*)`)

// variantIdentifiers lists the VARIANT_ names a C file uses: in code (comments
// skipped) and on preprocessor lines, which the tokenizer drops.
func variantIdentifiers(data []byte) ([]string, error) {
	if !strings.Contains(string(data), definePrefix) {
		return nil, nil
	}
	tokens, err := tokenizeC(string(data))
	if err != nil {
		return nil, err
	}
	names := make(map[string]bool)
	for _, token := range tokens {
		if token.kind == cIdent && strings.HasPrefix(token.text, definePrefix) {
			names[token.text] = true
		}
	}
	for _, line := range strings.Split(string(data), "\n") {
		trimmed := strings.TrimLeft(line, " \t")
		if !strings.HasPrefix(trimmed, "#") {
			continue
		}
		for _, name := range regexp.MustCompile(`\bVARIANT_[A-Za-z0-9_]*`).FindAllString(trimmed, -1) {
			names[name] = true
		}
	}
	return sortedKeys(names), nil
}

func validDefineValue(value string) bool {
	_, err := strconv.ParseInt(value, 0, 64)
	return err == nil
}

// validateDefines enforces the defines contract: every key is a VARIANT_
// name with an integer value, no source or header defines one itself, no
// header uses one (a header reaches every module), every VARIANT_ name a
// source uses is defined by each module compiling it, and every define is
// used by a source its module compiles.
func validateDefines(root string, config *projectConfig) []string {
	var problems []string
	fail := func(format string, args ...any) { problems = append(problems, fmt.Sprintf(format, args...)) }
	usesCache := make(map[string][]string)
	usesOf := func(source string) []string {
		if uses, ok := usesCache[source]; ok {
			return uses
		}
		data, err := os.ReadFile(filepath.Join(root, filepath.FromSlash(source)))
		if err != nil {
			usesCache[source] = nil
			return nil
		}
		if match := preprocessorDefine.FindSubmatch(data); match != nil {
			fail("%s defines %s; VARIANT_ names come only from target defines", source, match[1])
		}
		uses, err := variantIdentifiers(data)
		if err != nil {
			fail("%s: %v", source, err)
		}
		usesCache[source] = uses
		return uses
	}
	for _, m := range config.Modules {
		label := m.path + " " + m.ID
		seen := make(map[string]bool)
		for _, define := range m.Defines {
			if !validSymbol(define.Name) || !strings.HasPrefix(define.Name, definePrefix) || seen[define.Name] {
				fail("%s: define %q must be a unique %s name", label, define.Name, definePrefix)
			}
			seen[define.Name] = true
			if !validDefineValue(define.Value) {
				fail("%s: define %s has non-integer value %q", label, define.Name, define.Value)
			}
		}
		used := make(map[string]bool)
		for index := range m.Functions {
			f := &m.Functions[index]
			if f.Asm != "" {
				continue
			}
			source := m.sourceOf(f)
			for _, name := range usesOf(source) {
				used[name] = true
				if _, ok := m.defineValue(name); !ok {
					fail("%s: %s uses %s, which this module does not define", label, source, name)
				}
			}
		}
		for _, define := range m.Defines {
			if !used[define.Name] {
				fail("%s: define %s is used by no source this module compiles", label, define.Name)
			}
		}
	}
	_ = filepath.WalkDir(filepath.Join(root, "include"), func(path string, entry fs.DirEntry, err error) error {
		if err != nil || entry.IsDir() || filepath.Ext(path) != ".h" {
			return nil
		}
		data, err := os.ReadFile(path)
		if err != nil {
			return nil
		}
		relative, _ := filepath.Rel(root, path)
		if match := preprocessorDefine.FindSubmatch(data); match != nil {
			fail("%s defines %s; VARIANT_ names come only from target defines", filepath.ToSlash(relative), match[1])
		}
		if uses, _ := variantIdentifiers(data); len(uses) > 0 {
			fail("%s uses %s; headers may not use VARIANT_ names (every module includes them)", filepath.ToSlash(relative), strings.Join(uses, ", "))
		}
		return nil
	})
	return problems
}

// moduleSourceDir names the per-module folder of a source, or "" for the
// shared and library trees.
func moduleSourceDir(source string) string {
	parts := strings.Split(source, "/")
	if len(parts) < 3 || parts[0] != "src" || parts[1] == "shared" || parts[1] == "lib" {
		return ""
	}
	return filepath.ToSlash(filepath.Dir(source))
}

// validateVariantNames enforces the _variant rule: a C definition symbol
// with bodies under more than one module folder ends in _variant, and a
// _variant symbol has bodies under at least two.
func validateVariantNames(config *projectConfig) []string {
	dirs := make(map[string]map[string]bool)
	for _, m := range config.Modules {
		for index := range m.Functions {
			f := &m.Functions[index]
			if f.Asm != "" {
				continue
			}
			dir := moduleSourceDir(m.sourceOf(f))
			if dir == "" {
				continue
			}
			symbol := f.sourceSymbol()
			if dirs[symbol] == nil {
				dirs[symbol] = make(map[string]bool)
			}
			dirs[symbol][dir] = true
		}
	}
	var problems []string
	for _, symbol := range sortedKeys(mapKeysTrue(dirs)) {
		count := len(dirs[symbol])
		variant := strings.HasSuffix(symbol, variantSuffix)
		switch {
		case count > 1 && !variant:
			problems = append(problems, fmt.Sprintf("%s has a body under %s; a per-module body needs the %s suffix", symbol, strings.Join(sortedKeys(dirs[symbol]), " and "), variantSuffix))
		case count < 2 && variant:
			problems = append(problems, fmt.Sprintf("%s has one body (%s); the %s suffix is for per-module bodies", symbol, strings.Join(sortedKeys(dirs[symbol]), ""), variantSuffix))
		}
	}
	sort.Strings(problems)
	return problems
}

func mapKeysTrue[V any](values map[string]V) map[string]bool {
	keys := make(map[string]bool, len(values))
	for key := range values {
		keys[key] = true
	}
	return keys
}
