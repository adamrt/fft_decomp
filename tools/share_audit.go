// share_audit.go discovers lexical sharing candidates without changing sources
// or target contracts. Every proposed correspondence still needs review.
package main

import (
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"
)

type shareAuditOptions struct {
	minBytes        int
	modules         map[string]bool
	prefix          string
	normalizeLocals bool
}

type shareAuditReport struct {
	SchemaVersion            int               `json:"schema_version"`
	CandidateOnly            bool              `json:"candidate_only"`
	ReviewLimits             []string          `json:"review_limits"`
	ScannedEntries           int               `json:"scanned_entries"`
	Excluded                 map[string]int    `json:"excluded"`
	Groups                   []shareAuditGroup `json:"groups"`
	LocalNormalizationCounts map[string]int    `json:"local_name_normalization_counts,omitempty"`
}

type shareAuditGroup struct {
	ShapeHash    string            `json:"shape_sha256"`
	TokenCount   int               `json:"body_token_count"`
	Participants []shareAuditEntry `json:"participants"`
}

type shareAuditEntry struct {
	Module             string                        `json:"module"`
	Name               string                        `json:"name"`
	Address            string                        `json:"address"`
	Size               int                           `json:"size"`
	Hash               string                        `json:"hash"`
	Profile            string                        `json:"profile"`
	Source             string                        `json:"source"`
	SourceSymbol       string                        `json:"source_symbol"`
	DefinitionSymbol   string                        `json:"definition_symbol"`
	DefinitionContract shareAuditContract            `json:"definition_contract"`
	Roles              []shareAuditRole              `json:"ordered_roles"`
	LocalNormalization *shareAuditLocalNormalization `json:"local_name_normalization,omitempty"`
}

type shareAuditRole struct {
	Role     string             `json:"role"`
	Symbol   string             `json:"symbol"`
	Address  string             `json:"address"`
	Contract shareAuditContract `json:"contract"`
}

type shareAuditContract struct {
	Status    string                  `json:"status"`
	Signature []string                `json:"normalized_signature,omitempty"`
	Sites     []shareAuditDeclaration `json:"declaration_sites,omitempty"`
}

type shareAuditDeclaration struct {
	Path       string   `json:"path"`
	Line       int      `json:"line"`
	Definition bool     `json:"definition"`
	Signature  []string `json:"normalized_signature"`
	Raw        string   `json:"raw_declaration_excerpt"`
}

type shareAuditScanner struct {
	p               project
	config          *projectConfig
	resolver        symbolResolver
	contracts       map[string]shareAuditContract
	sources         map[string]string
	types           map[string]bool
	nonTagTypes     map[string]bool
	normalizeLocals bool
}

func (p project) shareAuditCommand(args []string) error {
	flags := flag.NewFlagSet("share-audit", flag.ContinueOnError)
	minBytes := flags.Int("min-bytes", 64, "minimum original function size")
	module := flags.String("module", "", "modules (ids, aliases, event or effect)")
	prefix := flags.String("prefix", "", "original public-name prefix")
	normalizeLocals := flags.Bool("normalize-locals", false, "normalize uniquely declared simple parameters and outer-block locals")
	if err := flags.Parse(args); err != nil {
		return err
	}
	if flags.NArg() != 0 || *minBytes < 0 {
		return errors.New("usage: tools share-audit [--min-bytes=N] [--module=MODULES] [--prefix=PREFIX] [--normalize-locals]")
	}
	config, err := p.loadProjectConfig()
	if err != nil {
		return err
	}
	options := shareAuditOptions{minBytes: *minBytes, prefix: *prefix, normalizeLocals: *normalizeLocals}
	if *module != "" {
		selected, err := resolveModuleSelection(config, *module)
		if err != nil {
			return err
		}
		options.modules = map[string]bool{}
		for _, m := range selected {
			options.modules[m.ID] = true
		}
	}
	report, err := p.shareAudit(config, options)
	if err != nil {
		return err
	}
	encoder := json.NewEncoder(p.stdout())
	encoder.SetIndent("", "  ")
	return encoder.Encode(report)
}

func newShareAuditScanner(p project, config *projectConfig) (*shareAuditScanner, error) {
	groups, err := collectDeclarationGroups(p.root, declarationScanDirs, config.Modules...)
	if err != nil {
		return nil, err
	}
	scanner := &shareAuditScanner{p: p, config: config, resolver: newSymbolResolver(config),
		contracts: map[string]shareAuditContract{}, sources: map[string]string{}, types: map[string]bool{}, nonTagTypes: map[string]bool{}}
	for word := range declarationBaseWords {
		scanner.types[word] = true
		scanner.nonTagTypes[word] = true
	}
	for _, group := range groups {
		contract := shareAuditContract{Status: "unknown"}
		if len(group.spellings) == 1 {
			contract.Status = "normalized_only"
			contract.Signature = append([]string(nil), group.spellings[0].signature...)
			if hasUnprototypedList(contract.Signature) {
				contract.Status = "unprototyped"
			}
		} else {
			contract.Status = "ambiguous"
		}
		for _, site := range group.sites {
			source, err := scanner.source(site.path)
			if err != nil {
				return nil, err
			}
			contract.Sites = append(contract.Sites, shareAuditDeclaration{site.path, site.line,
				site.definition, site.signature, shareAuditRawDeclaration(source, site.line, group.symbol)})
			for _, segment := range site.signature {
				tokens, err := tokenizeC(segment)
				if err != nil {
					continue
				}
				for index, token := range tokens {
					if token.kind == cIdent {
						scanner.types[token.text] = true
						if index == 0 || !isTagKeyword(tokens[index-1].text) {
							scanner.nonTagTypes[token.text] = true
						}
					}
				}
			}
		}
		scanner.contracts[group.symbol] = contract
	}
	return scanner, nil
}

func (scanner *shareAuditScanner) source(path string) (string, error) {
	if source, ok := scanner.sources[path]; ok {
		return source, nil
	}
	full, err := projectPath(scanner.p.root, path, "sharing source")
	if err != nil {
		return "", err
	}
	data, err := os.ReadFile(full)
	if err != nil {
		return "", err
	}
	source := string(data)
	scanner.sources[path] = source
	return source, nil
}

// Keep actual qualifiers and array extents visible beside the deliberately
// lossy declaration checker's normalized signature. This is an excerpt, not
// an independently parsed type contract.
func shareAuditRawDeclaration(source string, line int, symbol string) string {
	lines := strings.SplitAfter(source, "\n")
	if line < 1 || line > len(lines) {
		return ""
	}
	tail := strings.Join(lines[line-1:], "")
	tokens, err := tokenizeC(tail)
	if err != nil {
		return strings.TrimSpace(lines[line-1])
	}
	begin, depth := 0, 0
	for index := 0; index < len(tokens); index++ {
		token := tokens[index]
		switch token.text {
		case "(", "[":
			depth++
		case ")", "]":
			depth--
		case ";", "{":
			if depth != 0 {
				continue
			}
			for _, part := range tokens[begin:index] {
				if part.kind == cIdent && part.text == symbol {
					return strings.TrimSpace(tail[tokens[begin].start:token.end])
				}
			}
			if token.text == "{" {
				close := matchingToken(tokens, index)
				if close < 0 {
					return strings.TrimSpace(lines[line-1])
				}
				index = close
			}
			begin = index + 1
		}
	}
	return strings.TrimSpace(lines[line-1])
}

func (scanner *shareAuditScanner) contract(name string) shareAuditContract {
	if contract, ok := scanner.contracts[name]; ok {
		return contract
	}
	return shareAuditContract{Status: "unknown"}
}

func shareAuditContractKey(contract shareAuditContract, symbol string) string {
	if contract.Status == "normalized_only" {
		return strings.Join(contract.Signature, " ")
	}
	// Missing or ambiguous types must not normalize two different symbols
	// into an assumed compatible role.
	return contract.Status + ":" + symbol
}

func shareAuditDefinition(tokens []cToken, symbol string) ([]cToken, []cToken, bool) {
	for index, token := range tokens {
		if token.text != symbol || index+1 >= len(tokens) || tokens[index+1].text != "(" {
			continue
		}
		close := matchingToken(tokens, index+1)
		if close < 0 || close+1 >= len(tokens) || tokens[close+1].text != "{" {
			continue
		}
		end := matchingToken(tokens, close+1)
		if end < 0 {
			return nil, nil, false
		}
		return tokens[index+2 : close], tokens[close+1 : end+1], true
	}
	return nil, nil, false
}

// Conservatively preserve names declared as parameters or block locals. This
// bounded scan reuses the existing declarator parser; it is not C scope analysis.
func (scanner *shareAuditScanner) localNames(parameters, body []cToken) map[string]bool {
	names := map[string]bool{}
	collect := func(tokens []cToken) {
		if len(tokens) == 0 || !scanner.types[tokens[0].text] && !declarationQualifiers[tokens[0].text] && !isTagKeyword(tokens[0].text) {
			return
		}
		for _, declaration := range parseDeclaratorList(tokens, false) {
			names[declaration.symbol] = true
		}
	}
	for _, parameter := range splitTopLevel(parameters, ",") {
		collect(parameter)
	}
	for index, token := range body {
		start := index + 1
		if token.text != "{" && token.text != ";" {
			continue
		}
		end := start
		for end < len(body) && body[end].text != ";" && body[end].text != "{" && body[end].text != "}" {
			end++
		}
		collect(body[start:end])
	}
	return names
}

func (scanner *shareAuditScanner) entry(m *moduleSpec, f *functionSpec) (shareAuditEntry, string, int, string, error) {
	path := m.sourceOf(f)
	source, err := scanner.source(path)
	if err != nil {
		return shareAuditEntry{}, "", 0, "", err
	}
	tokens, err := tokenizeC(source)
	if err != nil {
		return shareAuditEntry{}, "", 0, "", err
	}
	parameters, body, found := shareAuditDefinition(tokens, f.sourceSymbol())
	if !found {
		return shareAuditEntry{}, "", 0, "unparsed_definition", nil
	}
	for _, token := range body {
		if token.text == "asm" || token.text == "__asm" || token.text == "__asm__" {
			return shareAuditEntry{}, "", 0, "assembly", nil
		}
	}
	used := map[string]struct{}{}
	for _, token := range body {
		if token.kind == cIdent {
			used[token.text] = struct{}{}
		}
	}
	bindings := scanner.resolver.linkerBindings(m, f, used)
	contract := scanner.contract(f.sourceSymbol())
	entry := shareAuditEntry{m.ID, f.Name, fmt.Sprintf("0x%08x", f.Addr), f.Size, f.Hash, m.profileOf(f),
		path, f.SourceSymbol, f.sourceSymbol(), contract, []shareAuditRole{}, nil}
	locals := scanner.localNames(parameters, body)
	roles := map[string]string{}
	selfSeen := false
	shape := []string{shareAuditContractKey(contract, f.sourceSymbol())}
	localRoles := map[string]string{}
	if scanner.normalizeLocals {
		var header []string
		localRoles, header, entry.LocalNormalization = scanner.normalizeLocalRoles(source, tokens, parameters, body, bindings, f.sourceSymbol())
		shape = append(shape, header...)
	}
	for index, token := range body {
		text := token.text
		member := index > 0 && (body[index-1].text == "." || body[index-1].text == "->")
		if role, ok := localRoles[text]; ok && token.kind == cIdent && !member {
			text = role
		} else if token.kind == cIdent && !member && !locals[text] {
			if text == f.sourceSymbol() {
				if !selfSeen {
					entry.Roles = append(entry.Roles, shareAuditRole{"$self", text, fmt.Sprintf("0x%08x", bindings[text]), contract})
					selfSeen = true
				}
				text = "$self"
			} else if address, ok := bindings[text]; ok {
				if role, seen := roles[text]; seen {
					text = role
				} else {
					symbol := text
					role := fmt.Sprintf("$role%d", len(roles))
					bindingContract := scanner.contract(symbol)
					roles[symbol] = role
					entry.Roles = append(entry.Roles, shareAuditRole{role, symbol, fmt.Sprintf("0x%08x", address), bindingContract})
					shape = append(shape, role+":"+shareAuditContractKey(bindingContract, symbol))
					text = role
				}
			}
		}
		shape = append(shape, text)
	}
	digest := sha256.Sum256([]byte(strings.Join(shape, "\x00")))
	return entry, hex.EncodeToString(digest[:]), len(body), "", nil
}

func (p project) shareAudit(config *projectConfig, options shareAuditOptions) (shareAuditReport, error) {
	report := shareAuditReport{SchemaVersion: 1, CandidateOnly: true, Excluded: map[string]int{}, Groups: []shareAuditGroup{},
		ReviewLimits: []string{
			"Candidate discovery only: normalized ABI signatures do not prove record layout, mutability, aliases, runtime residency or byte-exact sharing.",
			"The declaration scanner drops qualifiers, storage classes and array extents; inspect every raw declaration site and caller before changing contracts.",
			"Bodies are lexical, not preprocessed: includes, macro expansions, conditional compilation, compiler-emitted symbols and source-file setup are not proven equivalent.",
			"Local/parameter shadow detection is conservative and incomplete; member names, local names, operators, literals and unknown identifiers are retained.",
			"Unknown, ambiguous or unprototyped declarations retain symbol identity and need explicit review; different known normalized signatures do not group.",
		}}
	scanner, err := newShareAuditScanner(p, config)
	if err != nil {
		return report, err
	}
	scanner.normalizeLocals = options.normalizeLocals
	if options.normalizeLocals {
		report.LocalNormalizationCounts = map[string]int{}
		report.ReviewLimits = append(report.ReviewLimits, "Opt-in local normalization retains full definition-header/type/qualifier/extent/member tokens; nested, complex or macro-ambiguous cases fall back to exact comparison. Ordered local correspondence is candidate evidence only.")
	}
	groups := map[string]*shareAuditGroup{}
	for _, m := range config.Modules {
		if options.modules != nil && !options.modules[m.ID] {
			continue
		}
		for index := range m.Functions {
			f := &m.Functions[index]
			if f.Size < options.minBytes || !strings.HasPrefix(f.Name, options.prefix) {
				continue
			}
			report.ScannedEntries++
			source := filepath.ToSlash(m.sourceOf(f))
			reason := ""
			if f.Asm != "" || filepath.Ext(source) != ".c" {
				reason = "assembly"
			}
			if m.isLibraryFunction(f) {
				reason = "library"
			}
			if reason != "" {
				report.Excluded[reason]++
				continue
			}
			entry, key, count, reason, err := scanner.entry(m, f)
			if err != nil {
				return report, fmt.Errorf("%s %s: %w", m.ID, f.Name, err)
			}
			if reason != "" {
				report.Excluded[reason]++
				continue
			}
			if entry.LocalNormalization != nil {
				status := entry.LocalNormalization.Status
				if entry.LocalNormalization.Reason != "" {
					status += ":" + entry.LocalNormalization.Reason
				}
				report.LocalNormalizationCounts[status]++
			}
			if groups[key] == nil {
				groups[key] = &shareAuditGroup{ShapeHash: key, TokenCount: count}
			}
			groups[key].Participants = append(groups[key].Participants, entry)
		}
	}
	for _, group := range groups {
		if len(group.Participants) < 2 {
			continue
		}
		sources := map[string]bool{}
		for _, entry := range group.Participants {
			sources[entry.Source] = true
		}
		if len(sources) < 2 {
			report.Excluded["already_single_source_groups"]++
			continue
		}
		sort.Slice(group.Participants, func(i, j int) bool {
			a, b := group.Participants[i], group.Participants[j]
			if a.Module != b.Module {
				return a.Module < b.Module
			}
			if a.Address != b.Address {
				return a.Address < b.Address
			}
			return a.Name < b.Name
		})
		report.Groups = append(report.Groups, *group)
	}
	sort.Slice(report.Groups, func(i, j int) bool { return report.Groups[i].ShapeHash < report.Groups[j].ShapeHash })
	return report, nil
}
