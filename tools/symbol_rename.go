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
	Notes        []string // what a rename deliberately left under the old name
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

// addCreationOverRemoval creates path, or, when the transaction already
// removes it (a batch moving b.c to c.c and then a.c to b.c), replaces it.
func (transaction *symbolFileTransaction) addCreationOverRemoval(path string, replacement []byte, mode os.FileMode) error {
	if change, exists := transaction.changes[path]; exists && change.originalExists && !change.replace {
		change.replace, change.replacement, change.mode = true, replacement, mode.Perm()
		transaction.changes[path] = change
		return nil
	}
	return transaction.addCreation(path, replacement, mode)
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

// documentFiles are the project documents whose symbol mentions follow a
// rename, so the prose keeps pointing at code that exists.
var documentFiles = []string{"README.md", "AGENTS.md", "QUIRKS.md", "ARCHAEOLOGY.md"}

// renameDocumentIdentifier rewrites a symbol name inside backticks: the name
// alone, or followed by a call, index or member suffix (`name(`, `name[`,
// `name.`). Prose outside backticks is left alone.
func renameDocumentIdentifier(source []byte, oldName, newName string) []byte {
	pattern := regexp.MustCompile("`" + regexp.QuoteMeta(oldName) + "([`(\\[.])")
	return pattern.ReplaceAll(source, []byte("`"+newName+"${1}"))
}

// proseMentionPath matches what follows a name that is part of a path:
// a file extension or a directory separator.
var proseMentionPath = regexp.MustCompile(`^(\.(c|h|s|ld|o|md|yaml)\b|/)`)

// renameProseIdentifier rewrites whole-word mentions of a name in comments and
// documents. Only a name with an underscore is rewritten outside backticks,
// since load, build or main are also English words; a mention inside a path
// (src/x/name.c, fft/name.h) is left alone.
func renameProseIdentifier(text []byte, oldName, newName string) []byte {
	if !strings.Contains(oldName, "_") {
		return renameDocumentIdentifier(text, oldName, newName)
	}
	if !bytes.Contains(text, []byte(oldName)) {
		return text
	}
	pattern := regexp.MustCompile(`\b` + regexp.QuoteMeta(oldName) + `\b`)
	var output []byte
	last := 0
	for _, match := range pattern.FindAllIndex(text, -1) {
		if match[0] > 0 && text[match[0]-1] == '/' || proseMentionPath.Match(text[match[1]:]) {
			continue
		}
		output = append(output, text[last:match[0]]...)
		output = append(output, newName...)
		last = match[1]
	}
	if last == 0 {
		return text
	}
	return append(output, text[last:]...)
}

type sourceTextChange struct {
	original    []byte
	replacement []byte
	mode        os.FileMode
}

type sourceIdentifierRename struct {
	oldName, newName string
}

// sourceRenameScope limits where a rename applies. Modules, when set, names
// the programs it covers: the sources only those programs compile, and the
// headers once no other program's source still uses the old name.
// SkipStatics, without modules, keeps a file's static definition of the old
// name, and that file's references, when another file defines it publicly.
type sourceRenameScope struct {
	modules     map[string]bool
	fileModules map[string]map[string]bool // absolute source path -> modules compiling it
	skipStatics bool
}

type sourceIdentifierBatchPlan struct {
	changes       map[string]sourceTextChange
	references    []map[string]bool
	oldDefinition [][]string
	newDefinition [][]string
	kept          []map[string]bool // per rename: sources that keep the old name
	notes         []string          // what the plan left alone, for the report
}

func planSourceIdentifierRenames(root string, renames []sourceIdentifierRename) (sourceIdentifierBatchPlan, error) {
	return planSourceIdentifierRenamesScoped(root, renames, false, nil, nil)
}

// planSourceIdentifierRenamesUnify is planSourceIdentifierRenames that, when
// unify is set, lets the old and new definitions coexist only in disjoint
// program views. Views records the source bindings before any file moves.
func planSourceIdentifierRenamesUnify(root string, renames []sourceIdentifierRename, unify bool, views map[string][]*moduleSpec) (sourceIdentifierBatchPlan, error) {
	return planSourceIdentifierRenamesScoped(root, renames, unify, views, nil)
}

type plannedSourceFile struct {
	path          string
	mode          os.FileMode
	body, current []byte
	parsed        bool
	defined       map[string]bool // file-scope definitions, static included
	public        map[string]bool // the non-static ones
}

func (file *plannedSourceFile) definitions() error {
	if file.parsed || filepath.Ext(file.path) != ".c" {
		return nil
	}
	defined, public, err := sourceDefinitions(file.current)
	if err != nil {
		return fmt.Errorf("%s: %w", file.path, err)
	}
	file.defined, file.public, file.parsed = defined, public, true
	return nil
}

// planSourceIdentifierRenamesScoped applies the renames in order, so a name
// one pair frees may be taken by a later pair. Code is renamed outside include
// lines, comments and literals. Comments and documents follow too, except for
// an old name that is also a new name in the batch: its mentions may already
// mean the new symbol.
func planSourceIdentifierRenamesScoped(root string, renames []sourceIdentifierRename, unify bool, views map[string][]*moduleSpec, scope *sourceRenameScope) (sourceIdentifierBatchPlan, error) {
	plan := sourceIdentifierBatchPlan{
		changes:       make(map[string]sourceTextChange),
		references:    make([]map[string]bool, len(renames)),
		oldDefinition: make([][]string, len(renames)),
		newDefinition: make([][]string, len(renames)),
		kept:          make([]map[string]bool, len(renames)),
	}
	var files []*plannedSourceFile
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
			file := &plannedSourceFile{path: path, mode: info.Mode(), body: body, current: body}
			if err := file.definitions(); err != nil {
				return err
			}
			files = append(files, file)
			return nil
		})
		if err != nil {
			if errors.Is(err, os.ErrNotExist) {
				continue
			}
			return sourceIdentifierBatchPlan{}, err
		}
	}
	newNames := make(map[string]bool)
	for _, rename := range renames {
		newNames[rename.newName] = true
	}
	scoped := scope != nil && scope.modules != nil
	blocked := make([]bool, len(renames))
	for index, rename := range renames {
		plan.references[index] = make(map[string]bool)
		plan.kept[index] = make(map[string]bool)
		pattern := regexp.MustCompile(`\b` + regexp.QuoteMeta(rename.oldName) + `\b`)
		code := func(text []byte) []byte { return pattern.ReplaceAll(text, []byte(rename.newName)) }
		comment := func(text []byte) []byte { return text }
		if !newNames[rename.oldName] {
			comment = func(text []byte) []byte { return renameProseIdentifier(text, rename.oldName, rename.newName) }
		}
		usesOld := func(file *plannedSourceFile) bool {
			if !bytes.Contains(file.current, []byte(rename.oldName)) {
				return false
			}
			found := false
			rewriteCodeOutsideIncludes(file.current, func(text []byte) []byte {
				found = found || pattern.Match(text)
				return text
			})
			return found
		}
		rewrite := func(file *plannedSourceFile) error {
			if !bytes.Contains(file.current, []byte(rename.oldName)) {
				return nil
			}
			var after []byte
			if filepath.Ext(file.path) == ".ld" {
				updated, err := renameLinkerIdentifier(file.current, rename.oldName, rename.newName)
				if err != nil {
					return fmt.Errorf("%s: %w", file.path, err)
				}
				after = updated
			} else {
				after = rewriteSourceText(file.current, code, comment)
			}
			if bytes.Equal(after, file.current) {
				return nil
			}
			if filepath.Ext(file.path) == ".ld" || usesOld(file) {
				plan.references[index][file.path] = true
			}
			file.current, file.parsed = after, false
			return nil
		}
		publicElsewhere := false
		for _, file := range files {
			if err := file.definitions(); err != nil {
				return sourceIdentifierBatchPlan{}, err
			}
			if file.defined[rename.newName] {
				plan.newDefinition[index] = append(plan.newDefinition[index], file.path)
			}
			publicElsewhere = publicElsewhere || file.public[rename.oldName]
		}
		var headers []*plannedSourceFile
		for _, file := range files {
			if filepath.Ext(file.path) == ".h" {
				headers = append(headers, file)
				continue
			}
			keep := false
			if scoped {
				owners := scope.fileModules[file.path]
				keep = len(owners) == 0
				for module := range owners {
					keep = keep || !scope.modules[module]
				}
			} else if scope != nil && scope.skipStatics && publicElsewhere && file.defined[rename.oldName] && !file.public[rename.oldName] {
				keep = true
				plan.notes = append(plan.notes, fmt.Sprintf("kept static %s in %s", rename.oldName, file.path))
			}
			if keep {
				if !usesOld(file) {
					continue
				}
				plan.kept[index][file.path] = true
				if !scoped {
					continue
				}
				var selected, others []string
				for module := range scope.fileModules[file.path] {
					if scope.modules[module] {
						selected = append(selected, module)
					} else {
						others = append(others, module)
					}
				}
				if len(selected) != 0 {
					sort.Strings(selected)
					sort.Strings(others)
					return sourceIdentifierBatchPlan{}, fmt.Errorf("%s uses %s and is compiled by %s and also %s; select those modules too or edit it by hand",
						file.path, rename.oldName, strings.Join(selected, ","), strings.Join(others, ","))
				}
				blocked[index] = true
				continue
			}
			if file.defined[rename.oldName] {
				plan.oldDefinition[index] = append(plan.oldDefinition[index], file.path)
			}
			if err := rewrite(file); err != nil {
				return sourceIdentifierBatchPlan{}, err
			}
		}
		for _, file := range headers {
			if blocked[index] {
				if usesOld(file) {
					plan.notes = append(plan.notes, fmt.Sprintf("left %s in %s: unselected programs still use it", rename.oldName, file.path))
				}
				continue
			}
			if err := rewrite(file); err != nil {
				return sourceIdentifierBatchPlan{}, err
			}
		}
	}
	for _, file := range files {
		if !bytes.Equal(file.body, file.current) {
			plan.changes[file.path] = sourceTextChange{original: file.body, replacement: file.current, mode: file.mode}
		}
	}
	for _, name := range documentFiles {
		path := filepath.Join(root, name)
		info, body, err := readRegularSymbolFile(path)
		if err != nil {
			if errors.Is(err, os.ErrNotExist) {
				continue
			}
			return sourceIdentifierBatchPlan{}, err
		}
		current := body
		for index, rename := range renames {
			if !blocked[index] && !newNames[rename.oldName] {
				current = renameProseIdentifier(current, rename.oldName, rename.newName)
			}
		}
		if !bytes.Equal(body, current) {
			plan.changes[path] = sourceTextChange{original: body, replacement: current, mode: info.Mode()}
		}
	}
	for index, rename := range renames {
		if len(plan.oldDefinition[index]) == 0 || len(plan.newDefinition[index]) == 0 {
			continue
		}
		if unify {
			if err := validateUnifiedSourceDefinitions(rename, plan.oldDefinition[index], plan.newDefinition[index], views); err != nil {
				return sourceIdentifierBatchPlan{}, err
			}
			continue
		}
		return sourceIdentifierBatchPlan{}, fmt.Errorf("source definitions collide: %s and %s (--join gives same-role bodies of separate programs one name)", rename.oldName, rename.newName)
	}
	return plan, nil
}

// validateUnifiedSourceDefinitions checks all target-bound views, including
// static definitions that have no target entry of their own. Folder placement
// cannot establish that two definitions are never compiled into one program.
func validateUnifiedSourceDefinitions(rename sourceIdentifierRename, oldPaths, newPaths []string, views map[string][]*moduleSpec) error {
	type definitions struct{ oldPath, newPath string }
	byModule := make(map[string]definitions)
	seen := make(map[string]bool)
	for _, path := range append(append([]string(nil), oldPaths...), newPaths...) {
		if seen[path] {
			continue
		}
		seen[path] = true
		if len(views[path]) == 0 {
			return fmt.Errorf("cannot prove source definitions disjoint: %s has no target program view", path)
		}
		data, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		// The conditional evaluator knows target macros, not arbitrary local
		// or included definitions. Refuse an exclusion proof based on those.
		for index, line := range strings.Split(string(data), "\n") {
			trimmed := strings.TrimSpace(line)
			if !strings.HasPrefix(trimmed, "#") {
				continue
			}
			for _, name := range conditionalIdentifiers(trimmed) {
				if !strings.HasPrefix(name, moduleMacroPrefix) && !strings.HasPrefix(name, definePrefix) {
					return fmt.Errorf("%s:%d: cannot prove source definitions disjoint with non-target macro %s", path, index+1, name)
				}
			}
			word, rest := directiveWord(trimmed)
			if word == "define" || word == "undef" {
				fields := strings.Fields(rest)
				if len(fields) > 0 && (strings.HasPrefix(fields[0], moduleMacroPrefix) || strings.HasPrefix(fields[0], definePrefix)) {
					return fmt.Errorf("%s:%d: cannot prove source definitions disjoint with a local target macro change", path, index+1)
				}
			}
		}
		for _, module := range views[path] {
			active, err := activeConditionalText(string(data), module.macros())
			if err != nil {
				return fmt.Errorf("%s [%s]: %w", path, module.ID, err)
			}
			names, err := sourceDefinedIdentifiers([]byte(active))
			if err != nil {
				return fmt.Errorf("%s [%s]: %w", path, module.ID, err)
			}
			found := byModule[module.ID]
			if names[rename.oldName] {
				found.oldPath = path
			}
			if names[rename.newName] {
				found.newPath = path
			}
			if found.oldPath != "" && found.newPath != "" {
				return fmt.Errorf("source definitions collide in %s: %s (%s) and %s (%s)", module.ID,
					rename.oldName, found.oldPath, rename.newName, found.newPath)
			}
			byModule[module.ID] = found
		}
	}
	return nil
}

// sourceDefinedIdentifiers uses the declaration checker's file-scope parser,
// retaining static definitions because a rename can collide with them too.
// Function bodies are skipped, so local names and calls cannot be definitions.
func sourceDefinedIdentifiers(source []byte) (map[string]bool, error) {
	definitions, _, err := sourceDefinitions(source)
	return definitions, err
}

// sourceDefinitions is sourceDefinedIdentifiers that also returns the
// definitions without static storage. A name both maps hold is public in at
// least one conditional branch.
func sourceDefinitions(source []byte) (map[string]bool, map[string]bool, error) {
	definitions := make(map[string]bool)
	public := make(map[string]bool)
	err := visitSourceDefinitions(source, func(definition sourceDefinition) {
		for _, name := range []string{definition.symbol, definition.name} {
			if name == "" {
				continue
			}
			definitions[name] = true
			public[name] = public[name] || !definition.static
		}
	})
	if err != nil {
		return nil, nil, err
	}
	return definitions, public, nil
}

// sourceDefinition is one file-scope definition: a function with its body,
// an object, or an architectural register binding.
type sourceDefinition struct {
	name     string // the C name
	symbol   string // the linked symbol, which an asm label may rename; empty for a register
	static   bool
	function bool
	register bool
	offset   int
}

// visitSourceDefinitions walks a source's file-scope definitions in order.
func visitSourceDefinitions(source []byte, visit func(sourceDefinition)) error {
	text := string(source)
	tokens, err := tokenizeC(text)
	if err != nil {
		return err
	}
	for index := 0; index < len(tokens); {
		start := index
		if tokens[start].text == "}" {
			return fmt.Errorf("line %d: unexpected file-scope closing brace", lineOf(text, tokens[start].start))
		}
		next, statement, body, err := sourceDefinitionStatement(tokens, index)
		if err != nil {
			return fmt.Errorf("line %d: %w", lineOf(text, tokens[start].start), err)
		}
		index = next
		if len(statement) == 0 {
			// The shared parser skips standalone tag bodies. A trailing object
			// declarator instead needs parsing before its bindings can be trusted.
			for brace := start; brace < next; brace++ {
				if tokens[brace].text == "{" && isTagBody(tokens, start, brace) {
					close := matchingToken(tokens, brace)
					if close+1 < next && tokens[close+1].text != ";" {
						return fmt.Errorf("line %d: cannot parse object declared with a tag body", lineOf(text, tokens[start].start))
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
		external, static := false, false
		depth := 0
		for _, token := range statement {
			if depth == 0 && token.text == "static" {
				static = true
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
			return fmt.Errorf("line %d: cannot parse file-scope declaration", lineOf(text, tokens[start].start))
		}
		parts := splitTopLevel(filtered, ",")
		_, begin, _ := parseDeclarationSpecifier(parts[0])
		for partIndex, part := range parts {
			if len(part) == 0 {
				return fmt.Errorf("line %d: empty file-scope declarator", lineOf(text, tokens[start].start))
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
				return fmt.Errorf("line %d: %w", lineOf(text, part[0].start), err)
			} else if architectural {
				// This binds a C name, but no ELF symbol named "$N".
				visit(sourceDefinition{name: name, register: true, offset: part[0].start})
				continue
			}
			declarations := parseDeclaratorList(declarator, body)
			if len(declarations) != 1 {
				return fmt.Errorf("line %d: cannot parse file-scope declaration", lineOf(text, part[0].start))
			}
			declaration := declarations[0]
			if body || !functionDeclarationSignature(declaration.signature) && (!external || topLevelIndex(part, "=") >= 0) {
				lexical := part
				if partIndex == 0 {
					lexical = part[begin:]
				}
				definition := sourceDefinition{symbol: declaration.symbol, static: static, function: body, offset: part[0].start}
				if name := boundIdentifier(lexical); name >= 0 {
					definition.name = lexical[name].text
				}
				visit(definition)
			}
		}
	}
	return nil
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

// symbolRenameOptions selects how renameSymbolsWith applies a batch.
type symbolRenameOptions struct {
	function bool // functions, else data symbols
	dryRun   bool
	// join lets the new name exist already in modules the old name is absent
	// from, giving same-role bodies of separate programs one name.
	join bool
	// modules limits the rename to these modules' rows and the sources only
	// they compile; nil renames everywhere.
	modules map[string]bool
	// skipStatics keeps another program's same-named static copy when the old
	// name is public elsewhere (modules unset).
	skipStatics bool
}

// renameSymbols renames functions (function true) or data symbols across the
// configuration and every source under src/ and include/. A function
// definition rename also moves a standalone source named after the definition
// or one of its target entries to <new>.c.
// Concept-named sources and sources used by any merged unit keep their paths.
func (p project) renameSymbols(renames []symbolRename, function, dryRun bool) (result symbolMutationResult, err error) {
	return p.renameSymbolsWith(renames, symbolRenameOptions{function: function, dryRun: dryRun})
}

// renameSymbolsUnify is renameSymbols that, when unify is set, lets the new
// name already exist in modules the old name is absent from: the rename
// then gives one shared name to per-module copies.
func (p project) renameSymbolsUnify(renames []symbolRename, function, dryRun, unify bool) (result symbolMutationResult, err error) {
	return p.renameSymbolsWith(renames, symbolRenameOptions{function: function, dryRun: dryRun, join: unify})
}

// orderSymbolRenames checks that a batch names each old and each new name
// once and orders it so a pair whose new name another pair frees runs after
// that pair. A cycle needs a temporary name and is refused.
func orderSymbolRenames(renames []symbolRename) ([]symbolRename, error) {
	byOld := make(map[string]int)
	newSeen := make(map[string]bool)
	for index, rename := range renames {
		if err := validateRenameIdentifier(rename.oldName); err != nil {
			return nil, err
		}
		if err := validateRenameIdentifier(rename.newName); err != nil {
			return nil, err
		}
		if _, ok := byOld[rename.oldName]; ok || rename.oldName == rename.newName || newSeen[rename.newName] {
			return nil, fmt.Errorf("renames must be distinct and change the name: %s -> %s", rename.oldName, rename.newName)
		}
		byOld[rename.oldName] = index
		newSeen[rename.newName] = true
	}
	ordered := make([]symbolRename, 0, len(renames))
	state := make([]int, len(renames)) // 0 unvisited, 1 visiting, 2 done
	var visit func(int) error
	visit = func(index int) error {
		switch state[index] {
		case 1:
			return fmt.Errorf("renames form a cycle through %s; rename through a temporary name", renames[index].oldName)
		case 2:
			return nil
		}
		state[index] = 1
		if freed, ok := byOld[renames[index].newName]; ok {
			if err := visit(freed); err != nil {
				return err
			}
		}
		state[index] = 2
		ordered = append(ordered, renames[index])
		return nil
	}
	for index := range renames {
		if err := visit(index); err != nil {
			return nil, err
		}
	}
	return ordered, nil
}

// renameSymbolsWith is renameSymbols with a module scope, static skipping and
// joins; see symbolRenameOptions.
func (p project) renameSymbolsWith(renames []symbolRename, options symbolRenameOptions) (result symbolMutationResult, err error) {
	renames, err = orderSymbolRenames(renames)
	if err != nil {
		return result, err
	}
	err = p.withTargetLock(func() error {
		config, err := p.loadProjectConfig()
		if err != nil {
			return err
		}
		for module := range options.modules {
			if _, ok := config.module(module); !ok {
				return fmt.Errorf("unknown module %q", module)
			}
		}
		views := make(map[string][]*moduleSpec)
		for _, view := range declCheckViews(config) {
			path := filepath.Join(p.root, filepath.FromSlash(view.source))
			views[path] = append(views[path], config.byID[view.module])
		}
		scope := &sourceRenameScope{
			modules:     options.modules,
			fileModules: make(map[string]map[string]bool),
			skipStatics: options.skipStatics,
		}
		for _, m := range config.Modules {
			for index := range m.Functions {
				source := m.sourceOf(&m.Functions[index])
				if source == "" {
					continue
				}
				path := filepath.Join(p.root, filepath.FromSlash(source))
				if scope.fileModules[path] == nil {
					scope.fileModules[path] = make(map[string]bool)
				}
				scope.fileModules[path][m.ID] = true
			}
		}
		identifierRenames := make([]sourceIdentifierRename, len(renames))
		for index, rename := range renames {
			identifierRenames[index] = sourceIdentifierRename{rename.oldName, rename.newName}
		}
		sources, err := planSourceIdentifierRenamesScoped(p.root, identifierRenames, options.join, views, scope)
		if err != nil {
			return err
		}
		type move struct{ from, to string }
		var moves []move
		for index, rename := range renames {
			kept := sources.kept[index]
			keep := func(m *moduleSpec, source string) bool {
				if options.modules != nil && !options.modules[m.ID] {
					return true
				}
				return source != "" && kept[filepath.Join(p.root, filepath.FromSlash(source))]
			}
			if options.function {
				from, to, err := renameConfigFunctionScoped(config, rename.oldName, rename.newName, options.join, keep)
				if err != nil {
					return err
				}
				if from != "" {
					moves = append(moves, move{from, to})
				}
			} else if err := renameConfigDataScoped(config, rename.oldName, rename.newName, keep); err != nil {
				return err
			}
			if options.function && len(sources.newDefinition[index]) != 0 && !options.join {
				return fmt.Errorf("function definition already exists: %s", sources.newDefinition[index][0])
			}
		}
		result.Notes = sources.notes
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
			if err := transaction.addCreationOverRemoval(toPath, change.replacement, change.mode); err != nil {
				return err
			}
		}
		for path, change := range sources.changes {
			if err := transaction.addExistingSnapshot(path, change.original, change.replacement, change.mode); err != nil {
				return err
			}
		}
		result.ChangedFiles = len(transaction.changes)
		if options.dryRun {
			return nil
		}
		return transaction.commit()
	})
	return result, err
}

// renameConfigFunction keeps entry-only renames separate from source-definition
// renames. A definition rename updates every owner; only standalone sources
// named after the old definition or one of its target entries move with it.
func renameConfigFunction(config *projectConfig, oldName, newName string, unify bool) (string, string, error) {
	return renameConfigFunctionScoped(config, oldName, newName, unify, nil)
}

// renameConfigFunctionScoped is renameConfigFunction that leaves the rows
// keep reports (by module and source) under the old name.
func renameConfigFunctionScoped(config *projectConfig, oldName, newName string, unify bool, keep func(*moduleSpec, string) bool) (string, string, error) {
	if keep == nil {
		keep = func(*moduleSpec, string) bool { return false }
	}
	from, to := "", ""
	found := false
	unitSources := make(map[string]bool)
	for _, m := range config.Modules {
		for _, unit := range m.Units {
			unitSources[unit.Source] = true
		}
	}
	renamed := func(m *moduleSpec, f *functionSpec) bool {
		return (f.Name == oldName || f.sourceSymbol() == oldName) && !keep(m, m.sourceOf(f))
	}
	for _, m := range config.Modules {
		here := false
		for index := range m.Functions {
			here = here || renamed(m, &m.Functions[index])
		}
		if existing := m.function(newName); existing != nil {
			// Joining lets another module already use the name; the same
			// module holding both names is a real collision.
			if !unify || here {
				return "", "", fmt.Errorf("function already exists: %s", newName)
			}
		}
		for _, symbol := range append(append([]symbolSpec(nil), m.Data...), m.Imports...) {
			// A module may record another module's function as a data row (an
			// overlay entry it calls by address); joining keeps that name.
			if symbol.Name == newName && (!unify || here) {
				return "", "", fmt.Errorf("%s already names a symbol in %s", newName, m.ID)
			}
		}
		for index := range m.Functions {
			f := &m.Functions[index]
			if !renamed(m, f) {
				continue
			}
			found = true
			if f.sourceSymbol() == oldName {
				if f.Unit != "" {
					// A unit member's file holds its siblings too; the body is
					// renamed in place and the file keeps the unit's name.
					continue
				}
				source := m.sourceOf(f)
				stem := strings.TrimSuffix(filepath.Base(source), filepath.Ext(source))
				if unitSources[source] || (stem != oldName && stem != f.Name) {
					continue
				}
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
				if m.sourceOf(f) == from && !renamed(m, f) {
					return "", "", fmt.Errorf("source %s keeps %s in %s; rename it manually", from, oldName, m.ID)
				}
			}
		}
	}
	for _, m := range config.Modules {
		renamedEntries := make(map[string]bool)
		for index := range m.Functions {
			f := &m.Functions[index]
			if !renamed(m, f) {
				continue
			}
			source := m.sourceOf(f)
			if f.Name == oldName {
				renamedEntries[oldName] = true
				f.Name = newName
				// An alias's old default path remains owned by its definition.
				f.Source = source
			}
			if f.sourceSymbol() == oldName {
				f.SourceSymbol = newName
			}
			if from != "" && source == from {
				f.Source = to
			}
		}
		for index := range m.Overrides {
			if m.Overrides[index].Function == oldName && renamedEntries[oldName] {
				m.Overrides[index].Function = newName
			}
		}
	}
	return from, to, renameConfigReferencesScoped(config, oldName, newName, keep)
}

// renameConfigData renames data rows, imports and override bindings. A row
// that meets an existing row of the new name at the same address merges.
func renameConfigData(config *projectConfig, oldName, newName string) error {
	return renameConfigDataScoped(config, oldName, newName, nil)
}

func renameConfigDataScoped(config *projectConfig, oldName, newName string, keep func(*moduleSpec, string) bool) error {
	found := false
	for _, m := range config.Modules {
		if m.function(oldName) != nil {
			return fmt.Errorf("%s is a function in %s; use rename-function", oldName, m.ID)
		}
		if m.function(newName) != nil {
			return fmt.Errorf("%s is a function in %s", newName, m.ID)
		}
		if keep != nil && keep(m, "") {
			continue
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
	return renameConfigReferencesScoped(config, oldName, newName, keep)
}

func renameConfigReferences(config *projectConfig, oldName, newName string) error {
	return renameConfigReferencesScoped(config, oldName, newName, nil)
}

// renameConfigReferencesScoped renames data rows and imports in the modules
// keep leaves in scope, and override bindings whose function's source does
// not keep the old name.
func renameConfigReferencesScoped(config *projectConfig, oldName, newName string, keep func(*moduleSpec, string) bool) error {
	if keep == nil {
		keep = func(*moduleSpec, string) bool { return false }
	}
	for _, m := range config.Modules {
		if keep(m, "") {
			continue
		}
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
			override := &m.Overrides[index]
			if override.Name != oldName {
				continue
			}
			source := ""
			if f := m.function(override.Function); f != nil {
				source = m.sourceOf(f)
			}
			if !keep(m, source) {
				override.Name = newName
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
	return rewriteSourceText(source, rewrite, func(text []byte) []byte { return text })
}

// rewriteSourceText applies rewrite to every run of code text and comment to
// every comment, passing include lines and literals through unchanged.
func rewriteSourceText(source []byte, rewrite, comment func([]byte) []byte) []byte {
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
				output = append(output, comment(source[index:])...)
				index = len(source)
				continue
			}
			output = append(output, comment(source[index:index+2+stop+2])...)
			index += 2 + stop + 2
		case bytes.HasPrefix(source[index:], []byte("//")):
			flush()
			stop := bytes.IndexByte(source[index:], '\n')
			if stop < 0 {
				output = append(output, comment(source[index:])...)
				index = len(source)
				continue
			}
			output = append(output, comment(source[index:index+stop])...)
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
