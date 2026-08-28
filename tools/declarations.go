// declarations.go finds symbols whose file-scope declarations disagree across
// translation units. Each function is its own translation unit, so the
// compiler never compares them; this check does.
package main

import (
	"errors"
	"flag"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
)

// declarationScanDirs are scanned for .c and .h files. Headers are included so
// that a local extern contradicting its owning header is reported.
var declarationScanDirs = []string{"src", "include"}

type scannedDeclaration struct {
	symbol     string
	signature  []string // rendered type; '@' stands for the name, parameter lists are one segment
	line       int
	definition bool
}

type declarationSite struct {
	path       string
	line       int
	signature  []string
	definition bool
}

type declarationSpelling struct {
	signature []string
	sites     []declarationSite
}

type declarationGroup struct {
	symbol    string
	sites     []declarationSite
	spellings []declarationSpelling // most common first
	minority  int                   // declarations disagreeing with the best-supported spelling
}

// declarationQualifiers are dropped from rendered types: const and volatile
// are matching levers in this project rather than claims about the data, and
// the rest are storage classes.
var declarationQualifiers = map[string]bool{
	"const": true, "volatile": true, "register": true, "extern": true, "auto": true,
	"inline": true, "__inline": true, "__inline__": true,
}

var declarationBaseWords = map[string]bool{
	"signed": true, "unsigned": true, "short": true, "long": true, "int": true,
	"char": true, "void": true, "float": true, "double": true,
}

var macroInvocationName = regexp.MustCompile(`^[A-Z][A-Z0-9_]*$`)

// scanFileScopeDeclarations returns the declarations and function definitions
// bound at file scope, plus "struct tag" -> alias mappings from typedefs.
func scanFileScopeDeclarations(source string) ([]scannedDeclaration, map[string]string, error) {
	tokens, err := tokenizeC(source)
	if err != nil {
		return nil, nil, err
	}
	var declarations []scannedDeclaration
	tags := map[string]string{}
	for index := 0; index < len(tokens); {
		next, statement, definition, err := declarationStatement(tokens, index)
		if err != nil {
			return nil, nil, fmt.Errorf("line %d: %w", lineOf(source, tokens[index].start), err)
		}
		index = next
		if len(statement) == 0 {
			continue
		}
		if statement[0].text == "typedef" {
			recordTypedefTag(statement, tags)
			continue
		}
		for _, declaration := range parseDeclaratorList(statement, definition) {
			declaration.line = lineOf(source, declaration.line)
			declarations = append(declarations, declaration)
		}
	}
	return declarations, tags, nil
}

// declarationStatement consumes one file-scope statement starting at start.
// It returns the index past the statement and the tokens that declare
// something; statements that bind nothing return no tokens.
func declarationStatement(tokens []cToken, start int) (int, []cToken, bool, error) {
	first := tokens[start]
	if first.text == ";" || first.text == "}" {
		return start + 1, nil, false, nil
	}
	// A file-scope macro invocation such as DEFINE_PRIMITIVE_SETTER(...)
	// carries no terminating semicolon.
	if first.kind == cIdent && macroInvocationName.MatchString(first.text) && start+1 < len(tokens) && tokens[start+1].text == "(" {
		close := matchingToken(tokens, start+1)
		if close < 0 {
			return 0, nil, false, errors.New("unterminated macro invocation")
		}
		next := close + 1
		if next < len(tokens) && tokens[next].text == ";" {
			next++
		}
		return next, nil, false, nil
	}
	depth := 0
	assigned := false
	typeBody := false
	for index := start; index < len(tokens); index++ {
		switch tokens[index].text {
		case "(", "[":
			depth++
		case ")", "]":
			depth--
		case "=":
			if depth == 0 {
				assigned = true
			}
		case "{":
			close := matchingToken(tokens, index)
			if close < 0 {
				return 0, nil, false, errors.New("unterminated brace")
			}
			switch {
			case assigned:
				// Initializer: keep collecting to the ';'.
			case isTagBody(tokens, start, index):
				typeBody = true
			case depth == 0:
				// Function body, skipped entirely so locals never reach the pool.
				return close + 1, tokens[start:index], true, nil
			}
			index = close
		case ";":
			if depth != 0 {
				continue
			}
			if typeBody && tokens[start].text != "typedef" {
				return index + 1, nil, false, nil
			}
			return index + 1, tokens[start:index], false, nil
		}
	}
	return len(tokens), nil, false, errors.New("statement is not terminated")
}

func isTagBody(tokens []cToken, start, brace int) bool {
	previous := brace - 1
	if previous >= start && tokens[previous].kind == cIdent && !isTagKeyword(tokens[previous].text) {
		previous--
	}
	return previous >= start && isTagKeyword(tokens[previous].text)
}

func isTagKeyword(text string) bool {
	return text == "struct" || text == "union" || text == "enum"
}

func recordTypedefTag(statement []cToken, tags map[string]string) {
	if len(statement) < 4 || !isTagKeyword(statement[1].text) || statement[2].kind != cIdent {
		return
	}
	// The alias may repeat the tag (`typedef struct event_instr_t {...}
	// event_instr_t;`); that still means `struct event_instr_t` and
	// `event_instr_t` name one type.
	last := statement[len(statement)-1]
	if last.kind == cIdent {
		tags[statement[1].text+" "+statement[2].text] = last.text
	}
}

// parseDeclaratorList splits `T a, *b, c[2]` on top-level commas. Later
// declarators inherit the specifier of the first.
func parseDeclaratorList(statement []cToken, definition bool) []scannedDeclaration {
	for _, token := range statement {
		if token.text == "static" || token.text == "typedef" {
			return nil
		}
	}
	parts := splitTopLevel(statement, ",")
	specifier, begin, implicit := parseDeclarationSpecifier(parts[0])
	var declarations []scannedDeclaration
	for index, part := range parts {
		declarator := part
		if index == 0 {
			declarator = part[begin:]
		}
		symbol, signature, ok := parseDeclarationDeclarator(specifier, declarator)
		if !ok && index == 0 && implicit {
			// `func_80012345();` declares an implicit-int function.
			symbol, signature, ok = parseDeclarationDeclarator("s32", part[begin-1:])
		}
		if !ok {
			if index == 0 {
				return nil
			}
			continue
		}
		declarations = append(declarations, scannedDeclaration{
			symbol:     symbol,
			signature:  signature,
			line:       part[0].start, // converted to a line number by the caller
			definition: definition,
		})
	}
	return declarations
}

func splitTopLevel(tokens []cToken, separator string) [][]cToken {
	var parts [][]cToken
	depth := 0
	begin := 0
	for index, token := range tokens {
		switch token.text {
		case "(", "[", "{":
			depth++
		case ")", "]", "}":
			depth--
		case separator:
			if depth == 0 {
				parts = append(parts, tokens[begin:index])
				begin = index + 1
			}
		}
	}
	return append(parts, tokens[begin:])
}

// parseDeclarationSpecifier renders the leading type specifier and returns the
// index where the declarator begins. implicit reports that the specifier was a
// lone identifier, which may instead be the name of an implicit-int function.
func parseDeclarationSpecifier(part []cToken) (string, int, bool) {
	var words []string
	specifier := ""
	index := 0
	for index < len(part) && part[index].kind == cIdent {
		text := part[index].text
		switch {
		case declarationQualifiers[text]:
		case isTagKeyword(text):
			if specifier != "" || len(words) > 0 || index+1 >= len(part) || part[index+1].kind != cIdent {
				return specifier, index, false
			}
			specifier = text + " " + part[index+1].text
			index++
		case declarationBaseWords[text]:
			if specifier != "" {
				return specifier, index, false
			}
			words = append(words, text)
		default:
			if specifier != "" || len(words) > 0 {
				return normalizeBaseType(specifier, words), index, false
			}
			specifier = text
			if index+1 < len(part) && part[index+1].text == "(" {
				return specifier, index + 1, true
			}
		}
		index++
	}
	return normalizeBaseType(specifier, words), index, false
}

// normalizeBaseType spells C base types with the project typedefs from
// psx/types.h, so `int` and `s32` compare equal.
func normalizeBaseType(specifier string, words []string) string {
	if len(words) == 0 {
		return specifier
	}
	count := map[string]int{}
	for _, word := range words {
		count[word]++
	}
	unsigned := count["unsigned"] > 0
	switch {
	case count["void"] > 0:
		return "void"
	case count["float"] > 0:
		return "float"
	case count["double"] > 0:
		return "double"
	case count["char"] > 0:
		if unsigned {
			return "u8"
		}
		return "s8"
	case count["short"] > 0:
		if unsigned {
			return "u16"
		}
		return "s16"
	case count["long"] > 1:
		if unsigned {
			return "u64"
		}
		return "s64"
	case unsigned:
		return "u32"
	default:
		return "s32"
	}
}

// parseDeclarationDeclarator renders one declarator (after the specifier) and
// returns the symbol it binds. A trailing `__asm__("name")` linkage alias keys
// the declaration under the linked name, since it is another spelling of that
// symbol.
func parseDeclarationDeclarator(specifier string, part []cToken) (string, []string, bool) {
	if specifier == "" {
		return "", nil, false
	}
	if equals := topLevelIndex(part, "="); equals >= 0 {
		part = part[:equals]
	}
	alias := ""
	if n := len(part); n >= 4 && part[n-1].text == ")" && part[n-2].kind == cString && part[n-3].text == "(" &&
		(part[n-4].text == "__asm__" || part[n-4].text == "__asm" || part[n-4].text == "asm") {
		alias = strings.Trim(part[n-2].text, `"`)
		part = part[:n-4]
	}
	name := boundIdentifier(part)
	if name < 0 {
		return "", nil, false
	}
	rendered, ok := renderDeclarator(part, name, 0, len(part), true)
	if !ok {
		return "", nil, false
	}
	symbol := part[name].text
	if alias != "" {
		symbol = alias
	}
	return symbol, append([]string{specifier}, rendered...), true
}

func topLevelIndex(tokens []cToken, text string) int {
	depth := 0
	for index, token := range tokens {
		switch token.text {
		case "(", "[", "{":
			depth++
		case ")", "]", "}":
			depth--
		case text:
			if depth == 0 {
				return index
			}
		}
	}
	return -1
}

// boundIdentifier finds the identifier a declarator binds, or -1 for an
// abstract declarator. It only descends into a parenthesized group starting
// with '*' or '(' — a nested declarator such as `(*name)`. Any other group is
// a parameter list, whose identifiers name parameters rather than the symbol.
func boundIdentifier(part []cToken) int {
	for index := 0; index < len(part); index++ {
		token := part[index]
		switch {
		case token.text == "*" || declarationQualifiers[token.text]:
		case token.text == "(":
			close := matchingToken(part, index)
			if close <= index+1 || (part[index+1].text != "*" && part[index+1].text != "(") {
				return -1
			}
			inner := boundIdentifier(part[index+1 : close])
			if inner < 0 {
				return -1
			}
			return index + 1 + inner
		case token.kind == cIdent:
			return index
		default:
			return -1
		}
	}
	return -1
}

// renderDeclarator renders tokens [low, high) of a declarator with '@' for the
// bound name (or nothing when keepName is false), array extents collapsed to
// `[]`, and parameter lists normalized into single segments.
func renderDeclarator(part []cToken, name, low, high int, keepName bool) ([]string, bool) {
	var segments []string
	for index := low; index < high; index++ {
		token := part[index]
		switch {
		case index == name:
			if keepName {
				segments = append(segments, "@")
			}
		case token.text == "(":
			close := matchingToken(part[:high], index)
			if close < 0 {
				return nil, false
			}
			if name > index && name < close {
				inner, ok := renderDeclarator(part, name, index+1, close, keepName)
				if !ok {
					return nil, false
				}
				segments = append(segments, "(")
				segments = append(segments, inner...)
				segments = append(segments, ")")
			} else {
				parameters, ok := normalizeParameters(part[index+1 : close])
				if !ok {
					return nil, false
				}
				segments = append(segments, parameters)
			}
			index = close
		case token.text == "[":
			close := matchingToken(part[:high], index)
			if close < 0 {
				return nil, false
			}
			segments = append(segments, "[]")
			index = close
		case declarationQualifiers[token.text]:
		case token.text == "*" || token.text == ")":
			segments = append(segments, token.text)
		default:
			// Anything else (a literal, an operator) means this is not a declarator.
			return nil, false
		}
	}
	return segments, true
}

// normalizeParameters renders a parameter list without parameter names. An
// unprototyped `()` renders as `(...)`.
func normalizeParameters(parameters []cToken) (string, bool) {
	if len(parameters) == 0 {
		return "(...)", true
	}
	var rendered []string
	for _, part := range splitTopLevel(parameters, ",") {
		if len(part) == 1 && part[0].text == "..." {
			rendered = append(rendered, "...")
			continue
		}
		specifier, begin, _ := parseDeclarationSpecifier(part)
		if specifier == "" {
			return "", false
		}
		declarator := decayArrayParameter(part[begin:])
		name := boundIdentifier(declarator)
		segments, ok := renderDeclarator(declarator, name, 0, len(declarator), false)
		if !ok {
			return "", false
		}
		rendered = append(rendered, strings.Join(append([]string{specifier}, segments...), " "))
	}
	if len(rendered) == 1 && rendered[0] == "void" {
		return "(void)", true
	}
	return "(" + strings.Join(rendered, ", ") + ")", true
}

// decayArrayParameter rewrites a parameter `T name[N]` as `T *name`, which is
// the type C gives it.
func decayArrayParameter(declarator []cToken) []cToken {
	for index, token := range declarator {
		if token.text == "(" {
			return declarator
		}
		if token.text != "[" {
			continue
		}
		close := matchingToken(declarator, index)
		if close < 0 {
			return declarator
		}
		decayed := []cToken{{kind: cPunct, text: "*"}}
		decayed = append(decayed, declarator[:index]...)
		return append(decayed, declarator[close+1:]...)
	}
	return declarator
}

func isParameterSegment(segment string) bool {
	return len(segment) > 1 && segment[0] == '('
}

// signaturesAgree compares rendered types. An unprototyped `(...)` agrees with
// any parameter list in the same position; everything else, including the
// return type, must match.
func signaturesAgree(a, b []string) bool {
	if len(a) != len(b) {
		return false
	}
	for index := range a {
		if a[index] == b[index] {
			continue
		}
		if isParameterSegment(a[index]) && isParameterSegment(b[index]) && (a[index] == "(...)" || b[index] == "(...)") {
			continue
		}
		return false
	}
	return true
}

func renderSignature(signature []string, symbol string) string {
	return strings.ReplaceAll(strings.Join(signature, " "), "@", symbol)
}

func hasUnprototypedList(signature []string) bool {
	for _, segment := range signature {
		if segment == "(...)" {
			return true
		}
	}
	return false
}

func (group declarationGroup) conflicting() bool {
	for left := range group.spellings {
		for right := left + 1; right < len(group.spellings); right++ {
			if !signaturesAgree(group.spellings[left].signature, group.spellings[right].signature) {
				return true
			}
		}
	}
	return false
}

// collectDeclarationGroups scans dirs under root and groups every file-scope
// declaration by symbol.
func collectDeclarationGroups(root string, dirs []string) ([]declarationGroup, error) {
	sitesBySymbol := map[string][]declarationSite{}
	tags := map[string]string{}
	for _, dir := range dirs {
		err := filepath.WalkDir(filepath.Join(root, dir), func(path string, entry fs.DirEntry, err error) error {
			if err != nil {
				return err
			}
			if entry.IsDir() || (filepath.Ext(path) != ".c" && filepath.Ext(path) != ".h") {
				return nil
			}
			data, err := os.ReadFile(path)
			if err != nil {
				return err
			}
			relative, err := filepath.Rel(root, path)
			if err != nil {
				return err
			}
			relative = filepath.ToSlash(relative)
			declarations, fileTags, err := scanFileScopeDeclarations(string(data))
			if err != nil {
				return fmt.Errorf("%s: %w", relative, err)
			}
			for tag, alias := range fileTags {
				tags[tag] = alias
			}
			for _, declaration := range declarations {
				sitesBySymbol[declaration.symbol] = append(sitesBySymbol[declaration.symbol], declarationSite{
					path:       relative,
					line:       declaration.line,
					signature:  declaration.signature,
					definition: declaration.definition,
				})
			}
			return nil
		})
		if err != nil && !errors.Is(err, fs.ErrNotExist) {
			return nil, err
		}
	}
	replaceTag := tagReplacer(tags)
	groups := make([]declarationGroup, 0, len(sitesBySymbol))
	for symbol, sites := range sitesBySymbol {
		for index := range sites {
			for segment := range sites[index].signature {
				sites[index].signature[segment] = replaceTag(sites[index].signature[segment])
			}
		}
		groups = append(groups, newDeclarationGroup(symbol, sites))
	}
	sort.Slice(groups, func(i, j int) bool { return groups[i].symbol < groups[j].symbol })
	return groups, nil
}

// tagReplacer spells `struct tag` as its typedef alias so both spellings of
// one type compare equal.
func tagReplacer(tags map[string]string) func(string) string {
	if len(tags) == 0 {
		return func(segment string) string { return segment }
	}
	names := make([]string, 0, len(tags))
	for tag := range tags {
		names = append(names, regexp.QuoteMeta(tag))
	}
	sort.Strings(names)
	pattern := regexp.MustCompile(`\b(?:` + strings.Join(names, "|") + `)\b`)
	return func(segment string) string {
		if !strings.Contains(segment, "struct ") && !strings.Contains(segment, "union ") && !strings.Contains(segment, "enum ") {
			return segment
		}
		return pattern.ReplaceAllStringFunc(segment, func(tag string) string { return tags[tag] })
	}
}

func newDeclarationGroup(symbol string, sites []declarationSite) declarationGroup {
	sort.SliceStable(sites, func(i, j int) bool {
		if sites[i].definition != sites[j].definition {
			return sites[i].definition
		}
		iHeader, jHeader := strings.HasPrefix(sites[i].path, "include/"), strings.HasPrefix(sites[j].path, "include/")
		if iHeader != jHeader {
			return iHeader
		}
		if sites[i].path != sites[j].path {
			return sites[i].path < sites[j].path
		}
		return sites[i].line < sites[j].line
	})
	group := declarationGroup{symbol: symbol, sites: sites}
	index := map[string]int{}
	for _, site := range sites {
		key := strings.Join(site.signature, " ")
		position, ok := index[key]
		if !ok {
			position = len(group.spellings)
			index[key] = position
			group.spellings = append(group.spellings, declarationSpelling{signature: site.signature})
		}
		group.spellings[position].sites = append(group.spellings[position].sites, site)
	}
	sort.SliceStable(group.spellings, func(i, j int) bool {
		return len(group.spellings[i].sites) > len(group.spellings[j].sites)
	})
	prototyped := false
	for _, spelling := range group.spellings {
		if !hasUnprototypedList(spelling.signature) {
			prototyped = true
		}
	}
	best := 0
	for _, candidate := range group.spellings {
		if prototyped && hasUnprototypedList(candidate.signature) {
			continue
		}
		agreeing := 0
		for _, site := range sites {
			if signaturesAgree(site.signature, candidate.signature) {
				agreeing++
			}
		}
		if agreeing > best {
			best = agreeing
		}
	}
	group.minority = len(sites) - best
	return group
}

func conflictingDeclarationGroups(groups []declarationGroup) []declarationGroup {
	var conflicts []declarationGroup
	for _, group := range groups {
		if group.conflicting() {
			conflicts = append(conflicts, group)
		}
	}
	return conflicts
}

// validateDeclarations fails when any symbol has conflicting file-scope
// declarations.
func validateDeclarations(root string) error {
	groups, err := collectDeclarationGroups(root, declarationScanDirs)
	if err != nil {
		return err
	}
	conflicts := conflictingDeclarationGroups(groups)
	if len(conflicts) > 0 {
		names := make([]string, len(conflicts))
		for index, group := range conflicts {
			names[index] = group.symbol
		}
		return fmt.Errorf("declarations: %d symbols have conflicting file-scope declarations (see `make declarations`): %s", len(names), abbreviatedList(names, 10))
	}
	return nil
}

func abbreviatedList(names []string, limit int) string {
	if len(names) <= limit {
		return strings.Join(names, ", ")
	}
	return fmt.Sprintf("%s, ... %d more", strings.Join(names[:limit], ", "), len(names)-limit)
}

// symbolList collects repeated --symbol flags, so asking about several symbols
// reports all of them rather than silently keeping only the last.
type symbolList []string

func (list *symbolList) String() string { return strings.Join(*list, ",") }

func (list *symbolList) Set(value string) error {
	for _, name := range strings.Split(value, ",") {
		if name = strings.TrimSpace(name); name != "" {
			*list = append(*list, name)
		}
	}
	return nil
}

func (list symbolList) contains(symbol string) bool {
	for _, name := range list {
		if name == symbol {
			return true
		}
	}
	return false
}

func (p project) declarationsCommand(args []string) error {
	flags := flag.NewFlagSet("declarations", flag.ContinueOnError)
	module := flags.String("module", "", "only report symbols declared under src/<module>/")
	var symbols symbolList
	flags.Var(&symbols, "symbol", "only report this symbol, conflicting or not; repeat or comma-separate for several")
	allFiles := flags.Bool("all-files", false, "list every declaring file instead of the first three per spelling")
	if err := flags.Parse(args); err != nil {
		return err
	}
	if flags.NArg() != 0 {
		return errors.New("usage: tools declarations [--module=DIR] [--symbol=NAME] [--all-files]")
	}
	groups, err := collectDeclarationGroups(p.root, declarationScanDirs)
	if err != nil {
		return err
	}
	var selected []declarationGroup
	for _, group := range groups {
		if len(symbols) > 0 {
			if symbols.contains(group.symbol) {
				selected = append(selected, group)
			}
			continue
		}
		if !group.conflicting() {
			continue
		}
		if *module != "" && !groupTouchesModule(group, *module) {
			continue
		}
		selected = append(selected, group)
	}
	reportDeclarationGroups(selected, *allFiles)
	return nil
}

func groupTouchesModule(group declarationGroup, module string) bool {
	prefix := "src/" + strings.Trim(module, "/") + "/"
	for _, site := range group.sites {
		if strings.HasPrefix(site.path, prefix) {
			return true
		}
	}
	return false
}

// reportDeclarationGroups prints groups ranked by minority count, the number
// of declarations that must change for the symbol to agree.
func reportDeclarationGroups(groups []declarationGroup, allFiles bool) {
	sort.SliceStable(groups, func(i, j int) bool {
		if groups[i].minority != groups[j].minority {
			return groups[i].minority > groups[j].minority
		}
		return groups[i].symbol < groups[j].symbol
	})
	minority := 0
	for _, group := range groups {
		minority += group.minority
	}
	fmt.Printf("declarations: %d symbols, %d declarations to reconcile\n", len(groups), minority)
	for _, group := range groups {
		fmt.Printf("\n%s (%d declarations, %d to reconcile)\n", group.symbol, len(group.sites), group.minority)
		for _, spelling := range group.spellings {
			fmt.Printf("  %4dx  %s\n", len(spelling.sites), renderSignature(spelling.signature, group.symbol))
			limit := len(spelling.sites)
			if !allFiles && limit > 3 {
				limit = 3
			}
			var locations []string
			for _, site := range spelling.sites[:limit] {
				location := fmt.Sprintf("%s:%d", site.path, site.line)
				if site.definition {
					location += " (definition)"
				}
				locations = append(locations, location)
			}
			if extra := len(spelling.sites) - limit; extra > 0 {
				locations = append(locations, fmt.Sprintf("... %d more", extra))
			}
			fmt.Printf("         %s\n", strings.Join(locations, ", "))
		}
	}
}
