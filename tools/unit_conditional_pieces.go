// unit_conditional_pieces.go cuts a unit source that keeps program
// conditionals into its members as each module compiles them, so the unit can
// be absorbed into a larger one. Only conditionals that wrap whole members at
// file scope are resolved; conditionals inside a function body, and
// conditional blocks that hold declarations only, stay in the text as written.
package main

import (
	"fmt"
	"regexp"
	"strings"
)

// unitPieceOwner is one module that compiles the unit source. Only listed
// owners take part in the merge; the others still decide where the file's
// conditionals fall, and keep compiling the file as it is.
type unitPieceOwner struct {
	id      string
	macros  map[string]string
	members []string // the module's members of the unit, in address order
	listed  bool
}

// unitPieceVariant is one definition of a member with the modules whose
// compile reaches it.
type unitPieceVariant struct {
	text    string
	modules map[string]bool
}

// unitPieceText is file-scope text between two members that other modules
// compile than the member after it: declarations ahead of a member only some
// modules have, or left inside a conditional after its last member.
type unitPieceText struct {
	text    string
	modules map[string]bool
}

// unitConditionalCut is a unit source cut per member. Preludes hold, for a
// member, the text that precedes its first definition under another module
// set; it is written before the member under its own modules.
type unitConditionalCut struct {
	includes []string
	variants map[string][]unitPieceVariant
	preludes map[string][]unitPieceText
}

// lineCode reports whether the line holds code outside comments and its net
// brace count outside comments and literals. inComment carries a block
// comment across lines.
func lineCode(line string, inComment *bool) (code bool, braces int) {
	for i := 0; i < len(line); i++ {
		if *inComment {
			if strings.HasPrefix(line[i:], "*/") {
				*inComment = false
				i++
			}
			continue
		}
		switch {
		case strings.HasPrefix(line[i:], "/*"):
			*inComment = true
			i++
		case strings.HasPrefix(line[i:], "//"):
			return code, braces
		case line[i] == '"' || line[i] == '\'':
			code = true
			quote := line[i]
			for i++; i < len(line) && line[i] != quote; i++ {
				if line[i] == '\\' {
					i++
				}
			}
		case line[i] == '{':
			code = true
			braces++
		case line[i] == '}':
			code = true
			braces--
		case line[i] != ' ' && line[i] != '\t' && line[i] != '\r':
			code = true
		}
	}
	return code, braces
}

// testsProgramMacro reports whether a directive line names a program macro
// or a VARIANT_ constant.
func testsProgramMacro(trimmed string) bool {
	for _, name := range conditionalIdentifiers(trimmed) {
		if strings.HasPrefix(name, moduleMacroPrefix) || strings.HasPrefix(name, definePrefix) {
			return true
		}
	}
	return false
}

// opensBody reports whether the parameter list that opens on line start, at
// or after column from, is followed by a body rather than ending a prototype.
func opensBody(lines []string, start, from int) bool {
	depth, opened := 0, false
	inComment := false
	for index := start; index < len(lines); index++ {
		line := lines[index]
		i := 0
		if index == start {
			i = from
		}
		for ; i < len(line); i++ {
			if inComment {
				if strings.HasPrefix(line[i:], "*/") {
					inComment = false
					i++
				}
				continue
			}
			switch {
			case strings.HasPrefix(line[i:], "/*"):
				inComment = true
				i++
			case strings.HasPrefix(line[i:], "//"):
				i = len(line)
			case line[i] == '(':
				depth++
				opened = true
			case line[i] == ')':
				depth--
			case opened && depth == 0 && line[i] != ' ' && line[i] != '\t' && line[i] != '\r':
				return line[i] == '{'
			}
		}
	}
	return false
}

// cutConditionalUnit cuts the unit text into member definitions for the
// listed modules. A program conditional at file scope that encloses a member
// decides which modules get that definition; the directive lines of those
// blocks are consumed. Text between two members joins the member after it
// when the same modules compile both, and becomes a prelude of that member
// otherwise. A definition or text no listed module compiles is left out.
func cutConditionalUnit(text string, owners []unitPieceOwner) (unitConditionalCut, error) {
	cut := unitConditionalCut{variants: map[string][]unitPieceVariant{}, preludes: map[string][]unitPieceText{}}
	lines := strings.Split(text, "\n")
	// The file's own comment and its includes precede the members, as in
	// splitUnitSource.
	bodyStart := 0
	inComment := false
	for index, line := range lines {
		trimmed := strings.TrimSpace(line)
		if inComment {
			if strings.Contains(trimmed, "*/") {
				inComment = false
			}
			bodyStart = index + 1
			continue
		}
		if strings.HasPrefix(trimmed, "/*") {
			if !strings.Contains(trimmed, "*/") {
				inComment = true
			}
			bodyStart = index + 1
			continue
		}
		if strings.HasPrefix(trimmed, "#include") {
			cut.includes = append(cut.includes, trimmed)
			bodyStart = index + 1
			continue
		}
		if trimmed != "" {
			break
		}
		bodyStart = index + 1
	}
	definitions := map[string]*regexp.Regexp{}
	var names []string
	for _, owner := range owners {
		for _, member := range owner.members {
			if definitions[member] == nil {
				definitions[member] = regexp.MustCompile(`^[A-Za-z_][\w\s\*]*\b` + regexp.QuoteMeta(member) + `\s*\(`)
				names = append(names, member)
			}
		}
	}
	definedAt := func(index int) string {
		for _, name := range names {
			if match := definitions[name].FindStringIndex(lines[index]); match != nil && opensBody(lines, index, match[1]-1) {
				return name
			}
		}
		return ""
	}
	// kind classifies every line from bodyStart on: a member definition's
	// first line ('m') with its last line in memberEnd, a directive of a
	// conditional block that encloses a member ('s', structural), or text.
	kind := make([]byte, len(lines))
	memberName := map[int]string{}
	memberEnd := map[int]int{}
	type block struct {
		directives []int
		program    bool
		members    bool
	}
	var open []*block
	depth := 0
	inComment = false
	for index := bodyStart; index < len(lines); index++ {
		trimmed := strings.TrimSpace(lines[index])
		if !inComment && depth == 0 && strings.HasPrefix(trimmed, "#") {
			word, _ := directiveWord(trimmed)
			if conditionalDirectives[word] {
				switch word {
				case "if", "ifdef", "ifndef":
					open = append(open, &block{directives: []int{index}, program: testsProgramMacro(trimmed)})
				default:
					if len(open) == 0 {
						return cut, fmt.Errorf("line %d: #%s without #if", index+1, word)
					}
					top := open[len(open)-1]
					top.directives = append(top.directives, index)
					top.program = top.program || testsProgramMacro(trimmed)
					if word == "endif" {
						open = open[:len(open)-1]
						if top.members {
							if !top.program {
								return cut, fmt.Errorf("line %d: members sit under a conditional that tests no program macro", top.directives[0]+1)
							}
							for _, at := range top.directives {
								kind[at] = 's'
							}
						}
					}
				}
				continue
			}
		}
		if !inComment && depth == 0 {
			if name := definedAt(index); name != "" {
				end := bodyEndLine(lines, index)
				if end < 0 {
					return cut, fmt.Errorf("closing brace of %s not found", name)
				}
				kind[index] = 'm'
				memberName[index] = name
				memberEnd[index] = end
				for _, b := range open {
					b.members = true
				}
				index = end
				continue
			}
		}
		_, braces := lineCode(lines[index], &inComment)
		depth += braces
	}
	if len(open) != 0 {
		return cut, fmt.Errorf("unterminated #if block at line %d", open[len(open)-1].directives[0]+1)
	}
	// Resolve the structural blocks for every owner and collect the members.
	type frame struct{ parentActive, taken, active bool }
	stacks := make([][]frame, len(owners))
	activeKey := func() string {
		key := make([]byte, len(owners))
		for index := range owners {
			key[index] = '1'
			if stack := stacks[index]; len(stack) > 0 && !stack[len(stack)-1].active {
				key[index] = '0'
			}
		}
		return string(key)
	}
	modulesOf := func(key string) map[string]bool {
		modules := map[string]bool{}
		for at, owner := range owners {
			if key[at] == '1' && owner.listed {
				modules[owner.id] = true
			}
		}
		return modules
	}
	// listedKey reduces a key to the listed owners: text joins the member
	// after it when the same listed modules compile both.
	listedKey := func(key string) string {
		reduced := []byte(key)
		for at, owner := range owners {
			if !owner.listed {
				reduced[at] = '0'
			}
		}
		return string(reduced)
	}
	// Text waiting for its member: runs of lines that one module set
	// compiles. Comment and blank lines join the code that follows them.
	type segment struct {
		lines []string
		key   string
	}
	var segments []segment
	var floating []string
	var carried []segment // text ahead of a member no listed module compiles
	perOwner := make([][]string, len(owners))
	inComment = false
	for index := bodyStart; index < len(lines); index++ {
		switch kind[index] {
		case 's':
			word, rest := directiveWord(strings.TrimSpace(lines[index]))
			for at := range owners {
				stack := stacks[at]
				switch word {
				case "if", "ifdef", "ifndef":
					parent := len(stack) == 0 || stack[len(stack)-1].active
					value := false
					if parent {
						var err error
						if value, err = evaluateConditional(word, rest, owners[at].macros); err != nil {
							return cut, fmt.Errorf("line %d: %w", index+1, err)
						}
					}
					stacks[at] = append(stack, frame{parentActive: parent, taken: value, active: parent && value})
				case "elif", "else":
					top := &stack[len(stack)-1]
					value := true
					if word == "elif" && top.parentActive && !top.taken {
						var err error
						if value, err = evaluateConditional("if", rest, owners[at].macros); err != nil {
							return cut, fmt.Errorf("line %d: %w", index+1, err)
						}
					}
					top.active = top.parentActive && !top.taken && value
					if top.active {
						top.taken = true
					}
				case "endif":
					stacks[at] = stack[:len(stack)-1]
				}
			}
		case 'm':
			name := memberName[index]
			full := activeKey()
			key := listedKey(full)
			start, end := index, memberEnd[index]
			// The text ahead of the member, oldest first. Its last runs join
			// the member when the same listed modules compile them.
			var ahead []segment
			for _, part := range append(carried, segments...) {
				if strings.Contains(part.key, "1") {
					ahead = append(ahead, part)
				}
			}
			carried, segments = nil, nil
			inComment = false
			index = end
			for at := range owners {
				if full[at] == '1' {
					perOwner[at] = append(perOwner[at], name)
				}
			}
			if !strings.Contains(key, "1") {
				// No listed module compiles this definition; text that some
				// of them compile still needs a member to precede.
				carried, floating = ahead, nil
				continue
			}
			lead := floating
			floating = nil
			for len(ahead) > 0 && ahead[len(ahead)-1].key == key {
				lead = append(append([]string(nil), ahead[len(ahead)-1].lines...), lead...)
				ahead = ahead[:len(ahead)-1]
			}
			if len(ahead) > 0 && len(cut.variants[name]) > 0 {
				return cut, fmt.Errorf("line %d: text between two definitions of %s is under another program conditional; merge it by hand", start+1, name)
			}
			for _, part := range ahead {
				cut.preludes[name] = append(cut.preludes[name], unitPieceText{text: strings.TrimSpace(strings.Join(part.lines, "\n")), modules: modulesOf(part.key)})
			}
			body := strings.TrimSpace(strings.Join(append(lead, lines[start:end+1]...), "\n"))
			cut.variants[name] = append(cut.variants[name], unitPieceVariant{text: body, modules: modulesOf(key)})
		default:
			code, _ := lineCode(lines[index], &inComment)
			if !code {
				floating = append(floating, lines[index])
				continue
			}
			key := listedKey(activeKey())
			if count := len(segments); count > 0 && segments[count-1].key == key {
				segments[count-1].lines = append(append(segments[count-1].lines, floating...), lines[index])
			} else {
				segments = append(segments, segment{lines: append(append([]string(nil), floating...), lines[index]), key: key})
			}
			floating = nil
		}
	}
	for _, part := range segments {
		if strings.Contains(part.key, "1") {
			rest := strings.TrimSpace(strings.Join(part.lines, "\n"))
			return cut, fmt.Errorf("text after the last member: %q", rest[:min(len(rest), 60)])
		}
	}
	if len(carried) > 0 {
		rest := strings.TrimSpace(strings.Join(carried[0].lines, "\n"))
		return cut, fmt.Errorf("text after the last member: %q", rest[:min(len(rest), 60)])
	}
	for at, owner := range owners {
		if strings.Join(perOwner[at], " ") != strings.Join(owner.members, " ") {
			return cut, fmt.Errorf("%s compiles %v from the file but its unit lists %v", owner.id, perOwner[at], owner.members)
		}
	}
	return cut, nil
}
