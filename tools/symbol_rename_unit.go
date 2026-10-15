package main

import (
	"errors"
	"flag"
	"fmt"
	"os"
	"path/filepath"
)

// renameUnitCommand gives a unit a new name and moves its file to
// <folder>/<name>.c in every module that lists it. Member rows keep their
// function names; only the unit name and its source path change.
func (p project) renameUnitCommand(args []string) error {
	flags := flag.NewFlagSet("symbols rename-unit", flag.ContinueOnError)
	oldName := flags.String("unit", "", "unit name")
	newName := flags.String("name", "", "new unit name; the file becomes <folder>/<name>.c")
	dryRun := flags.Bool("dry-run", false, "validate and report without writing")
	if err := flags.Parse(args); err != nil {
		return err
	}
	if *oldName == "" || *newName == "" || flags.NArg() != 0 {
		return errors.New("usage: tools symbols rename-unit --unit OLD --name NEW [--dry-run]")
	}
	if err := validateRenameIdentifier(*newName); err != nil {
		return err
	}
	if *oldName == *newName {
		return errors.New("the new name must differ")
	}
	var result string
	err := p.withTargetLock(func() error {
		config, err := p.loadProjectConfig()
		if err != nil {
			return err
		}
		source := ""
		var owners []*moduleSpec
		for _, m := range config.Modules {
			unit := m.unit(*oldName)
			if unit == nil {
				continue
			}
			if source != "" && unit.Source != source {
				return fmt.Errorf("unit %s has different sources in %s and %s", *oldName, owners[0].ID, m.ID)
			}
			source = unit.Source
			owners = append(owners, m)
			if m.unit(*newName) != nil {
				return fmt.Errorf("%s already has a unit %s", m.ID, *newName)
			}
		}
		if len(owners) == 0 {
			return fmt.Errorf("no module has a unit %s", *oldName)
		}
		target := filepath.ToSlash(filepath.Dir(source)) + "/" + *newName + ".c"
		if _, err := os.Stat(filepath.Join(p.root, filepath.FromSlash(target))); err == nil {
			return fmt.Errorf("%s already exists", target)
		}
		for _, m := range owners {
			for index := range m.Units {
				if m.Units[index].Name == *oldName {
					m.Units[index].Name = *newName
					m.Units[index].Source = target
				}
			}
			for index := range m.Functions {
				if m.Functions[index].Unit == *oldName {
					m.Functions[index].Unit = *newName
				}
			}
		}
		transaction := newSymbolFileTransaction()
		fromPath := filepath.Join(p.root, filepath.FromSlash(source))
		info, data, err := readRegularSymbolFile(fromPath)
		if err != nil {
			return err
		}
		if err := transaction.addRemovalSnapshot(fromPath, data, info.Mode()); err != nil {
			return err
		}
		if err := transaction.addCreation(filepath.Join(p.root, filepath.FromSlash(target)), data, info.Mode()); err != nil {
			return err
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
		result = fmt.Sprintf("symbols: unit %s renamed to %s at %s in %d module(s)", *oldName, *newName, target, len(owners))
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
