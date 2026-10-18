package main

import (
	"fmt"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"
)

// Included headers can turn a seemingly ordinary call into a macro. Use the
// compiler's active macro set, then decline rather than rewrite expansions.
func rejectAliasMacros(root string, module *moduleSpec, target *functionSpec, text string, replacements map[string]string) error {
	p := project{root: root}
	dir, source := p.preprocessInput(filepath.Join(root, filepath.FromSlash(module.sourceOf(target))))
	args := append(p.cppFlags(), "-dM")
	args = append(args, module.defineFlags()...)
	args = append(args, source)
	command := exec.Command("mipsel-linux-gnu-cpp", args...)
	command.Dir = dir
	output, err := command.CombinedOutput()
	if err != nil {
		return fmt.Errorf("cannot inspect header macros: %w: %s; materialize reviewed source manually", err, strings.TrimSpace(string(output)))
	}
	macros := map[string]bool{}
	for _, match := range regexp.MustCompile(`(?m)^#define[ \t]+([A-Za-z_][A-Za-z0-9_]*)`).FindAllStringSubmatch(string(output), -1) {
		macros[match[1]] = true
	}
	tokens, err := tokenizeC(text)
	if err != nil {
		return err
	}
	for _, token := range tokens {
		if token.kind == cIdent && macros[token.text] {
			return fmt.Errorf("body uses macro %s; materialize reviewed source manually", token.text)
		}
	}
	for _, replacement := range replacements {
		if macros[replacement] {
			return fmt.Errorf("replacement name %s is a macro; materialize reviewed source manually", replacement)
		}
	}
	return nil
}

// Only ordinary function declarations and direct calls are rewritten. Data
// bindings and ambiguous C namespaces require a reviewed source, not a guess
// from a lexical replacement. All edits use original tokens simultaneously.
func materializeFunctionAlias(text, self string, replacements map[string]string, module *moduleSpec) (string, error) {
	reject := func(reason string) (string, error) {
		return "", fmt.Errorf("%s; materialize reviewed source manually", reason)
	}
	directive := false
	rewriteCodeOutsideIncludes([]byte(text), func(code []byte) []byte {
		if regexp.MustCompile(`(?m)^[ \t]*#`).Match(code) {
			directive = true
		}
		return code
	})
	if directive {
		return reject("source directives can hide function bindings")
	}
	tokens, err := tokenizeC(text)
	if err != nil {
		return "", err
	}
	critical := map[string]bool{}
	destinations := map[string]string{}
	for from, to := range replacements {
		critical[from], critical[to] = true, true
		if previous := destinations[to]; previous != "" && previous != from {
			return reject("several symbols map to " + to)
		}
		destinations[to] = from
	}
	allowed := map[int]bool{}
	knownCalls := map[string]bool{"if": true, "for": true, "while": true, "switch": true, "return": true, "sizeof": true}
	for _, function := range module.Functions {
		knownCalls[function.Name] = true
	}
	bodyStart, bodyEnd := -1, -1
	for index := 0; index < len(tokens); {
		next, statement, definition, err := sourceDefinitionStatement(tokens, index)
		if err != nil {
			return reject("cannot classify source declarations")
		}
		declarations := parseDeclaratorList(statement, definition)
		functionNames := map[string]bool{}
		for _, declaration := range declarations {
			if functionDeclarationSignature(declaration.signature) {
				functionNames[declaration.symbol] = true
				knownCalls[declaration.symbol] = true
			}
		}
		if definition {
			if !functionNames[self] || bodyStart >= 0 {
				return reject("source does not contain one ordinary alias target")
			}
			_, body, ok := shareAuditDefinition(tokens[index:next], self)
			if !ok {
				return reject("cannot classify function parameters")
			}
			bodyStart, bodyEnd = body[0].start, body[len(body)-1].end
		}
		depth := 0
		for cursor := index; cursor < next; cursor++ {
			token := tokens[cursor]
			if definition && token.start >= bodyStart {
				break
			}
			if token.kind == cIdent && critical[token.text] {
				if depth != 0 || !functionNames[token.text] || cursor+1 >= next || tokens[cursor+1].text != "(" {
					return reject("ambiguous declaration of " + token.text)
				}
				allowed[token.start] = true
			}
			switch token.text {
			case "(", "[", "{":
				depth++
			case ")", "]", "}":
				depth--
			}
		}
		index = next
	}
	if bodyStart < 0 {
		return reject("missing ordinary function definition")
	}
	called := map[string]bool{}
	for index, token := range tokens {
		if token.start > bodyStart && token.start < bodyEnd && token.kind == cIdent && index+1 < len(tokens) && tokens[index+1].text == "(" && tokens[index-1].text != "." && tokens[index-1].text != "->" && !knownCalls[token.text] {
			return reject("unclassified call or macro " + token.text)
		}
		if token.kind != cIdent || !critical[token.text] {
			continue
		}
		if _, source := replacements[token.text]; !source {
			return reject("replacement name " + token.text + " already occurs in the source")
		}
		if allowed[token.start] {
			continue
		}
		if token.start <= bodyStart || token.start >= bodyEnd || index+1 >= len(tokens) || tokens[index+1].text != "(" {
			return reject("non-call use of " + token.text)
		}
		previous := tokens[index-1]
		if previous.text == "." || previous.text == "->" || previous.kind == cIdent && previous.text != "return" && previous.text != "sizeof" {
			return reject("ambiguous call or local declaration of " + token.text)
		}
		allowed[token.start] = true
		called[token.text] = true
	}
	for from := range replacements {
		if from != self && !called[from] {
			return reject("override " + from + " has no direct call in the source")
		}
	}
	var output strings.Builder
	cursor := 0
	for _, token := range tokens {
		if !allowed[token.start] {
			continue
		}
		output.WriteString(text[cursor:token.start])
		output.WriteString(replacements[token.text])
		cursor = token.end
	}
	output.WriteString(text[cursor:])
	return output.String(), nil
}
