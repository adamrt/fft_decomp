package main

import (
	"errors"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strings"
)

// splitSymbolsCommand restores a unit's members to per-function files in the
// unit's folder, in every module that lists the unit: the include block is
// repeated in each file and each member takes the text from the end of the
// previous member's body (its comments and static helpers) to its own
// closing brace.
func (p project) splitSymbolsCommand(args []string) error {
	flags := flag.NewFlagSet("symbols split", flag.ContinueOnError)
	name := flags.String("unit", "", "unit name")
	dryRun := flags.Bool("dry-run", false, "validate and report without writing")
	if err := flags.Parse(args); err != nil {
		return err
	}
	if *name == "" || flags.NArg() != 0 {
		return errors.New("usage: tools symbols split --unit NAME [--dry-run]")
	}
	var result string
	err := p.withTargetLock(func() error {
		config, err := p.loadProjectConfig()
		if err != nil {
			return err
		}
		var owners []*moduleSpec
		source := ""
		var order []string
		for _, m := range config.Modules {
			unit := m.unit(*name)
			if unit == nil {
				continue
			}
			if source != "" && unit.Source != source {
				return fmt.Errorf("unit %s has different sources in %s and %s", *name, owners[0].ID, m.ID)
			}
			source = unit.Source
			owners = append(owners, m)
			var names []string
			for _, f := range m.unitMembers(*name) {
				names = append(names, f.Name)
			}
			if order == nil {
				order = names
			} else if strings.Join(order, " ") != strings.Join(names, " ") {
				return fmt.Errorf("unit %s lists %v in %s but %v in %s", *name, order, owners[0].ID, names, m.ID)
			}
		}
		if len(owners) == 0 {
			return fmt.Errorf("no module has a unit %s", *name)
		}
		full := filepath.Join(p.root, filepath.FromSlash(source))
		info, data, err := readRegularSymbolFile(full)
		if err != nil {
			return err
		}
		for number, line := range strings.Split(string(data), "\n") {
			if trimmed := strings.TrimSpace(line); strings.HasPrefix(trimmed, "#") {
				if word, _ := directiveWord(trimmed); conditionalDirectives[word] {
					return fmt.Errorf("%s:%d: unit %s has program conditionals; split it by hand", source, number+1, *name)
				}
			}
		}
		pieces, includes, err := splitUnitSource(string(data), order)
		if err != nil {
			return fmt.Errorf("%s: %w", source, err)
		}
		folder := filepath.ToSlash(filepath.Dir(source))
		transaction := newSymbolFileTransaction()
		if err := transaction.addRemovalSnapshot(full, data, info.Mode()); err != nil {
			return err
		}
		for index, member := range order {
			path := folder + "/" + member + ".c"
			if _, err := os.Stat(filepath.Join(p.root, filepath.FromSlash(path))); err == nil {
				return fmt.Errorf("%s already exists", path)
			}
			text := includes + "\n\n" + strings.TrimSpace(pieces[index]) + "\n"
			if err := validateUnitSourceData(path, []byte(text), []string{member}); err != nil {
				return err
			}
			if err := transaction.addCreation(filepath.Join(p.root, filepath.FromSlash(path)), []byte(text), info.Mode()); err != nil {
				return err
			}
		}
		for _, m := range owners {
			for _, f := range m.unitMembers(*name) {
				f.Unit = ""
				f.Source = folder + "/" + f.Name + ".c"
				if f.Source == m.defaultSource(f) {
					f.Source = ""
				}
			}
			var kept []unitDef
			for _, unit := range m.Units {
				if unit.Name != *name {
					kept = append(kept, unit)
				}
			}
			m.Units = kept
		}
		for path, rendered := range renderConfigFiles(config) {
			full := filepath.Join(p.root, filepath.FromSlash(path))
			info, original, err := readRegularSymbolFile(full)
			if err != nil {
				return err
			}
			if err := transaction.addExistingSnapshot(full, original, rendered, info.Mode()); err != nil {
				return err
			}
		}
		result = fmt.Sprintf("symbols: split unit %s into %d files under %s in %d module(s)", *name, len(order), folder, len(owners))
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

// splitUnitSource cuts a merged unit source into one piece per member, in
// order, and returns the include block separately. A piece runs from the end
// of the previous member's body to this member's closing brace.
func splitUnitSource(text string, members []string) ([]string, string, error) {
	lines := strings.Split(text, "\n")
	var includes []string
	bodyStart := 0
	inComment := false
	for index, line := range lines {
		trimmed := strings.TrimSpace(line)
		// The file's own comment and blank lines precede the includes.
		if inComment {
			if strings.Contains(trimmed, "*/") {
				inComment = false
			}
			bodyStart = index + 1
			continue
		}
		if strings.HasPrefix(trimmed, "/*") {
			if !strings.Contains(trimmed, "*/") {
				inComment = true
			}
			bodyStart = index + 1
			continue
		}
		if strings.HasPrefix(trimmed, "#include") {
			includes = append(includes, trimmed)
			bodyStart = index + 1
			continue
		}
		if trimmed != "" {
			break
		}
		bodyStart = index + 1
	}
	pieces := make([]string, 0, len(members))
	cursor := bodyStart
	for _, member := range members {
		definition := regexp.MustCompile(`^[A-Za-z_][\w\s\*]*\b` + regexp.QuoteMeta(member) + `\s*\(`)
		start := -1
		for index := cursor; index < len(lines); index++ {
			if definition.MatchString(lines[index]) && !strings.HasSuffix(strings.TrimSpace(lines[index]), ";") {
				start = index
				break
			}
		}
		if start < 0 {
			return nil, "", fmt.Errorf("definition of %s not found after line %d", member, cursor+1)
		}
		end := bodyEndLine(lines, start)
		if end < 0 {
			return nil, "", fmt.Errorf("closing brace of %s not found", member)
		}
		pieces = append(pieces, strings.Join(lines[cursor:end+1], "\n"))
		cursor = end + 1
	}
	if rest := strings.TrimSpace(strings.Join(lines[cursor:], "\n")); rest != "" {
		return nil, "", fmt.Errorf("text after the last member: %q", rest[:min(len(rest), 60)])
	}
	return pieces, strings.Join(includes, "\n"), nil
}

// bodyEndLine returns the line on which the brace opened on or after line
// start closes, skipping comments and string or character literals.
func bodyEndLine(lines []string, start int) int {
	depth, opened := 0, false
	inComment := false
	for index := start; index < len(lines); index++ {
		line := lines[index]
		for i := 0; i < len(line); i++ {
			if inComment {
				if strings.HasPrefix(line[i:], "*/") {
					inComment = false
					i++
				}
				continue
			}
			switch {
			case strings.HasPrefix(line[i:], "/*"):
				inComment = true
				i++
			case strings.HasPrefix(line[i:], "//"):
				i = len(line)
			case line[i] == '"' || line[i] == '\'':
				quote := line[i]
				for i++; i < len(line) && line[i] != quote; i++ {
					if line[i] == '\\' {
						i++
					}
				}
			case line[i] == '{':
				depth++
				opened = true
			case line[i] == '}':
				depth--
				if opened && depth == 0 {
					return index
				}
			}
		}
	}
	return -1
}
