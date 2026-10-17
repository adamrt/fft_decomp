// symbol_unify.go gives per-module copies one shared name (unify) and binds
// several entries to one shared source (share).
package main

import (
	"bytes"
	"errors"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
)

// unifySymbolsCommand renames every OLD to NEW in each module that has it,
// drops override rows the shared name makes redundant, and leaves one
// declaration of NEW, in the chosen header when one is named.
func (p project) unifySymbolsCommand(args []string) error {
	flags := flag.NewFlagSet("symbols unify", flag.ContinueOnError)
	newName := flags.String("new", "", "the shared name")
	header := flags.String("header", "", "subsystem header that keeps the one declaration, such as fft/menu.h")
	group := flags.String("group", "unified", "comment label of the header group a moved declaration joins")
	dryRun := flags.Bool("dry-run", false, "validate and report without writing")
	if err := flags.Parse(args); err != nil {
		return err
	}
	olds := flags.Args()
	if *newName == "" {
		return errors.New("usage: tools symbols unify --new NAME [--header fft/x.h] [--group LABEL] [--dry-run] OLD...")
	}
	if err := validateRenameIdentifier(*newName); err != nil {
		return err
	}
	if *header != "" && (strings.ContainsAny(*header, "\"\\\n") || !strings.HasSuffix(*header, ".h")) {
		return fmt.Errorf("--header must name a header such as fft/menu.h: %q", *header)
	}
	config, err := p.loadProjectConfig()
	if err != nil {
		return err
	}
	function := false
	for _, m := range config.Modules {
		for index := range m.Functions {
			f := &m.Functions[index]
			for _, name := range append([]string{*newName}, olds...) {
				if f.Name == name || f.sourceSymbol() == name {
					function = true
				}
			}
		}
	}
	known := func(name string) bool {
		for _, m := range config.Modules {
			for index := range m.Functions {
				if m.Functions[index].Name == name || m.Functions[index].sourceSymbol() == name {
					return true
				}
			}
			for _, list := range [][]symbolSpec{m.Data, m.Imports} {
				for _, symbol := range list {
					if symbol.Name == name {
						return true
					}
				}
			}
			for _, override := range m.Overrides {
				if override.Name == name {
					return true
				}
			}
		}
		return false
	}
	renamed := 0
	for _, old := range olds {
		if err := validateRenameIdentifier(old); err != nil {
			return err
		}
		if old == *newName {
			continue
		}
		// A rerun after a partial unify meets names already renamed.
		if !known(old) && known(*newName) {
			fmt.Printf("symbols: %s is already gone; %s exists\n", old, *newName)
			continue
		}
		if _, err := p.renameSymbolsUnify([]symbolRename{{old, *newName}}, function, *dryRun, true); err != nil {
			return fmt.Errorf("%s -> %s: %w", old, *newName, err)
		}
		renamed++
	}
	dropped := 0
	if !*dryRun {
		if dropped, err = p.dropRedundantOverrides(*newName); err != nil {
			return err
		}
	}
	kept, removed, err := p.consolidateDeclarations(*newName, *header, *group, *dryRun)
	if err != nil {
		return err
	}
	state := "unified"
	if *dryRun {
		state = "planned"
	}
	fmt.Printf("symbols: %s %s from %d name(s); %d override row(s) dropped; declaration in %s, %d duplicate(s) removed\n",
		state, *newName, renamed, dropped, kept, removed)
	return nil
}

// dropRedundantOverrides removes rows for the unified name that bind it to
// the address the module resolves it to anyway. Unrelated rows stay intact.
func (p project) dropRedundantOverrides(name string) (int, error) {
	dropped := 0
	err := p.withTargetLock(func() error {
		config, err := p.loadProjectConfig()
		if err != nil {
			return err
		}
		resolver := newSymbolResolver(config)
		for _, m := range config.Modules {
			kept := m.Overrides[:0]
			for _, override := range m.Overrides {
				if override.Name == name {
					if addr, ok := resolver.resolve(m, name); ok && addr == override.Addr {
						dropped++
						continue
					}
				}
				kept = append(kept, override)
			}
			m.Overrides = kept
		}
		if dropped == 0 {
			return nil
		}
		_, err = p.writeProjectConfig(config)
		return err
	})
	return dropped, err
}

var headerGroupLabel = regexp.MustCompile(`^/\* [a-z][^*\n]*\*/$`)

// declarationSpan is one file-scope declaration of a symbol in a header:
// the byte range to cut, including an attached comment above and the
// trailing comment on its last line.
type declarationSpan struct {
	path       string
	start, end int
	text       string // the declaration statement itself, without comments
}

// headerDeclarations finds every file-scope declaration of symbol in the
// header, with the span that removes it cleanly.
func headerDeclarations(path string, data []byte, symbol string) ([]declarationSpan, error) {
	source := string(data)
	tokens, err := tokenizeC(source)
	if err != nil {
		return nil, fmt.Errorf("%s: %w", path, err)
	}
	var spans []declarationSpan
	for index := 0; index < len(tokens); {
		next, statement, definition, err := declarationStatement(tokens, index)
		if err != nil {
			return nil, fmt.Errorf("%s: %w", path, err)
		}
		begin := index
		index = next
		if len(statement) == 0 || definition || statement[0].text == "typedef" {
			continue
		}
		declares := false
		for _, declaration := range parseDeclaratorList(statement, false) {
			declares = declares || declaration.symbol == symbol
		}
		if !declares {
			continue
		}
		start := tokens[begin].start
		end := tokens[next-1].end // the ';'
		// Whole lines: back to the line start, forward past a trailing comment.
		start = strings.LastIndex(source[:start], "\n") + 1
		if newline := strings.Index(source[end:], "\n"); newline >= 0 {
			end += newline + 1
		} else {
			end = len(source)
		}
		text := source[start:end]
		// An attached comment block directly above, unless it is a group label.
		above := source[:start]
		if strings.HasSuffix(above, "*/\n") {
			if open := strings.LastIndex(above, "/*"); open >= 0 {
				lineStart := strings.LastIndex(above[:open], "\n") + 1
				block := above[lineStart:]
				if strings.TrimSpace(above[lineStart:open]) == "" && !headerGroupLabel.MatchString(strings.TrimRight(block, "\n")) {
					start = lineStart
				}
			}
		}
		spans = append(spans, declarationSpan{path: path, start: start, end: end, text: text})
	}
	return spans, nil
}

// consolidateDeclarations leaves one declaration of symbol under include/:
// the one in header when given (moved there if absent), otherwise the first
// in a subsystem header, otherwise the first found. It returns the keeping
// header and the number of duplicates removed.
func (p project) consolidateDeclarations(symbol, header, group string, dryRun bool) (string, int, error) {
	type found struct {
		path  string
		data  []byte
		spans []declarationSpan
	}
	var files []found
	err := filepath.WalkDir(filepath.Join(p.root, "include"), func(path string, entry os.DirEntry, err error) error {
		if err != nil || entry.IsDir() || filepath.Ext(path) != ".h" {
			return err
		}
		data, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		if !bytes.Contains(data, []byte(symbol)) {
			return nil
		}
		spans, err := headerDeclarations(path, data, symbol)
		if err != nil {
			return err
		}
		if len(spans) > 0 {
			files = append(files, found{path: path, data: data, spans: spans})
		}
		return nil
	})
	if err != nil {
		return "", 0, err
	}
	sort.Slice(files, func(i, j int) bool { return files[i].path < files[j].path })
	if len(files) == 0 {
		return "none", 0, nil
	}
	target := ""
	if header != "" {
		target = filepath.Join(p.root, "include", filepath.FromSlash(header))
	}
	keep := -1
	for index, file := range files {
		if target != "" && file.path == target {
			keep = index
		}
	}
	if keep < 0 && target == "" {
		for index, file := range files {
			if moduleHeaderFile(file.path) {
				continue
			}
			keep = index
			break
		}
		if keep < 0 {
			keep = 0
		}
	}
	transaction := newSymbolFileTransaction()
	removed := 0
	var moved string
	for index, file := range files {
		spans := file.spans
		if index == keep {
			spans = spans[1:] // keep the first declaration there
		} else if keep < 0 && moved == "" {
			moved = spans[0].text
		}
		if len(spans) == 0 {
			continue
		}
		var out []byte
		last := 0
		for _, span := range spans {
			out = append(out, file.data[last:span.start]...)
			last = span.end
			removed++
		}
		out = append(out, file.data[last:]...)
		out = bytes.ReplaceAll(out, []byte("\n\n\n"), []byte("\n\n"))
		info, err := os.Stat(file.path)
		if err != nil {
			return "", 0, err
		}
		if err := transaction.addExistingSnapshot(file.path, file.data, out, info.Mode()); err != nil {
			return "", 0, err
		}
	}
	keptIn := ""
	if keep >= 0 {
		keptIn, _ = filepath.Rel(p.root, files[keep].path)
	} else {
		// Move the first declaration into the named header, under its group.
		info, data, err := readRegularSymbolFile(target)
		if err != nil {
			return "", 0, fmt.Errorf("--header %s: %w", header, err)
		}
		out, err := appendToHeaderGroup(data, group, strings.TrimRight(moved, "\n")+"\n")
		if err != nil {
			return "", 0, fmt.Errorf("%s: %w", header, err)
		}
		if err := transaction.addExistingSnapshot(target, data, out, info.Mode()); err != nil {
			return "", 0, err
		}
		keptIn = "include/" + header
		removed--
	}
	if dryRun || len(transaction.changes) == 0 {
		return filepath.ToSlash(keptIn), removed, nil
	}
	return filepath.ToSlash(keptIn), removed, transaction.commit()
}

// moduleHeaderFile reports whether a header belongs to one module rather
// than a subsystem: battle_*.h, world.h, wldcore.h, open.h, main.h, event_*.h.
func moduleHeaderFile(path string) bool {
	base := filepath.Base(path)
	switch base {
	case "battle.h", "world.h", "wldcore.h", "open.h", "main.h", "effect.h":
		return true
	}
	return strings.HasPrefix(base, "battle_") || strings.HasPrefix(base, "event_")
}

// appendToHeaderGroup adds a line to the `/* label */` group of a header,
// creating the group before the closing #endif when absent.
func appendToHeaderGroup(data []byte, label, line string) ([]byte, error) {
	text := string(data)
	marker := "/* " + label + " */\n"
	if index := strings.Index(text, "\n"+marker); index >= 0 {
		// Append after the group's last non-blank line.
		groupStart := index + 1 + len(marker)
		end := strings.Index(text[groupStart:], "\n\n")
		if end < 0 {
			end = len(text) - groupStart
		}
		insert := groupStart + end
		return []byte(text[:insert] + "\n" + strings.TrimRight(line, "\n") + text[insert:]), nil
	}
	guard := strings.LastIndex(text, "#endif")
	if guard < 0 {
		return nil, errors.New("header has no closing #endif")
	}
	block := marker + line + "\n"
	return []byte(text[:guard] + block + text[guard:]), nil
}

// shareSymbolsCommand binds entries in several modules to one source file:
// the kept member's file moves to --to, every listed entry compiles from it
// (through source_symbol when its name differs), and the other members'
// files go away when nothing else uses them.
func (p project) shareSymbolsCommand(args []string) error {
	flags := flag.NewFlagSet("symbols share", flag.ContinueOnError)
	to := flags.String("to", "", "shared source path under src/, such as src/menu/menu_x.c")
	from := flags.String("from", "", "MODULE:ENTRY whose source becomes the shared file (default: the first entry whose source is not already --to)")
	dryRun := flags.Bool("dry-run", false, "validate and report without writing")
	if err := flags.Parse(args); err != nil {
		return err
	}
	usage := errors.New("usage: tools symbols share --to src/SUB/NAME.c [--from MODULE:ENTRY] [--dry-run] MODULE:ENTRY...")
	if *to == "" || flags.NArg() == 0 {
		return usage
	}
	clean := filepath.ToSlash(filepath.Clean(*to))
	if !strings.HasPrefix(clean, "src/") || filepath.Ext(clean) != ".c" {
		return fmt.Errorf("--to must name a .c file under src/: %q", *to)
	}
	type member struct {
		m *moduleSpec
		f *functionSpec
	}
	var result string
	err := p.withTargetLock(func() error {
		config, err := p.loadProjectConfig()
		if err != nil {
			return err
		}
		parse := func(text string) (member, error) {
			moduleID, entry, ok := strings.Cut(text, ":")
			if !ok {
				return member{}, fmt.Errorf("%q is not MODULE:ENTRY", text)
			}
			m, err := resolveModuleName(config, moduleID)
			if err != nil {
				return member{}, err
			}
			f := m.function(entry)
			if f == nil {
				return member{}, fmt.Errorf("no function %s in %s", entry, m.ID)
			}
			if f.Asm != "" {
				return member{}, fmt.Errorf("%s:%s is assembly", m.ID, entry)
			}
			if f.Unit != "" {
				return member{}, fmt.Errorf("%s:%s is compiled within unit %s", m.ID, entry, f.Unit)
			}
			return member{m, f}, nil
		}
		var members []member
		for _, text := range flags.Args() {
			item, err := parse(text)
			if err != nil {
				return err
			}
			members = append(members, item)
		}
		kept := members[0]
		if *from != "" {
			if kept, err = parse(*from); err != nil {
				return err
			}
		} else {
			for _, item := range members {
				if item.m.sourceOf(item.f) != clean {
					kept = item
					break
				}
			}
		}
		keptSource := kept.m.sourceOf(kept.f)
		symbol := kept.f.sourceSymbol()
		keptPath := filepath.Join(p.root, filepath.FromSlash(keptSource))
		toPath := filepath.Join(p.root, filepath.FromSlash(clean))
		transaction := newSymbolFileTransaction()
		if keptSource != clean {
			info, data, err := readRegularSymbolFile(keptPath)
			if err != nil {
				return err
			}
			if err := os.MkdirAll(filepath.Dir(toPath), 0o755); err != nil {
				return err
			}
			if _, err := os.Stat(toPath); err == nil {
				return fmt.Errorf("%s already exists; pass it as --from or choose another path", clean)
			}
			if err := transaction.addRemovalSnapshot(keptPath, data, info.Mode()); err != nil {
				return err
			}
			if err := transaction.addCreation(toPath, data, info.Mode()); err != nil {
				return err
			}
		}
		if err := validateSourceDefinition(p.root, keptSource, symbol); err != nil {
			return err
		}
		// Rebind every listed entry, and every entry that compiled from the
		// kept file at its old path (aliases of the same body). Other members'
		// old files are left to their remaining users and removed only when
		// none remain.
		oldSources := map[string]bool{}
		for _, item := range members {
			oldSources[item.m.sourceOf(item.f)] = true
		}
		oldSources[keptSource] = true
		delete(oldSources, clean)
		rebound := 0
		for _, m := range config.Modules {
			for index := range m.Functions {
				f := &m.Functions[index]
				listed := m.sourceOf(f) == keptSource && keptSource != clean
				for _, item := range members {
					listed = listed || (item.m == m && item.f.Name == f.Name)
				}
				if !listed {
					continue
				}
				f.Source = clean
				f.SourceSymbol = ""
				if f.Name != symbol {
					f.SourceSymbol = symbol
				}
				rebound++
			}
		}
		// Files nothing compiles from any more are removed.
		stillUsed := map[string]bool{}
		for _, m := range config.Modules {
			for index := range m.Functions {
				stillUsed[m.sourceOf(&m.Functions[index])] = true
			}
		}
		removed := 0
		for source := range oldSources {
			if stillUsed[source] || source == keptSource {
				continue
			}
			path := filepath.Join(p.root, filepath.FromSlash(source))
			info, data, err := readRegularSymbolFile(path)
			if err != nil {
				if os.IsNotExist(err) {
					continue
				}
				return err
			}
			if err := transaction.addRemovalSnapshot(path, data, info.Mode()); err != nil {
				return err
			}
			removed++
		}
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
		result = fmt.Sprintf("symbols: %s defines %s for %d entries; %d twin file(s) removed", clean, symbol, rebound, removed)
		if *dryRun {
			result = "planned: " + result
			return nil
		}
		return transaction.commit()
	})
	if err != nil {
		return err
	}
	fmt.Println(result)
	return nil
}
