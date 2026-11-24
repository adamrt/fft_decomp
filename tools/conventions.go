// conventions.go checks source conventions that check-config enforces:
// header layers, file-scope storage prefixes and size-assert names.
package main

import (
	"errors"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
)

// headerLayers ranks the include/fft folders from the bottom: a header may
// include only headers of its own layer or a lower one.
var headerLayers = map[string]int{
	"sys": 0,
	"gfx": 1, "text": 1, "menu": 1,
	"game":   2,
	"battle": 3, "map": 3, "effect": 3, "script": 3, "world": 3, "party": 3, "front": 3,
}

var gameInclude = regexp.MustCompile(`^[ \t]*#[ \t]*include[ \t]*"fft/([^/"]+)/[^"]*"`)

// checkHeaderLayers reports each include/fft header include that reaches a
// higher layer, or a folder outside the layer map.
func checkHeaderLayers(root string) ([]string, error) {
	var problems []string
	base := filepath.Join(root, "include", "fft")
	err := filepath.WalkDir(base, func(path string, entry fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if entry.IsDir() || filepath.Ext(path) != ".h" {
			return nil
		}
		relative, _ := filepath.Rel(root, path)
		relative = filepath.ToSlash(relative)
		folder := strings.SplitN(strings.TrimPrefix(relative, "include/fft/"), "/", 2)[0]
		own, ok := headerLayers[folder]
		if !ok || !strings.Contains(strings.TrimPrefix(relative, "include/fft/"), "/") {
			problems = append(problems, fmt.Sprintf("%s: header is outside the layer folders", relative))
			return nil
		}
		data, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		for index, line := range strings.Split(string(data), "\n") {
			match := gameInclude.FindStringSubmatch(line)
			if match == nil {
				continue
			}
			layer, known := headerLayers[match[1]]
			switch {
			case !known:
				problems = append(problems, fmt.Sprintf("%s:%d: includes %s from no known layer", relative, index+1, match[1]))
			case layer > own:
				problems = append(problems, fmt.Sprintf("%s:%d: %s (layer %d) includes %s (layer %d)",
					relative, index+1, folder, own, strings.TrimSpace(line[strings.Index(line, "\"fft/")+1:len(line)-1]), layer))
			}
		}
		return nil
	})
	if err != nil && !errors.Is(err, fs.ErrNotExist) {
		return nil, err
	}
	return problems, nil
}

// conventionExempt reports the sources whose names follow another owner's
// conventions: the reconstructed PsyQ libraries and the Suzuki sound engine.
func conventionExempt(relative string) bool {
	return strings.HasPrefix(relative, "src/platform/psyq/") || strings.HasPrefix(relative, "src/lib/suzuki/")
}

// checkStorageNames reports file-scope names whose prefix disagrees with
// their storage: a static function starts with neither s_ nor g_, a static
// variable starts with s_, and a non-static C-defined variable with g_.
func checkStorageNames(root string) ([]string, error) {
	var problems []string
	err := filepath.WalkDir(filepath.Join(root, "src"), func(path string, entry fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if entry.IsDir() || filepath.Ext(path) != ".c" && filepath.Ext(path) != ".h" {
			return nil
		}
		relative, _ := filepath.Rel(root, path)
		relative = filepath.ToSlash(relative)
		if conventionExempt(relative) {
			return nil
		}
		data, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		text := string(data)
		return visitSourceDefinitions(data, func(definition sourceDefinition) {
			name := definition.name
			if name == "" {
				name = definition.symbol
			}
			var problem string
			switch {
			case definition.register:
				return
			case definition.function && definition.static && (strings.HasPrefix(name, "s_") || strings.HasPrefix(name, "g_")):
				problem = "static function must not start with s_ or g_"
			case definition.function:
				return
			case definition.static && !strings.HasPrefix(name, "s_"):
				problem = "static variable must start with s_"
			case !definition.static && !strings.HasPrefix(name, "g_"):
				problem = "global variable must start with g_"
			default:
				return
			}
			problems = append(problems, fmt.Sprintf("%s:%d: %s: %s", relative, lineOf(text, definition.offset), name, problem))
		})
	})
	if err != nil && !errors.Is(err, fs.ErrNotExist) {
		return nil, err
	}
	return problems, nil
}

var sizeAssertName = regexp.MustCompile(`^([A-Za-z0-9_]+)_size_must_be_([0-9]+)$`)

// checkSizeAsserts reports size asserts (typedef char X_size_must_be_N[...])
// that do not name the type their sizeof checks, or that do not directly
// follow the declaration of that type.
func checkSizeAsserts(root string) ([]string, error) {
	var problems []string
	for _, directory := range []string{"include", "src"} {
		err := filepath.WalkDir(filepath.Join(root, directory), func(path string, entry fs.DirEntry, err error) error {
			if err != nil {
				return err
			}
			if entry.IsDir() || filepath.Ext(path) != ".c" && filepath.Ext(path) != ".h" {
				return nil
			}
			data, err := os.ReadFile(path)
			if err != nil {
				return err
			}
			if !strings.Contains(string(data), "_size_must_be_") {
				return nil
			}
			relative, _ := filepath.Rel(root, path)
			relative = filepath.ToSlash(relative)
			// Suzuki keeps its established names.
			if strings.HasPrefix(relative, "src/lib/suzuki/") || relative == "include/fft/sys/suzuki.h" {
				return nil
			}
			found, err := sizeAssertProblems(relative, string(data))
			problems = append(problems, found...)
			return err
		})
		if err != nil && !errors.Is(err, fs.ErrNotExist) {
			return nil, err
		}
	}
	return problems, nil
}

func sizeAssertProblems(relative, text string) ([]string, error) {
	tokens, err := tokenizeC(text)
	if err != nil {
		return nil, fmt.Errorf("%s: %w", relative, err)
	}
	var problems []string
	var previous []cToken
	aliases := make(map[string]string) // typedef name -> struct, union or enum tag
	for index := 0; index < len(tokens); {
		end := fileScopeStatementEnd(tokens, index)
		statement := tokens[index:end]
		index = end
		if alias, tag, ok := typedefTag(statement); ok {
			aliases[alias] = tag
		}
		name, typeName, size, ok := sizeAssert(statement)
		if !ok {
			if len(statement) > 0 && statement[0].text != ";" {
				previous = statement
			}
			continue
		}
		line := lineOf(text, statement[0].start)
		match := sizeAssertName.FindStringSubmatch(name)
		switch {
		case match == nil:
			problems = append(problems, fmt.Sprintf("%s:%d: %s is not named X_size_must_be_N", relative, line, name))
		case !sizeAssertNames(match[1], typeName):
			problems = append(problems, fmt.Sprintf("%s:%d: %s checks %s; name it %s_size_must_be_%s", relative, line, name, typeName, strings.TrimSuffix(strings.TrimSuffix(typeName, "_t"), "_e"), match[2]))
		case isDecimal(size) && size != match[2]:
			problems = append(problems, fmt.Sprintf("%s:%d: %s asserts size %s", relative, line, name, size))
		}
		// A typedef may name its tag ahead of the body (often in another
		// header), so the tag's body counts as the type's declaration.
		tag := aliases[typeName]
		if tag == "" {
			tag = strings.TrimSuffix(typeName, "_t")
		}
		if !declaresName(previous, typeName) && !declaresTag(previous, tag) {
			problems = append(problems, fmt.Sprintf("%s:%d: %s does not directly follow the declaration of %s", relative, line, name, typeName))
		}
		previous = nil
	}
	return problems, nil
}

// fileScopeStatementEnd returns the end of the file-scope statement at
// start: its semicolon, or the closing brace of a function body.
func fileScopeStatementEnd(tokens []cToken, start int) int {
	for index := start; index < len(tokens); index++ {
		switch tokens[index].text {
		case ";":
			return index + 1
		case "(", "[", "{":
			close := matchingToken(tokens, index)
			if close < 0 {
				return len(tokens)
			}
			if tokens[index].text == "{" && index > start && tokens[index-1].text == ")" {
				return close + 1
			}
			index = close
		}
	}
	return len(tokens)
}

// sizeAssert recognizes `typedef char NAME[... sizeof(TYPE) == SIZE ...];`.
func sizeAssert(statement []cToken) (name, typeName, size string, ok bool) {
	if len(statement) < 4 || statement[0].text != "typedef" || statement[1].text != "char" || statement[2].kind != cIdent || statement[3].text != "[" {
		return "", "", "", false
	}
	name = statement[2].text
	for index := 4; index+2 < len(statement); index++ {
		if statement[index].text != "sizeof" || statement[index+1].text != "(" {
			continue
		}
		close := matchingToken(statement, index+1)
		if close < 0 {
			return "", "", "", false
		}
		for inner := index + 2; inner < close; inner++ {
			if statement[inner].kind == cIdent && !isTagKeyword(statement[inner].text) {
				typeName = statement[inner].text
			}
		}
		if close+2 < len(statement) && statement[close+1].text == "==" {
			size = statement[close+2].text
		}
		return name, typeName, size, typeName != ""
	}
	return "", "", "", false
}

func isDecimal(text string) bool {
	return text != "" && strings.Trim(text, "0123456789") == ""
}

// typedefTag recognizes `typedef struct TAG NAME;` (or union, enum), which
// names a tag whose body follows later in the file.
func typedefTag(statement []cToken) (string, string, bool) {
	if len(statement) == 5 && statement[0].text == "typedef" && isTagKeyword(statement[1].text) &&
		statement[2].kind == cIdent && statement[3].kind == cIdent && statement[4].text == ";" {
		return statement[3].text, statement[2].text, true
	}
	return "", "", false
}

// sizeAssertNames reports whether an assert's prefix names its type: the
// type without its _t or _e suffix, a PsyQ type's lowercase form (GsOT_TAG as
// gs_ot_tag), or, for a type that ends in _size, the prefix that reads as it
// (sprite_size_must_be_2 for sprite_size_t).
func sizeAssertNames(prefix, typeName string) bool {
	base := strings.TrimSuffix(strings.TrimSuffix(typeName, "_t"), "_e")
	if prefix == base || prefix == typeName || prefix+"_size" == base {
		return true
	}
	return strings.ToLower(typeName) != typeName &&
		strings.ReplaceAll(prefix, "_", "") == strings.ToLower(strings.ReplaceAll(typeName, "_", ""))
}

// declaresName reports whether a file-scope statement declares name as a
// typedef, tag or object outside any braces.
func declaresName(statement []cToken, name string) bool {
	depth := 0
	for _, token := range statement {
		switch token.text {
		case "{":
			depth++
		case "}":
			depth--
		}
		if depth == 0 && token.kind == cIdent && token.text == name {
			return true
		}
	}
	return false
}

// declaresTag reports whether a statement defines the body of struct, union
// or enum tag.
func declaresTag(statement []cToken, tag string) bool {
	for index := 0; index+2 < len(statement); index++ {
		if isTagKeyword(statement[index].text) && statement[index+1].text == tag && statement[index+2].text == "{" {
			return true
		}
	}
	return false
}

// checkSourceConventions runs the convention checks and fails with every
// problem found, grouped by check.
func (p project) checkSourceConventions() error {
	var report []string
	for _, check := range []struct {
		label string
		run   func(string) ([]string, error)
	}{
		{"header layers", checkHeaderLayers},
		{"storage names", checkStorageNames},
		{"size asserts", checkSizeAsserts},
	} {
		problems, err := check.run(p.root)
		if err != nil {
			return fmt.Errorf("%s: %w", check.label, err)
		}
		if len(problems) == 0 {
			continue
		}
		sort.Strings(problems)
		report = append(report, fmt.Sprintf("%s: %d problem(s)", check.label, len(problems)))
		for _, problem := range problems {
			report = append(report, "  "+problem)
		}
	}
	if len(report) > 0 {
		return errors.New(strings.Join(report, "\n"))
	}
	return nil
}
