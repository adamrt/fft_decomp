package main

import (
	"fmt"
	"os"
	"path/filepath"
)

// sourceSymbol distinguishes the C definition from the original entry's name.
func (f *functionSpec) sourceSymbol() string {
	if f.SourceSymbol != "" {
		return f.SourceSymbol
	}
	return f.Name
}

// Ordinary standalone entries may include a trailing retail body. Require
// their named public definition in the active view, without inventing a
// one-function-per-file restriction for those original spans.
func validateStandaloneSourceBinding(root string, module *moduleSpec, source, symbol string) error {
	data, err := os.ReadFile(filepath.Join(root, filepath.FromSlash(source)))
	if err != nil {
		return err
	}
	active, err := activeConditionalText(string(data), module.macros())
	if err != nil {
		return fmt.Errorf("%s: %w", source, err)
	}
	declarations, _, err := scanFileScopeDeclarations(active)
	if err != nil {
		return fmt.Errorf("%s: %w", source, err)
	}
	definitions := 0
	for _, declaration := range declarations {
		if declaration.symbol != symbol || !functionDeclarationSignature(declaration.signature) {
			continue
		}
		if declaration.static {
			return fmt.Errorf("source %s gives entry %s internal linkage for %s", source, symbol, module.ID)
		}
		if declaration.definition {
			definitions++
		}
	}
	if definitions != 1 {
		return fmt.Errorf("source %s must define public entry %s once for %s; found %d definitions", source, symbol, module.ID, definitions)
	}
	return nil
}

// Shared sources contain one public function; static helpers remain private.
func validateSourceDefinition(root, source, symbol string) error {
	data, err := os.ReadFile(filepath.Join(root, filepath.FromSlash(source)))
	if err != nil {
		return err
	}
	declarations, _, err := scanFileScopeDeclarations(string(data))
	if err != nil {
		return fmt.Errorf("%s: %w", source, err)
	}
	var definitions []string
	for _, declaration := range declarations {
		if declaration.definition && !declaration.static && functionDeclarationSignature(declaration.signature) {
			definitions = append(definitions, declaration.symbol)
		}
	}
	if len(definitions) != 1 || definitions[0] != symbol {
		return fmt.Errorf("source %s must define exactly one public function %s; found %v", source, symbol, definitions)
	}
	return nil
}
