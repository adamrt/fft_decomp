package main

import (
	"errors"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

// moveSymbolsCommand moves entries' source files into another folder under
// src/ and rebinds every entry compiled from each moved file. Names do not
// change; rename-function does that.
func (p project) moveSymbolsCommand(args []string) error {
	flags := flag.NewFlagSet("symbols move", flag.ContinueOnError)
	dir := flags.String("dir", "", "destination folder under src/, such as src/shared/menu")
	dryRun := flags.Bool("dry-run", false, "validate and report without writing")
	if err := flags.Parse(args); err != nil {
		return err
	}
	usage := errors.New("usage: tools symbols move --dir src/shared/SUB [--dry-run] MODULE:ENTRY...")
	if *dir == "" || flags.NArg() == 0 {
		return usage
	}
	clean := filepath.ToSlash(filepath.Clean(*dir))
	if !strings.HasPrefix(clean, "src/") || filepath.Ext(clean) != "" {
		return fmt.Errorf("--dir must name a folder under src/: %q", *dir)
	}
	var result string
	err := p.withTargetLock(func() error {
		config, err := p.loadProjectConfig()
		if err != nil {
			return err
		}
		moves := map[string]string{}
		var order []string
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
			if f.Asm != "" {
				return fmt.Errorf("%s:%s is assembly", m.ID, entry)
			}
			if f.Unit != "" {
				return fmt.Errorf("%s:%s is compiled within unit %s", m.ID, entry, f.Unit)
			}
			from := m.sourceOf(f)
			if filepath.ToSlash(filepath.Dir(from)) == clean {
				return fmt.Errorf("%s is already under %s", from, clean)
			}
			to := clean + "/" + filepath.Base(from)
			if previous, seen := moves[from]; seen && previous != to {
				return fmt.Errorf("%s moves to two paths", from)
			}
			if _, seen := moves[from]; !seen {
				moves[from] = to
				order = append(order, from)
			}
		}
		targets := map[string]string{}
		for _, from := range order {
			to := moves[from]
			if other, taken := targets[to]; taken {
				return fmt.Errorf("%s and %s both move to %s", other, from, to)
			}
			targets[to] = from
			if _, err := os.Stat(filepath.Join(p.root, filepath.FromSlash(to))); err == nil {
				return fmt.Errorf("%s already exists", to)
			}
		}
		rebound := 0
		for _, m := range config.Modules {
			for index := range m.Functions {
				f := &m.Functions[index]
				if to, ok := moves[m.sourceOf(f)]; ok {
					f.Source = to
					rebound++
				}
			}
		}
		transaction := newSymbolFileTransaction()
		for _, from := range order {
			fromPath := filepath.Join(p.root, filepath.FromSlash(from))
			toPath := filepath.Join(p.root, filepath.FromSlash(moves[from]))
			info, data, err := readRegularSymbolFile(fromPath)
			if err != nil {
				return err
			}
			if err := os.MkdirAll(filepath.Dir(toPath), 0o755); err != nil {
				return err
			}
			if err := transaction.addRemovalSnapshot(fromPath, data, info.Mode()); err != nil {
				return err
			}
			if err := transaction.addCreation(toPath, data, info.Mode()); err != nil {
				return err
			}
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
		result = fmt.Sprintf("symbols: moved %d file(s) under %s; %d entries rebound", len(order), clean, rebound)
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
