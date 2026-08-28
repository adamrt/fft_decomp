// symbol_rename.go renames functions and data symbols across target/*.yaml
// and the sources in one all-or-nothing file transaction.
package main

import (
	"bytes"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"reflect"
	"regexp"
	"sort"
	"strconv"
	"strings"
)

type symbolMutationResult struct {
	ChangedFiles int
}

type symbolFileChange struct {
	path           string
	original       []byte
	replacement    []byte
	originalExists bool
	replace        bool
	mode           os.FileMode
}

type symbolFileTransaction struct {
	changes map[string]symbolFileChange
}

func newSymbolFileTransaction() *symbolFileTransaction {
	return &symbolFileTransaction{changes: make(map[string]symbolFileChange)}
}

func (transaction *symbolFileTransaction) addExistingSnapshot(path string, original, replacement []byte, mode os.FileMode) error {
	if bytes.Equal(original, replacement) {
		return nil
	}
	return transaction.add(symbolFileChange{
		path: path, original: original, replacement: replacement,
		originalExists: true, replace: true, mode: mode.Perm(),
	})
}

func (transaction *symbolFileTransaction) addRemovalSnapshot(path string, original []byte, mode os.FileMode) error {
	return transaction.add(symbolFileChange{path: path, original: original, originalExists: true, mode: mode.Perm()})
}

func (transaction *symbolFileTransaction) addCreation(path string, replacement []byte, mode os.FileMode) error {
	if _, err := os.Lstat(path); err == nil {
		return fmt.Errorf("destination already exists: %s", path)
	} else if !errors.Is(err, os.ErrNotExist) {
		return err
	}
	if info, err := os.Stat(filepath.Dir(path)); err != nil {
		return err
	} else if !info.IsDir() {
		return fmt.Errorf("missing target directory: %s", filepath.Dir(path))
	}
	return transaction.add(symbolFileChange{path: path, replacement: replacement, replace: true, mode: mode.Perm()})
}

func (transaction *symbolFileTransaction) add(change symbolFileChange) error {
	if _, exists := transaction.changes[change.path]; exists {
		return fmt.Errorf("duplicate planned file change: %s", change.path)
	}
	transaction.changes[change.path] = change
	return nil
}

func readRegularSymbolFile(path string) (os.FileInfo, []byte, error) {
	info, err := os.Lstat(path)
	if err != nil {
		return nil, nil, err
	}
	if info.Mode()&os.ModeSymlink != 0 || !info.Mode().IsRegular() {
		return nil, nil, fmt.Errorf("refusing nonregular target: %s", path)
	}
	data, err := os.ReadFile(path)
	return info, data, err
}

func (transaction *symbolFileTransaction) validateCurrent() error {
	for _, path := range transaction.paths() {
		change := transaction.changes[path]
		info, err := os.Lstat(path)
		if !change.originalExists {
			if err == nil {
				return fmt.Errorf("%s changed on disk", path)
			}
			if !errors.Is(err, os.ErrNotExist) {
				return err
			}
			continue
		}
		if err != nil {
			return fmt.Errorf("%s changed on disk: %w", path, err)
		}
		if info.Mode()&os.ModeSymlink != 0 || !info.Mode().IsRegular() {
			return fmt.Errorf("%s changed on disk", path)
		}
		current, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		if !bytes.Equal(current, change.original) {
			return fmt.Errorf("%s changed on disk", path)
		}
	}
	return nil
}

func (transaction *symbolFileTransaction) paths() []string {
	paths := make([]string, 0, len(transaction.changes))
	for path := range transaction.changes {
		paths = append(paths, path)
	}
	sort.Strings(paths)
	return paths
}

type stagedSymbolChange struct {
	change      symbolFileChange
	replacement string
	backup      string
	published   bool
}

func (transaction *symbolFileTransaction) commit() error {
	return transaction.commitWithRename(os.Rename)
}

func (transaction *symbolFileTransaction) commitWithRename(renameFile func(string, string) error) (err error) {
	if err := transaction.validateCurrent(); err != nil {
		return err
	}
	staged := make([]stagedSymbolChange, 0, len(transaction.changes))
	defer func() {
		for _, item := range staged {
			if item.replacement != "" {
				_ = os.Remove(item.replacement)
			}
			if item.backup != "" {
				_ = os.Remove(item.backup)
			}
		}
	}()
	for _, path := range transaction.paths() {
		change := transaction.changes[path]
		staged = append(staged, stagedSymbolChange{change: change})
		item := &staged[len(staged)-1]
		if change.replace {
			file, createErr := os.CreateTemp(filepath.Dir(path), ".symbol-change-*.tmp")
			if createErr != nil {
				return createErr
			}
			item.replacement = file.Name()
			if chmodErr := file.Chmod(change.mode); chmodErr != nil {
				file.Close()
				return chmodErr
			}
			if _, writeErr := file.Write(change.replacement); writeErr != nil {
				file.Close()
				return writeErr
			}
			if syncErr := file.Sync(); syncErr != nil {
				file.Close()
				return syncErr
			}
			if closeErr := file.Close(); closeErr != nil {
				return closeErr
			}
		}
	}
	if err := transaction.validateCurrent(); err != nil {
		return err
	}

	rollback := func(last int) error {
		var rollbackErr error
		for index := last; index >= 0; index-- {
			item := &staged[index]
			if item.published {
				rollbackErr = errors.Join(rollbackErr, os.Remove(item.change.path))
				item.published = false
			}
			if item.backup != "" {
				rollbackErr = errors.Join(rollbackErr, os.Rename(item.backup, item.change.path))
				item.backup = ""
			}
		}
		return rollbackErr
	}
	for index := range staged {
		item := &staged[index]
		if item.change.originalExists {
			backup, createErr := os.CreateTemp(filepath.Dir(item.change.path), ".symbol-backup-*.tmp")
			if createErr != nil {
				return errors.Join(createErr, rollback(index-1))
			}
			item.backup = backup.Name()
			if closeErr := backup.Close(); closeErr != nil {
				return errors.Join(closeErr, rollback(index-1))
			}
			if removeErr := os.Remove(item.backup); removeErr != nil {
				return errors.Join(removeErr, rollback(index-1))
			}
			if renameErr := renameFile(item.change.path, item.backup); renameErr != nil {
				return errors.Join(renameErr, rollback(index-1))
			}
		}
		if item.change.replace {
			if renameErr := renameFile(item.replacement, item.change.path); renameErr != nil {
				return errors.Join(renameErr, rollback(index))
			}
			item.replacement = ""
			item.published = true
		}
	}
	for index := range staged {
		if staged[index].backup != "" {
			if removeErr := os.Remove(staged[index].backup); removeErr != nil {
				return removeErr
			}
			staged[index].backup = ""
		}
	}
	return nil
}

func validateRenameIdentifier(value string) error {
	if !validSymbol(value) {
		return fmt.Errorf("invalid C identifier: %s", value)
	}
	return nil
}

var sourceExtensions = map[string]bool{".c": true, ".h": true, ".ld": true, ".s": true}

type sourceTextChange struct {
	original    []byte
	replacement []byte
	mode        os.FileMode
}

type sourceIdentifierRename struct {
	oldName, newName string
}

type sourceIdentifierBatchPlan struct {
	changes       map[string]sourceTextChange
	references    []map[string]bool
	oldDefinition [][]string
	newDefinition [][]string
}

func planSourceIdentifierRenames(root string, renames []sourceIdentifierRename) (sourceIdentifierBatchPlan, error) {
	plan := sourceIdentifierBatchPlan{
		changes:       make(map[string]sourceTextChange),
		references:    make([]map[string]bool, len(renames)),
		oldDefinition: make([][]string, len(renames)),
		newDefinition: make([][]string, len(renames)),
	}
	patterns := make([]*regexp.Regexp, len(renames))
	for index, rename := range renames {
		plan.references[index] = make(map[string]bool)
		patterns[index] = regexp.MustCompile(`\b` + regexp.QuoteMeta(rename.oldName) + `\b`)
	}
	for _, directory := range []string{"src", "include"} {
		base := filepath.Join(root, directory)
		err := filepath.WalkDir(base, func(path string, entry os.DirEntry, walkErr error) error {
			if walkErr != nil {
				return walkErr
			}
			if entry.IsDir() || !sourceExtensions[filepath.Ext(path)] {
				return nil
			}
			info, body, err := readRegularSymbolFile(path)
			if err != nil {
				if entry.Type()&os.ModeSymlink != 0 {
					return fmt.Errorf("refusing source symlink: %s", path)
				}
				return err
			}
			current := body
			for index, rename := range renames {
				if filepath.Ext(path) == ".c" {
					if bytes.Contains(body, []byte(rename.oldName)) && sourceDefinesIdentifier(body, rename.oldName) {
						plan.oldDefinition[index] = append(plan.oldDefinition[index], path)
					}
					if bytes.Contains(body, []byte(rename.newName)) && sourceDefinesIdentifier(body, rename.newName) {
						plan.newDefinition[index] = append(plan.newDefinition[index], path)
					}
				}
				if !bytes.Contains(current, []byte(rename.oldName)) {
					continue
				}
				after := replaceIdentifierOutsideIncludes(patterns[index], current, []byte(rename.newName))
				if bytes.Equal(current, after) {
					continue
				}
				if filepath.Ext(path) == ".ld" {
					updated, err := renameLinkerIdentifier(current, rename.oldName, rename.newName)
					if err != nil {
						return fmt.Errorf("%s: %w", path, err)
					}
					after = updated
				}
				plan.references[index][path] = true
				current = after
			}
			if !bytes.Equal(body, current) {
				plan.changes[path] = sourceTextChange{original: body, replacement: current, mode: info.Mode()}
			}
			return nil
		})
		if err != nil {
			if errors.Is(err, os.ErrNotExist) {
				continue
			}
			return sourceIdentifierBatchPlan{}, err
		}
	}
	for index, rename := range renames {
		if len(plan.oldDefinition[index]) != 0 && len(plan.newDefinition[index]) != 0 {
			return sourceIdentifierBatchPlan{}, fmt.Errorf("source definitions collide: %s and %s", rename.oldName, rename.newName)
		}
	}
	return plan, nil
}

func sourceDefinesIdentifier(source []byte, name string) bool {
	function := regexp.MustCompile(`\b` + regexp.QuoteMeta(name) + `\s*\([^;{}]*\)\s*\{`)
	if function.Match(source) {
		return true
	}
	variable := regexp.MustCompile(`(?m)^[ \t]*(?:[A-Za-z_][A-Za-z0-9_]*[ \t*]+)+` + regexp.QuoteMeta(name) + `\s*(?:\[[^\]]*\]\s*)*(?:=|;)`)
	for _, match := range variable.FindAll(source, -1) {
		trimmed := strings.TrimSpace(string(match))
		if !strings.HasPrefix(trimmed, "extern ") && !strings.HasPrefix(trimmed, "typedef ") && !strings.HasPrefix(trimmed, "return ") && !strings.HasPrefix(trimmed, "goto ") {
			return true
		}
	}
	return false
}

func renameLinkerIdentifier(source []byte, oldName, newName string) ([]byte, error) {
	binding := regexp.MustCompile(`(?m)^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(0x[0-9a-fA-F]+)\s*;\s*$`)
	addresses := map[string]map[uint64]bool{oldName: {}, newName: {}}
	for _, match := range binding.FindAllSubmatch(source, -1) {
		name := string(match[1])
		if _, wanted := addresses[name]; !wanted {
			continue
		}
		value, _ := strconv.ParseUint(string(match[2]), 0, 64)
		addresses[name][value] = true
	}
	if len(addresses[oldName]) != 0 && len(addresses[newName]) != 0 && !reflect.DeepEqual(addresses[oldName], addresses[newName]) {
		return nil, errors.New("linker alias collision")
	}
	result := source
	if len(addresses[oldName]) != 0 && len(addresses[newName]) != 0 {
		oldBinding := regexp.MustCompile(`(?m)^\s*` + regexp.QuoteMeta(oldName) + `\s*=\s*0x[0-9a-fA-F]+\s*;\s*\n?`)
		result = oldBinding.ReplaceAll(result, nil)
	}
	return regexp.MustCompile(`\b`+regexp.QuoteMeta(oldName)+`\b`).ReplaceAll(result, []byte(newName)), nil
}

type symbolRename struct {
	oldName, newName string
}

// renameSymbols renames functions (function true) or data symbols across the
// configuration and every source under src/ and include/. A function
// rename also moves its source file to <dir>/<new>.c.
func (p project) renameSymbols(renames []symbolRename, function, dryRun bool) (result symbolMutationResult, err error) {
	seen := make(map[string]bool)
	for _, rename := range renames {
		if err := validateRenameIdentifier(rename.oldName); err != nil {
			return result, err
		}
		if err := validateRenameIdentifier(rename.newName); err != nil {
			return result, err
		}
		if rename.oldName == rename.newName || seen[rename.oldName] || seen[rename.newName] {
			return result, fmt.Errorf("renames must be distinct and change the name: %s -> %s", rename.oldName, rename.newName)
		}
		seen[rename.oldName], seen[rename.newName] = true, true
	}
	err = p.withTargetLock(func() error {
		config, err := p.loadProjectConfig()
		if err != nil {
			return err
		}
		type move struct{ from, to string }
		var moves []move
		for _, rename := range renames {
			if function {
				from, to, err := renameConfigFunction(config, rename.oldName, rename.newName)
				if err != nil {
					return err
				}
				moves = append(moves, move{from, to})
			} else if err := renameConfigData(config, rename.oldName, rename.newName); err != nil {
				return err
			}
		}
		identifierRenames := make([]sourceIdentifierRename, len(renames))
		for index, rename := range renames {
			identifierRenames[index] = sourceIdentifierRename{rename.oldName, rename.newName}
		}
		sources, err := planSourceIdentifierRenames(p.root, identifierRenames)
		if err != nil {
			return err
		}
		for index := range renames {
			if function && len(sources.newDefinition[index]) != 0 {
				return fmt.Errorf("function definition already exists: %s", sources.newDefinition[index][0])
			}
		}
		transaction := newSymbolFileTransaction()
		for path, data := range renderConfigFiles(config) {
			full := filepath.Join(p.root, filepath.FromSlash(path))
			info, original, err := readRegularSymbolFile(full)
			if err != nil {
				return err
			}
			if err := transaction.addExistingSnapshot(full, original, data, info.Mode()); err != nil {
				return err
			}
		}
		for _, item := range moves {
			fromPath := filepath.Join(p.root, filepath.FromSlash(item.from))
			toPath := filepath.Join(p.root, filepath.FromSlash(item.to))
			change, ok := sources.changes[fromPath]
			if !ok {
				info, original, err := readRegularSymbolFile(fromPath)
				if err != nil {
					return err
				}
				change = sourceTextChange{original: original, replacement: original, mode: info.Mode()}
			}
			delete(sources.changes, fromPath)
			if err := transaction.addRemovalSnapshot(fromPath, change.original, change.mode); err != nil {
				return err
			}
			if err := transaction.addCreation(toPath, change.replacement, change.mode); err != nil {
				return err
			}
		}
		for path, change := range sources.changes {
			if err := transaction.addExistingSnapshot(path, change.original, change.replacement, change.mode); err != nil {
				return err
			}
		}
		result.ChangedFiles = len(transaction.changes)
		if dryRun {
			return nil
		}
		return transaction.commit()
	})
	return result, err
}

// renameConfigFunction renames every module's function oldName (a shared
// source can back one function in several modules) and returns the source
// move it implies.
func renameConfigFunction(config *projectConfig, oldName, newName string) (string, string, error) {
	from, to := "", ""
	for _, m := range config.Modules {
		if m.function(newName) != nil {
			return "", "", fmt.Errorf("function already exists: %s", newName)
		}
		for _, symbol := range append(append([]symbolSpec(nil), m.Data...), m.Imports...) {
			if symbol.Name == newName {
				return "", "", fmt.Errorf("%s already names a symbol in %s", newName, m.ID)
			}
		}
	}
	for _, m := range config.Modules {
		f := m.function(oldName)
		if f == nil {
			continue
		}
		source := m.sourceOf(f)
		if from != "" && source != from {
			return "", "", fmt.Errorf("%s has different sources in different modules; rename it manually", oldName)
		}
		for _, other := range m.Functions {
			if other.Name != oldName && m.sourceOf(&other) == source {
				return "", "", fmt.Errorf("source %s owns several functions; rename it manually", source)
			}
		}
		from = source
		to = filepath.ToSlash(filepath.Join(filepath.Dir(source), newName+filepath.Ext(source)))
		f.Name = newName
		if f.Source != "" {
			f.Source = to
		}
		for index := range m.Overrides {
			if m.Overrides[index].Function == oldName {
				m.Overrides[index].Function = newName
			}
		}
	}
	if from == "" {
		return "", "", fmt.Errorf("no function named %s", oldName)
	}
	renameConfigReferences(config, oldName, newName)
	return from, to, nil
}

// renameConfigData renames data rows, imports and override bindings. A row
// that meets an existing row of the new name at the same address merges.
func renameConfigData(config *projectConfig, oldName, newName string) error {
	found := false
	for _, m := range config.Modules {
		if m.function(oldName) != nil {
			return fmt.Errorf("%s is a function in %s; use rename-function", oldName, m.ID)
		}
		if m.function(newName) != nil {
			return fmt.Errorf("%s is a function in %s", newName, m.ID)
		}
		for _, list := range []*[]symbolSpec{&m.Data, &m.Imports} {
			for _, symbol := range *list {
				if symbol.Name == oldName {
					found = true
				}
			}
		}
		for _, override := range m.Overrides {
			if override.Name == oldName {
				found = true
			}
		}
	}
	if !found {
		return fmt.Errorf("no data symbol named %s", oldName)
	}
	return renameConfigReferences(config, oldName, newName)
}

func renameConfigReferences(config *projectConfig, oldName, newName string) error {
	for _, m := range config.Modules {
		for _, list := range []*[]symbolSpec{&m.Data, &m.Imports} {
			existing := make(map[string]uint32)
			for _, symbol := range *list {
				existing[symbol.Name] = symbol.Addr
			}
			kept := (*list)[:0]
			for _, symbol := range *list {
				if symbol.Name == oldName {
					if address, ok := existing[newName]; ok {
						if address != symbol.Addr {
							return fmt.Errorf("%s: %s and %s have different addresses", m.ID, oldName, newName)
						}
						continue
					}
					symbol.Name = newName
				}
				kept = append(kept, symbol)
			}
			*list = kept
		}
		for index := range m.Overrides {
			if m.Overrides[index].Name == oldName {
				m.Overrides[index].Name = newName
			}
		}
	}
	return nil
}

var includeDirectiveLine = regexp.MustCompile(`^[ \t]*#[ \t]*include\b`)

// replaceIdentifierOutsideIncludes renames identifier occurrences in code
// only: never on #include lines (a function named like a header, main_startup
// and fft/main_startup.h, must not rewrite the include path) and never inside
// a comment or a literal, where a symbol that is also an English word (load,
// build, poll, main) would rewrite the prose.
func replaceIdentifierOutsideIncludes(pattern *regexp.Regexp, source, replacement []byte) []byte {
	var output []byte
	code := make([]byte, 0, len(source))
	flush := func() {
		if len(code) > 0 {
			output = append(output, pattern.ReplaceAll(code, replacement)...)
			code = code[:0]
		}
	}
	for index := 0; index < len(source); {
		if lineStart(source, index) && includeDirectiveLine.Match(source[index:]) {
			flush()
			stop := bytes.IndexByte(source[index:], '\n')
			if stop < 0 {
				output = append(output, source[index:]...)
				break
			}
			output = append(output, source[index:index+stop+1]...)
			index += stop + 1
			continue
		}
		switch {
		case bytes.HasPrefix(source[index:], []byte("/*")):
			flush()
			stop := bytes.Index(source[index+2:], []byte("*/"))
			if stop < 0 {
				output = append(output, source[index:]...)
				index = len(source)
				continue
			}
			output = append(output, source[index:index+2+stop+2]...)
			index += 2 + stop + 2
		case bytes.HasPrefix(source[index:], []byte("//")):
			flush()
			stop := bytes.IndexByte(source[index:], '\n')
			if stop < 0 {
				output = append(output, source[index:]...)
				index = len(source)
				continue
			}
			output = append(output, source[index:index+stop]...)
			index += stop
		case source[index] == '"' || source[index] == '\'':
			flush()
			quote := source[index]
			stop := index + 1
			for stop < len(source) && source[stop] != quote {
				if source[stop] == '\\' {
					stop++
				}
				stop++
			}
			if stop < len(source) {
				stop++
			}
			output = append(output, source[index:stop]...)
			index = stop
		default:
			code = append(code, source[index])
			index++
		}
	}
	flush()
	return output
}

// lineStart reports whether index is at the beginning of a line.
func lineStart(source []byte, index int) bool {
	return index == 0 || source[index-1] == '\n'
}
