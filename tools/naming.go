// naming.go reports public functions whose owner prefix is not the basename
// of the header that declares them, to guide future renames. It never fails.
package main

import (
	"errors"
	"fmt"
	"io"
	"io/fs"
	"os"
	"path/filepath"
	"sort"
	"strings"
)

// namingLibraries are the large libraries whose functions split by
// sub-topic across headers in their folder.
var namingLibraries = map[string]string{
	"menu_": "menu", "gfx_": "gfx", "effect_": "effect", "sprite_": "gfx",
	"map_": "map", "script_": "script", "fade_": "gfx", "text_": "text",
}

// namingSharedOwners are small owners that belong to a larger concept and
// share its header (by header basename).
var namingSharedOwners = map[string][]string{
	"bar":       {"rumor_"},
	"memory":    {"heap_", "mem_"},
	"system":    {"boot_", "loading_message_"},
	"math":      {"random_", "vector_"},
	"shop":      {"shop_fur_", "shop_soldier_"},
	"menu_text": {"menu_text_"},
	"script":    {"scenario_"},
	"location":  {"travel_"},
	"message":   {"announcement_"},
	"command":   {"command_"},
}

type namingMismatch struct {
	header    string
	owner     string
	functions []string
}

// namingMismatches lists, per header, the public functions it declares
// whose names do not start with the header's owner prefix.
func namingMismatches(root string) ([]namingMismatch, error) {
	var found []namingMismatch
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
		folder := filepath.Base(filepath.Dir(path))
		owner := strings.TrimSuffix(filepath.Base(path), ".h")
		data, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		declarations, _, err := scanFileScopeDeclarations(string(data))
		if err != nil {
			return fmt.Errorf("%s: %w", relative, err)
		}
		seen := make(map[string]bool)
		var names []string
		for _, declaration := range declarations {
			name := declaration.symbol
			if declaration.static || !functionDeclarationSignature(declaration.signature) || seen[name] || namingAccepts(folder, owner, name) {
				continue
			}
			seen[name] = true
			names = append(names, name)
		}
		if len(names) > 0 {
			sort.Strings(names)
			found = append(found, namingMismatch{header: relative, owner: owner + "_", functions: names})
		}
		return nil
	})
	if err != nil && !errors.Is(err, fs.ErrNotExist) {
		return nil, err
	}
	return found, nil
}

func namingAccepts(folder, owner, name string) bool {
	if name == owner || strings.HasPrefix(name, owner+"_") || name == "main" || strings.HasSuffix(name, "_entrypoint") {
		return true
	}
	for prefix, library := range namingLibraries {
		if library == folder && strings.HasPrefix(name, prefix) {
			return true
		}
	}
	for _, prefix := range namingSharedOwners[owner] {
		if strings.HasPrefix(name, prefix) {
			return true
		}
	}
	return false
}

// writeNamingReport prints one row per header: its owner prefix, how many
// functions it declares under other prefixes, and those functions grouped
// by their first word.
func writeNamingReport(out io.Writer, mismatches []namingMismatch) {
	sort.Slice(mismatches, func(i, j int) bool {
		if len(mismatches[i].functions) != len(mismatches[j].functions) {
			return len(mismatches[i].functions) > len(mismatches[j].functions)
		}
		return mismatches[i].header < mismatches[j].header
	})
	total := 0
	width := len("header")
	for _, mismatch := range mismatches {
		total += len(mismatch.functions)
		width = max(width, len(mismatch.header))
	}
	fmt.Fprintf(out, "naming: %d public functions in %d headers do not start with their header's owner prefix\n", total, len(mismatches))
	if len(mismatches) == 0 {
		return
	}
	fmt.Fprintf(out, "%-*s  %-24s  %5s  %s\n", width, "header", "owner", "count", "functions by leading word")
	for _, mismatch := range mismatches {
		groups := make(map[string][]string)
		var words []string
		for _, name := range mismatch.functions {
			word, _, _ := strings.Cut(name, "_")
			if groups[word] == nil {
				words = append(words, word)
			}
			groups[word] = append(groups[word], name)
		}
		var cells []string
		for _, word := range words {
			cells = append(cells, strings.Join(groups[word], " "))
		}
		fmt.Fprintf(out, "%-*s  %-24s  %5d  %s\n", width, mismatch.header, mismatch.owner, len(mismatch.functions), strings.Join(cells, " | "))
	}
}

func (p project) namingCommand() error {
	mismatches, err := namingMismatches(p.root)
	if err != nil {
		return err
	}
	writeNamingReport(os.Stdout, mismatches)
	return nil
}
