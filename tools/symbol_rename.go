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
			var definitions map[string]bool
			if filepath.Ext(path) == ".c" {
				definitions, err = sourceDefinedIdentifiers(body)
				if err != nil {
					return fmt.Errorf("%s: %w", path, err)
				}
			}
			current := body
			for index, rename := range renames {
				if definitions[rename.oldName] {
					plan.oldDefinition[index] = append(plan.oldDefinition[index], path)
				}
				if definitions[rename.newName] {
					plan.newDefinition[index] = append(plan.newDefinition[index], path)
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

// sourceDefinedIdentifiers uses the declaration checker's file-scope parser,
// retaining static definitions because a rename can collide with them too.
// Function bodies are skipped, so local names and calls cannot be definitions.
func sourceDefinedIdentifiers(source []byte) (map[string]bool, error) {
	text := string(source)
	tokens, err := tokenizeC(text)
	if err != nil {
		return nil, err
	}
	definitions := make(map[string]bool)
	for index := 0; index < len(tokens); {
		start := index
		if tokens[start].text == "}" {
			return nil, fmt.Errorf("line %d: unexpected file-scope closing brace", lineOf(text, tokens[start].start))
		}
		next, statement, body, err := sourceDefinitionStatement(tokens, index)
		if err != nil {
			return nil, fmt.Errorf("line %d: %w", lineOf(text, tokens[start].start), err)
		}
		index = next
		if len(statement) == 0 {
			// The shared parser skips standalone tag bodies. A trailing object
			// declarator instead needs parsing before its bindings can be trusted.
			for brace := start; brace < next; brace++ {
				if tokens[brace].text == "{" && isTagBody(tokens, start, brace) {
					close := matchingToken(tokens, brace)
					if close+1 < next && tokens[close+1].text != ";" {
						return nil, fmt.Errorf("line %d: cannot parse object declared with a tag body", lineOf(text, tokens[start].start))
					}
					break
				}
			}
			continue
		}
		if statement[0].text == "typedef" || fileScopeAssembly(statement) ||
			len(statement) == 2 && isTagKeyword(statement[0].text) && statement[1].kind == cIdent {
			continue
		}
		// The declaration checker omits static symbols from cross-file comparisons.
		// Strip that storage class here while preserving all declarator tokens.
		filtered := make([]cToken, 0, len(statement))
		external := false
		depth := 0
		for _, token := range statement {
			if depth == 0 && token.text == "static" {
				continue
			}
			if depth == 0 && token.text == "extern" {
				external = true
			}
			filtered = append(filtered, token)
			switch token.text {
			case "(", "[", "{":
				depth++
			case ")", "]", "}":
				depth--
			}
		}
		if len(filtered) == 0 {
			return nil, fmt.Errorf("line %d: cannot parse file-scope declaration", lineOf(text, tokens[start].start))
		}
		parts := splitTopLevel(filtered, ",")
		_, begin, _ := parseDeclarationSpecifier(parts[0])
		for partIndex, part := range parts {
			if len(part) == 0 {
				return nil, fmt.Errorf("line %d: empty file-scope declarator", lineOf(text, tokens[start].start))
			}
			// An extern without a body or initializer cannot define a symbol.
			// Its parameter types need not fit the signature comparison parser.
			if external && !body && topLevelIndex(part, "=") < 0 {
				continue
			}
			declarator := part
			if partIndex > 0 {
				declarator = append(append([]cToken(nil), parts[0][:begin]...), part...)
			}
			if name, architectural, err := sourceArchitecturalRegister(declarator, body); err != nil {
				return nil, fmt.Errorf("line %d: %w", lineOf(text, part[0].start), err)
			} else if architectural {
				// This binds a C name, but no ELF symbol named "$N".
				definitions[name] = true
				continue
			}
			declarations := parseDeclaratorList(declarator, body)
			if len(declarations) != 1 {
				return nil, fmt.Errorf("line %d: cannot parse file-scope declaration", lineOf(text, part[0].start))
			}
			declaration := declarations[0]
			if body || !functionDeclarationSignature(declaration.signature) && (!external || topLevelIndex(part, "=") >= 0) {
				definitions[declaration.symbol] = true
				lexical := part
				if partIndex == 0 {
					lexical = part[begin:]
				}
				if name := boundIdentifier(lexical); name >= 0 {
					definitions[lexical[name].text] = true
				}
			}
		}
	}
	return definitions, nil
}

func sourceArchitecturalRegister(statement []cToken, body bool) (string, bool, error) {
	specifier, begin, _ := parseDeclarationSpecifier(statement)
	registered := false
	for _, token := range statement[:begin] {
		registered = registered || token.text == "register"
	}
	if !registered {
		return "", false, nil
	}
	declarator := statement[begin:]
	alias, signature, ok := parseDeclarationDeclarator(specifier, declarator)
	if !ok || len(alias) == 0 || alias[0] != '$' {
		return "", false, nil
	}
	if body || topLevelIndex(statement, "=") >= 0 || functionDeclarationSignature(signature) || !mipsArchitecturalRegisterName.MatchString(alias) {
		return "", false, errors.New("invalid architectural register declaration")
	}
	name := boundIdentifier(declarator)
	if name < 0 {
		return "", false, errors.New("architectural register has no C binding")
	}
	return declarator[name].text, true, nil
}

// K&R definitions place parameter declarations between the header and body.
// Traverse those declarations as parameters, never as file-scope objects.
func sourceDefinitionStatement(tokens []cToken, start int) (int, []cToken, bool, error) {
	if tokens[start].kind == cIdent && macroInvocationName.MatchString(tokens[start].text) &&
		start+1 < len(tokens) && tokens[start+1].text == "(" {
		return declarationStatement(tokens, start)
	}
	open := start
	for open < len(tokens) && tokens[open].text != "(" {
		if tokens[open].text == ";" || tokens[open].text == "{" || tokens[open].text == "=" {
			return declarationStatement(tokens, start)
		}
		open++
	}
	if open == len(tokens) {
		return declarationStatement(tokens, start)
	}
	close := matchingToken(tokens, open)
	if close < 0 || close+1 >= len(tokens) || tokens[close+1].kind != cIdent {
		return declarationStatement(tokens, start)
	}
	parameters := make(map[string]bool)
	for index := open + 1; index < close; index++ {
		token := tokens[index]
		if (index-open)%2 == 0 {
			if token.text != "," {
				return declarationStatement(tokens, start)
			}
		} else if token.kind != cIdent || declarationBaseWords[token.text] || declarationQualifiers[token.text] {
			return declarationStatement(tokens, start)
		} else {
			parameters[token.text] = true
		}
	}
	if len(parameters) == 0 || (close-open)%2 != 0 {
		return declarationStatement(tokens, start)
	}
	header := tokens[start : close+1]
	withoutStatic := make([]cToken, 0, len(header))
	for _, token := range header {
		if token.text != "static" {
			withoutStatic = append(withoutStatic, token)
		}
	}
	declarations := parseDeclaratorList(withoutStatic, true)
	if len(declarations) != 1 || !functionDeclarationSignature(declarations[0].signature) {
		return declarationStatement(tokens, start)
	}
	for cursor := close + 1; cursor < len(tokens); {
		if tokens[cursor].text == "{" {
			end := matchingToken(tokens, cursor)
			if end < 0 {
				return 0, nil, false, errors.New("unterminated K&R function body")
			}
			return end + 1, header, true, nil
		}
		next, statement, body, err := declarationStatement(tokens, cursor)
		if err != nil {
			return 0, nil, false, err
		}
		declarations := parseDeclaratorList(statement, false)
		if body || len(declarations) == 0 || topLevelIndex(statement, "=") >= 0 {
			return 0, nil, false, errors.New("cannot parse K&R parameter declaration")
		}
		for _, declaration := range declarations {
			if !parameters[declaration.symbol] {
				return 0, nil, false, fmt.Errorf("K&R declaration is not a parameter: %s", declaration.symbol)
			}
		}
		cursor = next
	}
	return 0, nil, false, errors.New("missing K&R function body")
}

// Global assembly has no C binding. Accept the simple string-only form used
// by this project's assembler mode directives; other forms remain parse errors.
func fileScopeAssembly(statement []cToken) bool {
	if len(statement) < 4 || statement[0].text != "asm" && statement[0].text != "__asm" && statement[0].text != "__asm__" {
		return false
	}
	open := 1
	if statement[open].text == "volatile" || statement[open].text == "__volatile__" {
		open++
	}
	if open >= len(statement) || statement[open].text != "(" || matchingToken(statement, open) != len(statement)-1 {
		return false
	}
	for _, token := range statement[open+1 : len(statement)-1] {
		if token.kind != cString {
			return false
		}
	}
	return len(statement) > open+2
}

func functionDeclarationSignature(signature []string) bool {
	for index, segment := range signature {
		if segment != "@" {
			continue
		}
		left, right := index, index+1
		for {
			if right < len(signature) && isParameterSegment(signature[right]) {
				return true
			}
			if left == 0 || right >= len(signature) || signature[left-1] != "(" || signature[right] != ")" {
				return false
			}
			left--
			right++
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
// definition rename also moves its source file to <dir>/<new>.c.
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
				if from != "" {
					moves = append(moves, move{from, to})
				}
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

// renameConfigFunction keeps entry-only renames separate from source-definition
// renames. A definition rename updates every owner and moves its shared source.
func renameConfigFunction(config *projectConfig, oldName, newName string) (string, string, error) {
	from, to := "", ""
	found := false
	for _, m := range config.Modules {
		if m.function(newName) != nil {
			return "", "", fmt.Errorf("function already exists: %s", newName)
		}
		for _, symbol := range append(append([]symbolSpec(nil), m.Data...), m.Imports...) {
			if symbol.Name == newName {
				return "", "", fmt.Errorf("%s already names a symbol in %s", newName, m.ID)
			}
		}
		for index := range m.Functions {
			f := &m.Functions[index]
			if f.Name == oldName {
				found = true
			}
			if f.sourceSymbol() == oldName {
				found = true
				source := m.sourceOf(f)
				if from != "" && source != from {
					return "", "", fmt.Errorf("%s has different sources in different modules; rename it manually", oldName)
				}
				from = source
				to = filepath.ToSlash(filepath.Join(filepath.Dir(source), newName+filepath.Ext(source)))
			}
		}
	}
	if !found {
		return "", "", fmt.Errorf("no function or source symbol named %s", oldName)
	}
	// Check all owners before changing any entry or implicit default path.
	if from != "" {
		for _, m := range config.Modules {
			for index := range m.Functions {
				f := &m.Functions[index]
				if m.sourceOf(f) == from && f.sourceSymbol() != oldName {
					return "", "", fmt.Errorf("source %s has conflicting definition symbols; rename it manually", from)
				}
			}
		}
	}
	for _, m := range config.Modules {
		for index := range m.Functions {
			f := &m.Functions[index]
			source := m.sourceOf(f)
			definition := f.sourceSymbol()
			entryRename := f.Name == oldName
			if entryRename {
				f.Name = newName
				// An alias's old default path remains owned by its definition.
				f.Source = source
			}
			if definition == oldName {
				f.SourceSymbol = newName
			}
			if from != "" && source == from {
				f.Source = to
			}
		}
		for index := range m.Overrides {
			if m.Overrides[index].Function == oldName {
				m.Overrides[index].Function = newName
			}
		}
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
	return rewriteCodeOutsideIncludes(source, func(code []byte) []byte { return pattern.ReplaceAll(code, replacement) })
}

// rewriteCodeOutsideIncludes applies rewrite to every run of code text,
// passing include lines, comments and literals through unchanged.
func rewriteCodeOutsideIncludes(source []byte, rewrite func([]byte) []byte) []byte {
	var output []byte
	code := make([]byte, 0, len(source))
	flush := func() {
		if len(code) > 0 {
			output = append(output, rewrite(code)...)
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
