// units_source.go collects references from each active C function body for
// the unit report. These are lexical references, not a complete binding or
// call graph: local shadowing and indirect references still need review.
package main

import (
	"bytes"
	"fmt"
	"os/exec"
	"path/filepath"
	"strings"
)

type unitSourceFacts struct {
	idents      map[string]struct{}
	hasDivision bool
}

type unitSourceKey struct {
	path, defines string
}

type unitSourceView struct {
	functions map[string]unitSourceFacts
	err       error
}

type unitSourceCache map[unitSourceKey]unitSourceView

// Expand includes and macros with the same program defines as compilation.
// One source may expose different functions and references in each program.
func (p project) unitSourceFunctions(m *moduleSpec, source string, cache unitSourceCache) unitSourceView {
	defines := m.defineFlags()
	key := unitSourceKey{source, strings.Join(defines, "\x00")}
	if view, ok := cache[key]; ok {
		return view
	}
	dir, path := p.preprocessInput(filepath.Join(p.root, filepath.FromSlash(source)))
	args := p.cppFlags()
	args = append(args, defines...)
	args = append(args, path)
	command := exec.Command("mipsel-linux-gnu-cpp", args...)
	command.Dir = dir
	var stderr bytes.Buffer
	command.Stderr = &stderr
	data, err := command.Output()
	view := unitSourceView{}
	if err != nil {
		view.err = fmt.Errorf("preprocess: %w: %s", err, strings.TrimSpace(stderr.String()))
	} else {
		view.functions, view.err = scanUnitFunctionSources(string(data))
	}
	cache[key] = view
	return view
}

// File-scope declarations and initializers are not attributed to functions.
// The declaration parser also handles K&R definitions; the lexer excludes
// comments and preserves literals as tokens rather than identifier text.
func scanUnitFunctionSources(source string) (map[string]unitSourceFacts, error) {
	tokens, err := tokenizeC(source)
	if err != nil {
		return nil, err
	}
	functions := map[string]unitSourceFacts{}
	for index := 0; index < len(tokens); {
		next, statement, definition, err := sourceDefinitionStatement(tokens, index)
		if err != nil {
			return nil, fmt.Errorf("line %d: %w", lineOf(source, tokens[index].start), err)
		}
		start := index
		index = next
		if !definition {
			continue
		}
		declarations := parseDeclaratorList(statement, true)
		if len(declarations) != 1 || !functionDeclarationSignature(declarations[0].signature) {
			return nil, fmt.Errorf("line %d: cannot parse function definition", lineOf(source, tokens[start].start))
		}
		name := declarations[0].symbol
		if _, exists := functions[name]; exists {
			return nil, fmt.Errorf("duplicate active function %s", name)
		}
		// The statement ends just after the outer closing brace. Walk back
		// to its opening brace, including nested blocks and initializers.
		depth, open := 0, -1
		for i := next - 1; i >= start; i-- {
			switch tokens[i].text {
			case "}":
				depth++
			case "{":
				depth--
				if depth == 0 {
					open = i
				}
			}
			if open >= 0 {
				break
			}
		}
		if open < 0 {
			return nil, fmt.Errorf("line %d: missing function body", lineOf(source, tokens[start].start))
		}
		facts := unitSourceFacts{idents: map[string]struct{}{}}
		for i := open + 1; i < next-1; i++ {
			token := tokens[i]
			if token.kind == cIdent && tokens[i-1].text != "." && tokens[i-1].text != "->" {
				facts.idents[token.text] = struct{}{}
			}
			if token.kind == cPunct && (token.text == "/" || token.text == "/=" || token.text == "%" || token.text == "%=") {
				facts.hasDivision = true
			}
		}
		functions[name] = facts
	}
	return functions, nil
}
