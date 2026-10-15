package main

import (
	"errors"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"
)

// unitSymbolsCommand merges the listed entries' per-function sources into one
// unit source and makes the entries members of a unit of that name in each
// of their modules. The members of one module must follow one another in
// address order, every module must list the same functions, and the
// per-function files must already be the same in every module.
func (p project) unitSymbolsCommand(args []string) error {
	flags := flag.NewFlagSet("symbols unit", flag.ContinueOnError)
	name := flags.String("name", "", "unit name")
	source := flags.String("source", "", "merged source path under src/, such as src/shared/thread/thread.c")
	dryRun := flags.Bool("dry-run", false, "validate and report without writing")
	if err := flags.Parse(args); err != nil {
		return err
	}
	usage := errors.New("usage: tools symbols unit --name NAME --source src/SUB/NAME.c [--dry-run] MODULE:ENTRY...")
	if *name == "" || *source == "" || flags.NArg() == 0 {
		return usage
	}
	if err := validateRenameIdentifier(*name); err != nil {
		return err
	}
	clean := filepath.ToSlash(filepath.Clean(*source))
	if !strings.HasPrefix(clean, "src/") || filepath.Ext(clean) != ".c" {
		return fmt.Errorf("--source must name a .c file under src/: %q", *source)
	}
	var result string
	err := p.withTargetLock(func() error {
		config, err := p.loadProjectConfig()
		if err != nil {
			return err
		}
		byModule := map[*moduleSpec][]*functionSpec{}
		var order []*moduleSpec
		for _, text := range flags.Args() {
			moduleID, entry, ok := strings.Cut(text, ":")
			if !ok {
				return fmt.Errorf("%q is not MODULE:ENTRY", text)
			}
			m, err := resolveModuleName(config, moduleID)
			if err != nil {
				return err
			}
			f := m.function(entry)
			if f == nil {
				return fmt.Errorf("no function %s in %s", entry, m.ID)
			}
			if f.Asm != "" || f.SourceSymbol != "" || f.Unit != "" {
				return fmt.Errorf("%s:%s must be a plain C entry (not assembly, an alias or a unit member)", m.ID, entry)
			}
			if _, seen := byModule[m]; !seen {
				order = append(order, m)
			}
			byModule[m] = append(byModule[m], f)
		}
		var mergedSources []string
		var symbols []string
		for _, m := range order {
			if m.unit(*name) != nil {
				return fmt.Errorf("%s already has a unit %s", m.ID, *name)
			}
			members := byModule[m]
			sort.Slice(members, func(i, j int) bool { return members[i].Addr < members[j].Addr })
			var functions, sources []string
			for _, f := range members {
				functions = append(functions, f.Name)
				sources = append(sources, m.sourceOf(f))
			}
			spec := &unitSpec{name: *name, source: clean, functions: functions}
			if _, err := resolveUnitInstance(config, spec, unitInstanceYAML{Module: m.ID}); err != nil {
				return err
			}
			if mergedSources == nil {
				mergedSources, symbols = sources, functions
			} else if strings.Join(sources, " ") != strings.Join(mergedSources, " ") {
				return fmt.Errorf("%s compiles %v where %s compiles %v; a unit needs the same sources in every module", m.ID, sources, order[0].ID, mergedSources)
			}
		}
		for _, existing := range mergedSources {
			if existing == clean {
				return fmt.Errorf("%s is already a member source; choose a new path for the unit", clean)
			}
		}
		// Every entry compiled from a member file must join the unit: a file
		// left behind would be a second body of the same function.
		listed := map[string]bool{}
		for _, m := range order {
			for _, f := range byModule[m] {
				listed[m.ID+":"+f.Name] = true
			}
		}
		for _, m := range config.Modules {
			for index := range m.Functions {
				f := &m.Functions[index]
				for _, existing := range mergedSources {
					if m.sourceOf(f) == existing && !listed[m.ID+":"+f.Name] {
						return fmt.Errorf("%s:%s also compiles %s; list it in the unit or leave that function out", m.ID, f.Name, existing)
					}
				}
			}
		}
		if _, err := os.Stat(filepath.Join(p.root, filepath.FromSlash(clean))); err == nil {
			return fmt.Errorf("%s already exists", clean)
		}
		merged, err := mergeUnitSources(p.root, mergedSources)
		if err != nil {
			return err
		}
		if err := validateUnitSourceData(clean, merged, symbols); err != nil {
			return err
		}
		for _, m := range order {
			m.Units = append(m.Units, unitDef{Name: *name, Source: clean})
			for _, f := range byModule[m] {
				f.Unit = *name
				f.Source = ""
			}
		}
		stillUsed := map[string]bool{}
		for _, m := range config.Modules {
			for index := range m.Functions {
				stillUsed[m.sourceOf(&m.Functions[index])] = true
			}
		}
		transaction := newSymbolFileTransaction()
		if err := os.MkdirAll(filepath.Dir(filepath.Join(p.root, filepath.FromSlash(clean))), 0o755); err != nil {
			return err
		}
		if err := transaction.addCreation(filepath.Join(p.root, filepath.FromSlash(clean)), merged, 0o644); err != nil {
			return err
		}
		removed := 0
		for _, old := range mergedSources {
			if stillUsed[old] {
				continue
			}
			path := filepath.Join(p.root, filepath.FromSlash(old))
			info, data, err := readRegularSymbolFile(path)
			if err != nil {
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
		result = fmt.Sprintf("symbols: unit %s at %s holds %d functions in %d module(s); %d per-function file(s) removed", *name, clean, len(symbols), len(order), removed)
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
