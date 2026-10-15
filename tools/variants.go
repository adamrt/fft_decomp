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

var preprocessorDefine = regexp.MustCompile(`(?m)^[ \t]*#[ \t]*define[ \t]+((?:VARIANT_|FFT_)[A-Za-z0-9_]*)`)

// conditionalProblems checks the conditional directives of one file: a
// source may test only the program macros (FFT_) and VARIANT_ names; a
// header may test program macros (a record laid out per program) but never
// VARIANT_ names, since every module includes it. Include guards and other
// names are fine in headers.
func conditionalProblems(path string, data []byte, knownMacros map[string]bool, header bool) []string {
	var problems []string
	lines := strings.Split(string(data), "\n")
	// A macro the file defines itself may be tested first (the NULL idiom,
	// include guards): that is a guard, not a program difference.
	definedHere := map[string]bool{}
	for _, line := range lines {
		if trimmed := strings.TrimSpace(line); strings.HasPrefix(trimmed, "#") {
			if word, rest := directiveWord(trimmed); word == "define" {
				if fields := strings.FieldsFunc(rest, func(r rune) bool { return r == ' ' || r == '\t' || r == '(' }); len(fields) > 0 {
					definedHere[fields[0]] = true
				}
			}
		}
	}
	for number, line := range lines {
		trimmed := strings.TrimSpace(line)
		if !strings.HasPrefix(trimmed, "#") {
			continue
		}
		for _, name := range conditionalIdentifiers(trimmed) {
			program := strings.HasPrefix(name, moduleMacroPrefix)
			variant := strings.HasPrefix(name, definePrefix)
			switch {
			case definedHere[name] && !program && !variant:
			case header && variant:
				problems = append(problems, fmt.Sprintf("%s:%d tests %s; headers may not test VARIANT_ names (every module includes them)", path, number+1, name))
			case header && program && !knownMacros[name]:
				problems = append(problems, fmt.Sprintf("%s:%d tests %s, which names no module or module family", path, number+1, name))
			case header:
			case program && !knownMacros[name]:
				problems = append(problems, fmt.Sprintf("%s:%d tests %s, which names no module or module family", path, number+1, name))
			case !program && !variant:
				problems = append(problems, fmt.Sprintf("%s:%d tests %s; sources may test only FFT_ program macros and VARIANT_ names", path, number+1, name))
			}
		}
	}
	return problems
}

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
	knownMacros := make(map[string]bool)
	for _, m := range config.Modules {
		for _, name := range m.macroNames() {
			knownMacros[name] = true
		}
	}
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
			fail("%s defines %s; VARIANT_ and FFT_ names come only from the build", source, match[1])
		}
		problems = append(problems, conditionalProblems(source, data, knownMacros, false)...)
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
			fail("%s defines %s; VARIANT_ and FFT_ names come only from the build", filepath.ToSlash(relative), match[1])
		}
		problems = append(problems, conditionalProblems(filepath.ToSlash(relative), data, knownMacros, true)...)
		if uses, _ := variantIdentifiers(data); len(uses) > 0 {
			fail("%s uses %s; headers may not use VARIANT_ names (every module includes them)", filepath.ToSlash(relative), strings.Join(uses, ", "))
		}
		return nil
	})
	return problems
}

// moduleSourceDir names the folder a body lives in, or "" for the library
// trees. A shared folder counts: a shared body beside a module's own body
// under one name is a variant pair too.
func moduleSourceDir(source string) string {
	parts := strings.Split(source, "/")
	if len(parts) < 3 || parts[0] != "src" || parts[1] == "lib" {
		return ""
	}
	return filepath.ToSlash(filepath.Dir(source))
}

// validateVariantNames enforces the _variant rule: a C definition symbol
// with more than one body (bodies under several folders, or several
// conditional definitions in one shared file) ends in _variant, and a
// _variant symbol has at least two bodies.
func validateVariantNames(root string, config *projectConfig) []string {
	files := make(map[string]map[string]bool) // symbol -> source files defining it
	for _, m := range config.Modules {
		for index := range m.Functions {
			f := &m.Functions[index]
			if f.Asm != "" {
				continue
			}
			source := m.sourceOf(f)
			if moduleSourceDir(source) == "" {
				continue
			}
			symbol := f.sourceSymbol()
			if files[symbol] == nil {
				files[symbol] = make(map[string]bool)
			}
			files[symbol][source] = true
		}
	}
	definitionCounts := make(map[string]map[string]int) // file -> symbol -> definitions in its raw text
	countIn := func(source string) map[string]int {
		if counts, ok := definitionCounts[source]; ok {
			return counts
		}
		counts := map[string]int{}
		data, err := os.ReadFile(filepath.Join(root, filepath.FromSlash(source)))
		if err == nil {
			if declarations, _, err := scanFileScopeDeclarations(string(data)); err == nil {
				for _, declaration := range declarations {
					if declaration.definition && functionDeclarationSignature(declaration.signature) {
						counts[declaration.symbol]++
					}
				}
			}
		}
		definitionCounts[source] = counts
		return counts
	}
	var problems []string
	for _, symbol := range sortedKeys(mapKeysTrue(files)) {
		bodies := 0
		folders := make(map[string]bool)
		for _, source := range sortedKeys(files[symbol]) {
			folders[moduleSourceDir(source)] = true
			count := countIn(source)[symbol]
			if count == 0 {
				count = 1
			}
			bodies += count
		}
		variant := strings.HasSuffix(symbol, variantSuffix)
		switch {
		case bodies > 1 && !variant && len(folders) > 1:
			problems = append(problems, fmt.Sprintf("%s has a body under %s; a per-module body needs the %s suffix", symbol, strings.Join(sortedKeys(folders), " and "), variantSuffix))
		case bodies > 1 && !variant:
			problems = append(problems, fmt.Sprintf("%s has %d conditional bodies in %s; a per-module body needs the %s suffix", symbol, bodies, strings.Join(sortedKeys(files[symbol]), " and "), variantSuffix))
		case bodies < 2 && variant:
			problems = append(problems, fmt.Sprintf("%s has one body (%s); the %s suffix is for per-module bodies", symbol, strings.Join(sortedKeys(folders), ""), variantSuffix))
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
