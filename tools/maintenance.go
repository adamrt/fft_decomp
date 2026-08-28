// maintenance.go implements check-config and the source inventory check.
package main

import (
	"encoding/hex"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
)

func validSHA256(value string) bool {
	decoded, err := hex.DecodeString(value)
	return err == nil && len(decoded) == 32
}

func validateSourceInventory(root, relative string, expected map[string]bool, owner string) error {
	base := filepath.Join(root, filepath.FromSlash(relative))
	seen := make(map[string]bool)
	err := filepath.WalkDir(base, func(path string, entry fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if entry.IsDir() || (filepath.Ext(path) != ".c" && filepath.Ext(path) != ".s") {
			return nil
		}
		rel, err := filepath.Rel(root, path)
		if err != nil {
			return err
		}
		name := filepath.ToSlash(rel)
		seen[name] = true
		if !expected[name] {
			return fmt.Errorf("orphan source %s is not in the %s", name, owner)
		}
		return nil
	})
	if err != nil {
		return err
	}
	for name := range expected {
		if seen[name] {
			continue
		}
		if _, err := os.Stat(filepath.Join(root, filepath.FromSlash(name))); err != nil {
			return fmt.Errorf("%s references missing source %s", owner, name)
		}
		return fmt.Errorf("%s references source outside %s: %s", owner, relative, name)
	}
	return nil
}

// checkConfig validates target/*.yaml and the declarations, then prints the
// reconstruction totals.
func (p project) checkConfig() error {
	config, err := p.loadCheckedConfig()
	if err != nil {
		return err
	}
	counts := map[string]int{}
	sizes := map[string]int{}
	functions, total := 0, 0
	for _, m := range config.Modules {
		for _, f := range m.Functions {
			language := "c"
			if f.Asm != "" {
				language = "asm"
			}
			counts[language]++
			sizes[language] += f.Size
			functions++
			total += f.Size
		}
	}
	fmt.Printf("config exact: %d modules, %d functions (%d bytes)\n", len(config.Modules), functions, total)
	for _, language := range []string{"c", "asm"} {
		fmt.Printf("  %s: %d functions (%d bytes)\n", language, counts[language], sizes[language])
	}
	fmt.Printf("  declarations: no conflicting file-scope declarations\n")
	return nil
}
