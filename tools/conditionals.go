// conditionals.go evaluates the preprocessor conditionals a source may use to
// serve several programs: #if, #ifdef, #ifndef, #elif, #else and #endif over
// the FFT_ module macros and the VARIANT_ build constants.
package main

import (
	"fmt"
	"strconv"
	"strings"
)

// conditionalDirectives are the directive words that open, branch or close
// a conditional block.
var conditionalDirectives = map[string]bool{"if": true, "ifdef": true, "ifndef": true, "elif": true, "else": true, "endif": true}

// activeConditionalText returns the source with every line inside an
// inactive conditional block, and every conditional directive line, replaced
// by an empty line, so line numbers survive. Macros maps a macro name to its
// value; an undefined identifier evaluates to 0.
func activeConditionalText(source string, macros map[string]string) (string, error) {
	type frame struct {
		parentActive bool
		taken        bool // a branch of this block has already been active
		active       bool
	}
	lines := strings.Split(source, "\n")
	out := make([]string, len(lines))
	var stack []frame
	active := func() bool { return len(stack) == 0 || stack[len(stack)-1].active }
	for index, line := range lines {
		trimmed := strings.TrimSpace(line)
		if !strings.HasPrefix(trimmed, "#") {
			if active() {
				out[index] = line
			}
			continue
		}
		word, rest := directiveWord(trimmed)
		if !conditionalDirectives[word] {
			if active() {
				out[index] = line
			}
			continue
		}
		switch word {
		case "if", "ifdef", "ifndef":
			parent := active()
			value := false
			if parent {
				var err error
				if value, err = evaluateConditional(word, rest, macros); err != nil {
					return "", fmt.Errorf("line %d: %w", index+1, err)
				}
			}
			stack = append(stack, frame{parentActive: parent, taken: value, active: parent && value})
		case "elif", "else":
			if len(stack) == 0 {
				return "", fmt.Errorf("line %d: #%s without #if", index+1, word)
			}
			top := &stack[len(stack)-1]
			value := true
			if word == "elif" && top.parentActive && !top.taken {
				var err error
				if value, err = evaluateConditional("if", rest, macros); err != nil {
					return "", fmt.Errorf("line %d: %w", index+1, err)
				}
			}
			top.active = top.parentActive && !top.taken && value
			if top.active {
				top.taken = true
			}
		case "endif":
			if len(stack) == 0 {
				return "", fmt.Errorf("line %d: #endif without #if", index+1)
			}
			stack = stack[:len(stack)-1]
		}
	}
	if len(stack) != 0 {
		return "", fmt.Errorf("unterminated #if block")
	}
	return strings.Join(out, "\n"), nil
}

// directiveWord splits "#  ifdef X /* c */" into "ifdef" and "X".
func directiveWord(trimmed string) (string, string) {
	rest := strings.TrimSpace(trimmed[1:])
	if at := strings.Index(rest, "//"); at >= 0 {
		rest = rest[:at]
	}
	for {
		start := strings.Index(rest, "/*")
		if start < 0 {
			break
		}
		end := strings.Index(rest[start:], "*/")
		if end < 0 {
			rest = rest[:start]
			break
		}
		rest = rest[:start] + " " + rest[start+end+2:]
	}
	fields := strings.Fields(rest)
	if len(fields) == 0 {
		return "", ""
	}
	return fields[0], strings.TrimSpace(strings.TrimPrefix(rest, fields[0]))
}

// evaluateConditional evaluates the condition of an #if, #ifdef or #ifndef.
func evaluateConditional(word, expression string, macros map[string]string) (bool, error) {
	switch word {
	case "ifdef", "ifndef":
		name := strings.TrimSpace(expression)
		if !validSymbol(name) {
			return false, fmt.Errorf("#%s needs one macro name, got %q", word, expression)
		}
		_, defined := macros[name]
		return defined == (word == "ifdef"), nil
	}
	parser := &conditionalParser{tokens: tokenizeConditional(expression), macros: macros}
	value, err := parser.parseOr()
	if err != nil {
		return false, err
	}
	if parser.at < len(parser.tokens) {
		return false, fmt.Errorf("unexpected %q in #if condition", parser.tokens[parser.at])
	}
	return value != 0, nil
}

// conditionalIdentifiers lists the macro names a conditional directive line
// tests, for the rule that sources test only FFT_ and VARIANT_ names.
func conditionalIdentifiers(trimmed string) []string {
	word, rest := directiveWord(trimmed)
	if !conditionalDirectives[word] || word == "else" || word == "endif" {
		return nil
	}
	var names []string
	for _, token := range tokenizeConditional(rest) {
		if token != "defined" && validSymbol(token) {
			names = append(names, token)
		}
	}
	return names
}

func tokenizeConditional(text string) []string {
	var tokens []string
	for i := 0; i < len(text); {
		c := text[i]
		switch {
		case c == ' ' || c == '\t':
			i++
		case c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'):
			start := i
			for i < len(text) && (text[i] == '_' || (text[i] >= 'a' && text[i] <= 'z') || (text[i] >= 'A' && text[i] <= 'Z') || (text[i] >= '0' && text[i] <= '9')) {
				i++
			}
			tokens = append(tokens, text[start:i])
		default:
			for _, op := range []string{"&&", "||", "==", "!=", "<=", ">="} {
				if strings.HasPrefix(text[i:], op) {
					tokens = append(tokens, op)
					i += 2
					goto next
				}
			}
			tokens = append(tokens, string(c))
			i++
		next:
		}
	}
	return tokens
}

type conditionalParser struct {
	tokens []string
	at     int
	macros map[string]string
}

func (p *conditionalParser) peek() string {
	if p.at < len(p.tokens) {
		return p.tokens[p.at]
	}
	return ""
}

func (p *conditionalParser) parseOr() (int64, error) {
	left, err := p.parseAnd()
	for err == nil && p.peek() == "||" {
		p.at++
		var right int64
		if right, err = p.parseAnd(); err == nil && (left != 0 || right != 0) {
			left = 1
		} else if err == nil {
			left = 0
		}
	}
	return left, err
}

func (p *conditionalParser) parseAnd() (int64, error) {
	left, err := p.parseCompare()
	for err == nil && p.peek() == "&&" {
		p.at++
		var right int64
		if right, err = p.parseCompare(); err == nil && left != 0 && right != 0 {
			left = 1
		} else if err == nil {
			left = 0
		}
	}
	return left, err
}

func (p *conditionalParser) parseCompare() (int64, error) {
	left, err := p.parseUnary()
	if err != nil {
		return 0, err
	}
	for {
		op := p.peek()
		switch op {
		case "==", "!=", "<", ">", "<=", ">=":
		default:
			return left, nil
		}
		p.at++
		right, err := p.parseUnary()
		if err != nil {
			return 0, err
		}
		var result bool
		switch op {
		case "==":
			result = left == right
		case "!=":
			result = left != right
		case "<":
			result = left < right
		case ">":
			result = left > right
		case "<=":
			result = left <= right
		case ">=":
			result = left >= right
		}
		left = 0
		if result {
			left = 1
		}
	}
}

func (p *conditionalParser) parseUnary() (int64, error) {
	switch token := p.peek(); token {
	case "!":
		p.at++
		value, err := p.parseUnary()
		if err != nil {
			return 0, err
		}
		if value == 0 {
			return 1, nil
		}
		return 0, nil
	case "(":
		p.at++
		value, err := p.parseOr()
		if err != nil {
			return 0, err
		}
		if p.peek() != ")" {
			return 0, fmt.Errorf("missing ) in #if condition")
		}
		p.at++
		return value, nil
	case "defined":
		p.at++
		parenthesized := p.peek() == "("
		if parenthesized {
			p.at++
		}
		name := p.peek()
		if !validSymbol(name) {
			return 0, fmt.Errorf("defined needs a macro name")
		}
		p.at++
		if parenthesized {
			if p.peek() != ")" {
				return 0, fmt.Errorf("missing ) after defined(%s", name)
			}
			p.at++
		}
		if _, ok := p.macros[name]; ok {
			return 1, nil
		}
		return 0, nil
	case "":
		return 0, fmt.Errorf("incomplete #if condition")
	default:
		p.at++
		if value, err := strconv.ParseInt(strings.TrimRight(token, "uUlL"), 0, 64); err == nil {
			return value, nil
		}
		if !validSymbol(token) {
			return 0, fmt.Errorf("unexpected %q in #if condition", token)
		}
		text, ok := p.macros[token]
		if !ok {
			return 0, nil
		}
		value, err := strconv.ParseInt(text, 0, 64)
		if err != nil {
			return 0, fmt.Errorf("macro %s has non-integer value %q", token, text)
		}
		return value, nil
	}
}

// programConditionalLine returns the 1-based line of the first conditional
// directive that tests a program macro or a VARIANT_ name, or 0. A guard on
// any other name tests neither and is ordinary text.
func programConditionalLine(source string) int {
	for index, line := range strings.Split(source, "\n") {
		trimmed := strings.TrimSpace(line)
		if !strings.HasPrefix(trimmed, "#") {
			continue
		}
		for _, name := range conditionalIdentifiers(trimmed) {
			if strings.HasPrefix(name, moduleMacroPrefix) || strings.HasPrefix(name, definePrefix) {
				return index + 1
			}
		}
	}
	return 0
}
