// placeholders.go keeps placeholder member names honest: `_unused_XX` and
// `_padding_XX` members must never be accessed by name. Code that starts using
// one renames it to `_unknown_XX` or gives it a real name.
package main

import (
	"errors"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"regexp"
	"sort"
)

var unaccessedPlaceholder = regexp.MustCompile(`^_(unused|padding)_[0-9a-f]+(_bit[0-9]+)?(_[a-z])?$`)

type placeholderAccess struct {
	path   string
	line   int
	member string
}

func findPlaceholderAccesses(root string, dirs []string) ([]placeholderAccess, error) {
	var found []placeholderAccess
	for _, dir := range dirs {
		err := filepath.WalkDir(filepath.Join(root, dir), func(path string, entry fs.DirEntry, err error) error {
			if err != nil {
				return err
			}
			if entry.IsDir() || (filepath.Ext(path) != ".c" && filepath.Ext(path) != ".h") {
				return nil
			}
			data, err := os.ReadFile(path)
			if err != nil {
				return err
			}
			relative, err := filepath.Rel(root, path)
			if err != nil {
				return err
			}
			source := string(data)
			tokens, err := tokenizeC(source)
			if err != nil {
				return fmt.Errorf("%s: %w", filepath.ToSlash(relative), err)
			}
			for index := 1; index < len(tokens); index++ {
				previous, token := tokens[index-1], tokens[index]
				if token.kind != cIdent || previous.kind != cPunct || (previous.text != "." && previous.text != "->") {
					continue
				}
				if unaccessedPlaceholder.MatchString(token.text) {
					found = append(found, placeholderAccess{filepath.ToSlash(relative), lineOf(source, token.start), token.text})
				}
			}
			return nil
		})
		if err != nil && !errors.Is(err, fs.ErrNotExist) {
			return nil, err
		}
	}
	sort.Slice(found, func(i, j int) bool {
		if found[i].path != found[j].path {
			return found[i].path < found[j].path
		}
		return found[i].line < found[j].line
	})
	return found, nil
}

// validatePlaceholderMembers fails when code accesses an `_unused_` or
// `_padding_` member by name.
func validatePlaceholderMembers(root string) error {
	found, err := findPlaceholderAccesses(root, declarationScanDirs)
	if err != nil {
		return err
	}
	if len(found) == 0 {
		return nil
	}
	sites := make([]string, len(found))
	for index, access := range found {
		sites[index] = fmt.Sprintf("%s:%d %s", access.path, access.line, access.member)
	}
	return fmt.Errorf("placeholders: %d accesses to _unused_/_padding_ members; rename them to _unknown_XX or a real name: %s",
		len(found), abbreviatedList(sites, 10))
}
