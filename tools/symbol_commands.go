package main

import (
	"bufio"
	"errors"
	"flag"
	"fmt"
	"os"
	"strings"
)

func (p project) symbolsCommand(args []string) error {
	if len(args) == 0 {
		return errors.New("usage: tools symbols rename-function|rename-global ...")
	}
	switch args[0] {
	case "rename-function", "rename-global":
		return p.renameSymbolCommand(args[0], args[1:])
	default:
		return fmt.Errorf("unknown symbols action %q; choose rename-function or rename-global", args[0])
	}
}

func (p project) renameSymbolCommand(action string, args []string) error {
	flags := flag.NewFlagSet("symbols "+action, flag.ContinueOnError)
	oldName := flags.String("old", "", "current symbol name")
	newName := flags.String("new", "", "replacement symbol name")
	pairsName := flags.String("pairs", "", "old<TAB>new file renamed in one transaction")
	dryRun := flags.Bool("dry-run", false, "validate and report without writing")
	if err := flags.Parse(args); err != nil {
		return err
	}
	usage := fmt.Errorf("usage: tools symbols %s --old OLD --new NEW [--dry-run] | --pairs PAIRS.tsv [--dry-run]", action)
	if flags.NArg() != 0 {
		return usage
	}
	var renames []symbolRename
	switch {
	case *pairsName != "" && *oldName == "" && *newName == "":
		path, err := projectPath(p.root, *pairsName, "rename pairs")
		if err != nil {
			return err
		}
		data, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		if renames, err = parseRenamePairs(data); err != nil {
			return err
		}
	case *pairsName == "" && *oldName != "" && *newName != "":
		renames = []symbolRename{{*oldName, *newName}}
	default:
		return usage
	}
	result, err := p.renameSymbols(renames, action == "rename-function", *dryRun)
	if err != nil {
		return err
	}
	state := "renamed"
	if *dryRun {
		state = "planned"
	}
	fmt.Printf("symbols: %s %d symbol(s) across %d files\n", state, len(renames), result.ChangedFiles)
	return nil
}

func parseRenamePairs(data []byte) ([]symbolRename, error) {
	var renames []symbolRename
	scanner := bufio.NewScanner(strings.NewReader(string(data)))
	for line := 1; scanner.Scan(); line++ {
		text := strings.TrimSpace(scanner.Text())
		if text == "" || strings.HasPrefix(text, "#") {
			continue
		}
		fields := strings.Split(text, "\t")
		if len(fields) != 2 {
			return nil, fmt.Errorf("rename pairs line %d: expected old and new name separated by a tab", line)
		}
		renames = append(renames, symbolRename{strings.TrimSpace(fields[0]), strings.TrimSpace(fields[1])})
	}
	if err := scanner.Err(); err != nil {
		return nil, err
	}
	if len(renames) == 0 {
		return nil, errors.New("rename pairs file contains no pairs")
	}
	return renames, nil
}
