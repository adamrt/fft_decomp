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
