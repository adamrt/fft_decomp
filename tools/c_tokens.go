// c_tokens.go is the small C lexer the declaration checker uses.
package main

import (
	"fmt"
	"strings"
)

type cTokenKind int

const (
	cIdent cTokenKind = iota
	cNumber
	cString
	cPunct
)

type cToken struct {
	kind       cTokenKind
	text       string
	start, end int
}

// tokenizeC splits C source into tokens, skipping comments and preprocessor
// lines. It is a lexer for declarations, not a conforming C front end.
func tokenizeC(source string) ([]cToken, error) {
	var tokens []cToken
	lineStart := true
	for index := 0; index < len(source); {
		character := source[index]
		switch {
		case character == '\n':
			lineStart = true
			index++
			continue
		case character == ' ' || character == '\t' || character == '\r' || character == '\f' || character == '\v':
			index++
			continue
		case character == '#' && lineStart:
			for index < len(source) && source[index] != '\n' {
				if source[index] == '\\' && index+1 < len(source) && source[index+1] == '\n' {
					index++
				}
				index++
			}
			continue
		case strings.HasPrefix(source[index:], "//"):
			for index < len(source) && source[index] != '\n' {
				index++
			}
			continue
		case strings.HasPrefix(source[index:], "/*"):
			end := strings.Index(source[index+2:], "*/")
			if end < 0 {
				return nil, fmt.Errorf("unterminated comment at byte %d", index)
			}
			index += end + 4
			continue
		}
		lineStart = false
		start := index
		switch {
		case isIdentStart(character):
			for index < len(source) && isIdentPart(source[index]) {
				index++
			}
			tokens = append(tokens, cToken{cIdent, source[start:index], start, index})
		case character >= '0' && character <= '9' || character == '.' && index+1 < len(source) && source[index+1] >= '0' && source[index+1] <= '9':
			for index < len(source) && (isIdentPart(source[index]) || source[index] == '.') {
				index++
			}
			tokens = append(tokens, cToken{cNumber, source[start:index], start, index})
		case character == '"' || character == '\'':
			index++
			for index < len(source) && source[index] != character {
				if source[index] == '\\' {
					index++
				}
				if index < len(source) && source[index] == '\n' {
					return nil, fmt.Errorf("unterminated literal at byte %d", start)
				}
				index++
			}
			if index >= len(source) {
				return nil, fmt.Errorf("unterminated literal at byte %d", start)
			}
			index++
			tokens = append(tokens, cToken{cString, source[start:index], start, index})
		default:
			length := 1
			for _, operator := range []string{"<<=", ">>=", "...", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", "&&", "||", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^="} {
				if strings.HasPrefix(source[index:], operator) {
					length = len(operator)
					break
				}
			}
			index += length
			tokens = append(tokens, cToken{cPunct, source[start:index], start, index})
		}
	}
	return tokens, nil
}

func isIdentStart(character byte) bool {
	return character == '_' || character >= 'a' && character <= 'z' || character >= 'A' && character <= 'Z'
}

func isIdentPart(character byte) bool {
	return isIdentStart(character) || character >= '0' && character <= '9'
}

func matchingToken(tokens []cToken, open int) int {
	pairs := map[string]string{"(": ")", "[": "]", "{": "}"}
	closer, ok := pairs[tokens[open].text]
	if !ok {
		return -1
	}
	depth := 0
	for index := open; index < len(tokens); index++ {
		switch tokens[index].text {
		case tokens[open].text:
			depth++
		case closer:
			depth--
			if depth == 0 {
				return index
			}
		}
	}
	return -1
}

func lineOf(source string, offset int) int {
	return strings.Count(source[:offset], "\n") + 1
}
