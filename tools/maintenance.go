// maintenance.go implements check-config and the source inventory check.
package main

import (
	"encoding/hex"
	"errors"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"regexp"
	"strings"
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
	fmt.Printf("  placeholders: no accesses to _unused_ or _padding_ members\n")
	if err := p.checkSourceConventions(); err != nil {
		return err
	}
	fmt.Printf("  conventions: header layers, storage prefixes and size asserts hold\n")
	shards, err := p.checkWorkflowShards(config)
	if err != nil {
		return err
	}
	if shards > 0 {
		fmt.Printf("  workflow: the %d check shards cover every module once\n", shards)
	}
	return nil
}

// workflowFile is the CI workflow whose check job is split into shards.
const workflowFile = ".github/workflows/check.yml"

var workflowShardPattern = regexp.MustCompile(`(?m)^\s*-\s*\{\s*name:\s*[^,}]+,\s*modules:\s*([^}]+?)\s*\}\s*$`)

// checkWorkflowShards fails unless the module lists of the workflow's
// check shards select every configured module exactly once, so CI cannot
// skip a module that the shard list forgot. It returns the shard count, or 0
// when the checkout has no workflow file.
func (p project) checkWorkflowShards(config *projectConfig) (int, error) {
	data, err := os.ReadFile(filepath.Join(p.root, filepath.FromSlash(workflowFile)))
	if errors.Is(err, fs.ErrNotExist) {
		return 0, nil
	}
	if err != nil {
		return 0, err
	}
	matches := workflowShardPattern.FindAllStringSubmatch(string(data), -1)
	if len(matches) == 0 {
		return 0, fmt.Errorf("%s: no check shard of the form `- { name: N, modules: M }` found", workflowFile)
	}
	owner := make(map[*moduleSpec]string)
	for _, match := range matches {
		selection := strings.TrimSpace(match[1])
		modules, err := resolveModuleSelection(config, selection)
		if err != nil {
			return 0, fmt.Errorf("%s: shard %q: %w", workflowFile, selection, err)
		}
		for _, m := range modules {
			if other, taken := owner[m]; taken {
				return 0, fmt.Errorf("%s: module %s is checked by two shards (%q and %q)", workflowFile, m.ID, other, selection)
			}
			owner[m] = selection
		}
	}
	var missing []string
	for _, m := range config.Modules {
		if _, covered := owner[m]; !covered {
			missing = append(missing, m.ID)
		}
	}
	if len(missing) > 0 {
		return 0, fmt.Errorf("%s: %d modules are in no check shard: %s", workflowFile, len(missing), strings.Join(missing, " "))
	}
	return len(matches), nil
}
