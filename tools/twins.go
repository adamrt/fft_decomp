// twins.go groups functions across modules whose instruction streams match
// once every call and address operand is read as a symbol name, under a
// consistent one-to-one renaming. Division-check guards are stripped so a
// checked and an unchecked build of one source compare equal, and branch
// targets are expressed as instruction indices in the stripped stream.
package main

import (
	"bytes"
	"crypto/sha1"
	"encoding/binary"
	"encoding/hex"
	"errors"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"

	"gopkg.in/yaml.v3"
)

const twinsSchemaVersion = 1

type twinsOptions struct {
	modules  map[string]bool
	minBytes int
	maxNear  int
	out      string
}

// twinSymbol is one address-ordered symbol a module can resolve: a function
// (with its size), a data row or an import (extending to the next symbol).
type twinSymbol struct {
	addr, end uint32
	name      string
}

// twinScope resolves addresses to names for one module: own symbols first,
// then the linked modules', then main's, as the linker binding does.
type twinScope struct {
	exact  map[uint32]string
	ranges []twinSymbol // sorted by addr, first occurrence wins
}

func newTwinScope(config *projectConfig, m *moduleSpec) *twinScope {
	scope := &twinScope{exact: map[uint32]string{}}
	add := func(module *moduleSpec, imports bool) {
		functions := append([]functionSpec(nil), module.Functions...)
		sort.SliceStable(functions, func(i, j int) bool { return functions[i].Addr < functions[j].Addr })
		for _, f := range functions {
			if _, seen := scope.exact[f.Addr]; !seen {
				scope.exact[f.Addr] = f.Name
				scope.ranges = append(scope.ranges, twinSymbol{f.Addr, f.Addr + uint32(f.Size), f.Name})
			}
			for index, rodata := range f.Rodata {
				name := f.Name + ".rodata"
				if index > 0 {
					name = fmt.Sprintf("%s.rodata%d", f.Name, index)
				}
				if _, seen := scope.exact[rodata.Addr]; !seen {
					scope.exact[rodata.Addr] = name
					scope.ranges = append(scope.ranges, twinSymbol{rodata.Addr, rodata.Addr + uint32(rodata.Size), name})
				}
			}
		}
		lists := [][]symbolSpec{module.Data}
		if imports {
			lists = append(lists, module.Imports)
		}
		for _, list := range lists {
			for _, symbol := range list {
				if _, seen := scope.exact[symbol.Addr]; !seen {
					scope.exact[symbol.Addr] = symbol.Name
					scope.ranges = append(scope.ranges, twinSymbol{symbol.Addr, 0, symbol.Name})
				}
			}
		}
	}
	add(m, true)
	for _, link := range m.Links {
		if linked, ok := config.module(link); ok {
			add(linked, false)
		}
	}
	if m.ID != mainModuleID {
		if main, ok := config.module(mainModuleID); ok {
			add(main, false)
		}
	}
	sort.SliceStable(scope.ranges, func(i, j int) bool { return scope.ranges[i].addr < scope.ranges[j].addr })
	// A data row extends to the next symbol, at most 0x1000 bytes.
	for index := range scope.ranges {
		symbol := &scope.ranges[index]
		if symbol.end != 0 {
			continue
		}
		symbol.end = symbol.addr + 0x1000
		if index+1 < len(scope.ranges) && scope.ranges[index+1].addr < symbol.end {
			symbol.end = scope.ranges[index+1].addr
		}
	}
	return scope
}

// resolve names an address: the symbol at it, or symbol+offset inside a
// known range, or "" when nothing covers it.
func (scope *twinScope) resolve(addr uint32) (string, uint32) {
	if name, ok := scope.exact[addr]; ok {
		return name, 0
	}
	index := sort.Search(len(scope.ranges), func(i int) bool { return scope.ranges[i].addr > addr }) - 1
	for ; index >= 0; index-- {
		symbol := scope.ranges[index]
		if addr >= symbol.addr && addr < symbol.end {
			return symbol.name, addr - symbol.addr
		}
		if addr-symbol.addr > 0x1000 {
			break
		}
	}
	return "", 0
}

// twinFunction is one function's symbolic stream.
type twinFunction struct {
	m          *moduleSpec
	f          *functionSpec
	source     string
	tokens     []string // canonical stream: symbol slots as #k
	mnemonics  []string // the same with every operand masked
	symbols    []string // slot k's symbol name, or 0x-address when unresolved
	unresolved int
	divchecks  int
}

func (fn *twinFunction) label() string { return fn.m.ID + ":" + fn.f.Name }

// twinSymbolize builds the canonical stream of one function.
func twinSymbolize(m *moduleSpec, f *functionSpec, code []byte, scope *twinScope) *twinFunction {
	fn := &twinFunction{m: m, f: f, source: m.sourceOf(f)}
	words := make([]uint32, len(code)/4)
	for i := range words {
		words[i] = binary.LittleEndian.Uint32(code[4*i:])
	}
	// Pass 1: find division-check guards to skip.
	skip := make([]bool, len(words))
	for i := 0; i < len(words); i++ {
		w := words[i]
		if w>>26 != 0 || (w&63 != 26 && w&63 != 27) || w>>11&31 != 0 {
			continue
		}
		rs, rt := w>>21&31, w>>16&31
		if i+3 >= len(words) || words[i+1] != 0x14000002|rt<<21 || words[i+2] != 0 || words[i+3] != 0x0007000d {
			continue
		}
		count := 3
		if w&63 == 26 && i+9 < len(words) && words[i+4] == 0x2401ffff && words[i+5] == 0x14010004|rt<<21 &&
			words[i+6] == 0x3c018000 && words[i+7] == 0x14010002|rs<<21 && words[i+8] == 0 && words[i+9] == 0x0006000d {
			count = 9
		}
		for k := 1; k <= count; k++ {
			skip[i+k] = true
		}
		fn.divchecks++
	}
	index := make([]int, len(words)+1) // original word -> stripped index
	kept := 0
	for i := range words {
		index[i] = kept
		if !skip[i] {
			kept++
		}
	}
	index[len(words)] = kept
	slots := map[string]int{}
	slot := func(name string) string {
		k, ok := slots[name]
		if !ok {
			k = len(slots)
			slots[name] = k
			fn.symbols = append(fn.symbols, name)
		}
		return fmt.Sprintf("#%d", k)
	}
	symbolToken := func(addr uint32) string {
		name, offset := scope.resolve(addr)
		if name == "" {
			fn.unresolved++
			return slot(fmt.Sprintf("0x%08x", addr))
		}
		if offset != 0 {
			return fmt.Sprintf("%s+0x%x", slot(name), offset)
		}
		return slot(name)
	}
	// Pass 2: tokens. lui values are tracked per register until the register
	// is written again; the first %lo partner names the lui.
	type luiState struct {
		value uint32
		token int // token index to rewrite
		set   bool
	}
	var lui [32]luiState
	tokens := make([]string, 0, kept)
	masked := make([]string, 0, kept)
	for i, w := range words {
		if skip[i] {
			continue
		}
		pc := uint32(4 * i)
		op := w >> 26
		rs, rt, rd := w>>21&31, w>>16&31, w>>11&31
		imm := int32(int16(w & 0xffff))
		line := decodeMIPS(w, pc)
		text := line.row()
		mask := line.mnemonic
		dest := -1
		switch {
		case op == 0:
			switch w & 63 {
			case 8, 12, 13, 17, 19, 24, 25, 26, 27:
			default:
				dest = int(rd)
			}
			if w&63 == 9 && rd == 0 {
				dest = 31
			}
			mask += fmt.Sprintf(" %d,%d,%d", rd, rs, rt)
		case op == 1:
			if rt == 16 || rt == 17 {
				dest = 31
			}
			text = branchToken(line, index[int(pc/4)+1+int(imm)])
			mask += fmt.Sprintf(" %d", rs)
		case op == 2 || op == 3:
			if op == 3 {
				dest = 31
			}
			target := (f.Addr+pc+4)&0xf0000000 | (w&0x03ffffff)<<2
			if target >= f.Addr && target < f.Addr+uint32(f.Size) {
				text = fmt.Sprintf("%s\t@%d", line.mnemonic, index[(target-f.Addr)/4])
			} else {
				text = line.mnemonic + "\t" + symbolToken(target)
			}
		case op >= 4 && op <= 7:
			text = branchToken(line, index[int(pc/4)+1+int(imm)])
			mask += fmt.Sprintf(" %d,%d", rs, rt)
		case op == 15:
			dest = int(rt)
			mask += fmt.Sprintf(" %d", rt)
		case op >= 8 && op <= 14:
			dest = int(rt)
			mask += fmt.Sprintf(" %d,%d", rt, rs)
		case op >= 16 && op <= 19:
			if rs == 0 || rs == 2 { // mfc, cfc
				dest = int(rt)
			}
			mask += fmt.Sprintf(" %x", w&0x03ffffff)
		case op >= 32 && op <= 38:
			dest = int(rt)
			mask += fmt.Sprintf(" %d,%d", rt, rs)
		case op >= 40 && op <= 46 || op >= 48 && op <= 51 || op >= 56 && op <= 59:
			mask += fmt.Sprintf(" %d,%d", rt, rs)
		}
		// %lo partners: addiu/addi and memory accesses whose base holds a lui.
		if (op == 8 || op == 9 || op >= 32 && op <= 46 || op >= 48 && op <= 51 || op >= 56 && op <= 59) && rs != 28 && lui[rs].set {
			addr := lui[rs].value + uint32(imm)
			symbol := symbolToken(addr)
			if op == 8 || op == 9 {
				text = fmt.Sprintf("%s\t%s,%s,%%lo(%s)", line.mnemonic, mipsRegisterNames[rt], mipsRegisterNames[rs], symbol)
			} else {
				text = fmt.Sprintf("%s\t%s,%%lo(%s)(%s)", line.mnemonic, mipsRegisterNames[rt], symbol, mipsRegisterNames[rs])
			}
			if hi := lui[rs].token; hi >= 0 {
				tokens[hi] = fmt.Sprintf("lui\t%s,%%hi(%s)", mipsRegisterNames[rs], symbol)
				lui[rs].token = -1
			}
		}
		tokens = append(tokens, text)
		masked = append(masked, mask)
		if dest > 0 {
			lui[dest] = luiState{}
		}
		if op == 15 && rt != 0 {
			lui[rt] = luiState{value: uint32(w&0xffff) << 16, token: len(tokens) - 1, set: true}
		}
	}
	fn.tokens, fn.mnemonics = tokens, masked
	return fn
}

// branchToken rewrites a decoded branch with its target as a stripped
// stream index, keeping the decoder's operand spelling (beqz, bnez).
func branchToken(line mipsLine, target int) string {
	args := line.args
	if comma := strings.LastIndex(args, ","); comma >= 0 {
		args = args[:comma+1]
	} else {
		args = ""
	}
	return fmt.Sprintf("%s\t%s@%d", line.mnemonic, args, target)
}

func twinKey(tokens []string) string {
	sum := sha1.Sum([]byte(strings.Join(tokens, "\n")))
	return hex.EncodeToString(sum[:8])
}

// twinsReport is the YAML report.
type twinsReport struct {
	SchemaVersion int                `yaml:"schema_version"`
	CandidateOnly bool               `yaml:"candidate_only"`
	Options       twinsReportOptions `yaml:"options"`
	Totals        twinsReportTotals  `yaml:"totals"`
	Groups        []twinsGroup       `yaml:"groups"`
	NearMisses    []twinsNearMiss    `yaml:"near_misses"`
}

type twinsReportOptions struct {
	MinBytes int `yaml:"min_bytes"`
	MaxNear  int `yaml:"max_near"`
}

type twinsReportTotals struct {
	Functions     int `yaml:"functions"`
	Groups        int `yaml:"groups"`
	Shared        int `yaml:"shared"`
	Mergeable     int `yaml:"mergeable"`
	NeedsRenames  int `yaml:"needs_renames"`
	NearMisses    int `yaml:"near_misses"`
	BytesUnlocked int `yaml:"bytes_unlocked"`
}

type twinsMember struct {
	Module     string `yaml:"module"`
	Name       string `yaml:"name"`
	Addr       string `yaml:"addr"`
	Size       int    `yaml:"size"`
	Profile    string `yaml:"profile,omitempty"`
	Source     string `yaml:"source"`
	Divchecks  int    `yaml:"divchecks,omitempty"`
	Unresolved int    `yaml:"unresolved,omitempty"`
}

type twinsGroup struct {
	ID         string              `yaml:"id"`
	Status     string              `yaml:"status"` // shared, mergeable, needs-renames
	Bytes      int                 `yaml:"bytes"`
	Unlocked   int                 `yaml:"unlocked"`
	SameModule bool                `yaml:"same_module,omitempty"`
	Modules    []string            `yaml:"modules"`
	Sources    []string            `yaml:"sources"`
	Members    []twinsMember       `yaml:"members"`
	Unified    int                 `yaml:"unified"`
	Renames    []map[string]string `yaml:"renames,omitempty"`
}

type twinsNearMiss struct {
	ID          string              `yaml:"id"`
	Bytes       int                 `yaml:"bytes"`
	Differences int                 `yaml:"differences"`
	Members     []twinsMember       `yaml:"members"`
	Lines       []map[string]string `yaml:"lines"`
}

func twinMember(fn *twinFunction) twinsMember {
	member := twinsMember{Module: fn.m.ID, Name: fn.f.Name, Addr: fmt.Sprintf("0x%08x", fn.f.Addr), Size: fn.f.Size,
		Source: fn.source, Divchecks: fn.divchecks, Unresolved: fn.unresolved}
	if fn.m.profileOf(fn.f) != fn.m.Profile {
		member.Profile = fn.m.profileOf(fn.f)
	}
	return member
}

// memberKeys names each member of a group for rename rows: the module id,
// or module:name when a module appears twice.
func memberKeys(members []*twinFunction) []string {
	counts := map[string]int{}
	for _, fn := range members {
		counts[fn.m.ID]++
	}
	keys := make([]string, len(members))
	for index, fn := range members {
		keys[index] = fn.m.ID
		if counts[fn.m.ID] > 1 {
			keys[index] = fn.label()
		}
	}
	return keys
}

func (p project) twinsCommand(args []string) error {
	flags := flag.NewFlagSet("twins", flag.ContinueOnError)
	modules := flags.String("module", "", "modules to scan (default all)")
	minBytes := flags.Int("min-bytes", 48, "smallest function to group")
	maxNear := flags.Int("max-near", 6, "most differing instructions for a near-miss pair")
	out := flags.String("out", "build/analysis/twins", "output directory under build/")
	if err := flags.Parse(args); err != nil {
		return err
	}
	if flags.NArg() != 0 {
		return errors.New("twins accepts named flags only")
	}
	if clean := filepath.ToSlash(filepath.Clean(*out)); clean != "build" && !strings.HasPrefix(clean, "build/") {
		return fmt.Errorf("twins output must lie under build/: %s", *out)
	}
	config, err := p.loadProjectConfig()
	if err != nil {
		return err
	}
	options := twinsOptions{minBytes: *minBytes, maxNear: *maxNear, out: *out}
	if *modules != "" {
		selected, err := resolveModuleSelection(config, *modules)
		if err != nil {
			return err
		}
		options.modules = map[string]bool{}
		for _, m := range selected {
			options.modules[m.ID] = true
		}
	}
	report, err := p.findTwins(config, options)
	if err != nil {
		return err
	}
	dir := filepath.Join(p.root, filepath.FromSlash(filepath.Clean(*out)))
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return err
	}
	var buffer bytes.Buffer
	encoder := yaml.NewEncoder(&buffer)
	encoder.SetIndent(2)
	if err := encoder.Encode(report); err != nil {
		return err
	}
	if err := atomicWrite(filepath.Join(dir, "twins.yaml"), buffer.Bytes()); err != nil {
		return err
	}
	if err := atomicWrite(filepath.Join(dir, "summary.md"), []byte(renderTwinsMarkdown(report))); err != nil {
		return err
	}
	t := report.Totals
	fmt.Printf("twins: %d functions; %d groups (%d shared, %d mergeable, %d need renames), %d near misses; %d bytes unlocked\n",
		t.Functions, t.Groups, t.Shared, t.Mergeable, t.NeedsRenames, t.NearMisses, t.BytesUnlocked)
	fmt.Printf("  report: %s/twins.yaml, %s/summary.md\n", *out, *out)
	return nil
}

// findTwins symbolizes every C function and groups equal streams.
func (p project) findTwins(config *projectConfig, options twinsOptions) (*twinsReport, error) {
	var functions []*twinFunction
	for _, m := range config.Modules {
		if options.modules != nil && !options.modules[m.ID] {
			continue
		}
		image, err := p.readModuleImage(m)
		if err != nil {
			return nil, fmt.Errorf("%s: %w (extract the disc first)", m.ID, err)
		}
		scope := newTwinScope(config, m)
		for index := range m.Functions {
			f := &m.Functions[index]
			if f.Asm != "" || f.Size < options.minBytes || strings.HasPrefix(m.sourceOf(f), "src/lib/") {
				continue
			}
			start := m.offset(f.Addr)
			if start < 0 || start+f.Size > len(image) {
				continue
			}
			functions = append(functions, twinSymbolize(m, f, image[start:start+f.Size], scope))
		}
	}
	report := &twinsReport{SchemaVersion: twinsSchemaVersion, CandidateOnly: true,
		Options: twinsReportOptions{MinBytes: options.minBytes, MaxNear: options.maxNear}}
	report.Totals.Functions = len(functions)
	byKey := map[string][]*twinFunction{}
	var keys []string
	for _, fn := range functions {
		key := twinKey(fn.tokens)
		if byKey[key] == nil {
			keys = append(keys, key)
		}
		byKey[key] = append(byKey[key], fn)
	}
	for _, key := range keys {
		members := byKey[key]
		if len(members) < 2 {
			continue
		}
		for _, twins := range splitAmbiguous(members) {
			if len(twins) >= 2 {
				report.Groups = append(report.Groups, twinGroup(twins))
			}
		}
	}
	sort.SliceStable(report.Groups, func(i, j int) bool {
		if report.Groups[i].Unlocked != report.Groups[j].Unlocked {
			return report.Groups[i].Unlocked > report.Groups[j].Unlocked
		}
		return report.Groups[i].Members[0].Name < report.Groups[j].Members[0].Name
	})
	for index := range report.Groups {
		group := &report.Groups[index]
		group.ID = fmt.Sprintf("t%04d", index+1)
		switch group.Status {
		case "shared":
			report.Totals.Shared++
		case "mergeable":
			report.Totals.Mergeable++
		default:
			report.Totals.NeedsRenames++
		}
		report.Totals.BytesUnlocked += group.Unlocked
	}
	report.Totals.Groups = len(report.Groups)
	report.NearMisses = twinNearMisses(functions, byKey, options)
	report.Totals.NearMisses = len(report.NearMisses)
	return report, nil
}

// moduleTokens are the leading name tokens that mark a module-specific copy.
var moduleTokens = map[string]bool{
	"battle": true, "world": true, "wldcore": true, "main": true, "opening": true, "open": true, "effect": true,
	"attack": true, "bunit": true, "card": true, "debugchr": true, "equip": true, "etc": true, "helpmenu": true,
	"jobstts": true, "option": true, "require": true, "small": true,
}

// stripModulePrefix drops a leading module token: world_menu_close becomes
// menu_close and g_card_gfx_context becomes g_gfx_context.
func stripModulePrefix(name string) string {
	prefix := ""
	if strings.HasPrefix(name, "g_") {
		prefix, name = "g_", name[2:]
	}
	if index := strings.Index(name, "_"); index > 0 && moduleTokens[name[:index]] {
		name = name[index+1:]
	}
	return prefix + name
}

// splitAmbiguous separates a stream group in which one module appears more
// than once: such members are different functions of one shape (call
// wrappers, primary and secondary readers), so twins are paired across
// modules by function name, then by symbol names, after module prefixes are
// dropped. Members left alone are not twins of anything.
func splitAmbiguous(members []*twinFunction) [][]*twinFunction {
	perModule := map[string]int{}
	for _, fn := range members {
		perModule[fn.m.ID]++
	}
	ambiguous := false
	for _, count := range perModule {
		ambiguous = ambiguous || count > 1
	}
	if !ambiguous {
		return [][]*twinFunction{members}
	}
	cluster := func(pool []*twinFunction, keyOf func(*twinFunction) string) ([][]*twinFunction, []*twinFunction) {
		byKey := map[string][]*twinFunction{}
		var order []string
		for _, fn := range pool {
			key := keyOf(fn)
			if byKey[key] == nil {
				order = append(order, key)
			}
			byKey[key] = append(byKey[key], fn)
		}
		var groups [][]*twinFunction
		var rest []*twinFunction
		for _, key := range order {
			var twins []*twinFunction
			seen := map[string]bool{}
			for _, fn := range byKey[key] {
				if seen[fn.m.ID] {
					rest = append(rest, fn)
					continue
				}
				seen[fn.m.ID] = true
				twins = append(twins, fn)
			}
			if len(twins) >= 2 {
				groups = append(groups, twins)
			} else {
				rest = append(rest, twins...)
			}
		}
		return groups, rest
	}
	groups, rest := cluster(members, func(fn *twinFunction) string { return stripModulePrefix(fn.f.Name) })
	more, _ := cluster(rest, func(fn *twinFunction) string {
		stripped := make([]string, len(fn.symbols))
		for index, symbol := range fn.symbols {
			stripped[index] = stripModulePrefix(symbol)
		}
		return strings.Join(stripped, "\n")
	})
	return append(groups, more...)
}

func twinGroup(members []*twinFunction) twinsGroup {
	sort.SliceStable(members, func(i, j int) bool {
		if members[i].m.ID != members[j].m.ID {
			return members[i].m.ID < members[j].m.ID
		}
		return members[i].f.Addr < members[j].f.Addr
	})
	group := twinsGroup{Bytes: members[0].f.Size}
	modules := map[string]bool{}
	sources := map[string]bool{}
	for _, fn := range members {
		group.Members = append(group.Members, twinMember(fn))
		modules[fn.m.ID] = true
		sources[fn.source] = true
		if fn.f.Size > group.Bytes {
			group.Bytes = fn.f.Size
		}
	}
	group.Modules, group.Sources = sortedKeys(modules), sortedKeys(sources)
	group.SameModule = len(modules) == 1
	group.Unlocked = group.Bytes * (len(sources) - 1)
	keys := memberKeys(members)
	slots := len(members[0].symbols)
	for k := 0; k < slots; k++ {
		same := true
		for _, fn := range members[1:] {
			if fn.symbols[k] != members[0].symbols[k] {
				same = false
				break
			}
		}
		if same {
			group.Unified++
			continue
		}
		row := map[string]string{}
		for index, fn := range members {
			row[keys[index]] = fn.symbols[k]
		}
		group.Renames = append(group.Renames, row)
	}
	switch {
	case len(sources) == 1:
		group.Status = "shared"
	case len(group.Renames) == 0:
		group.Status = "mergeable"
	default:
		group.Status = "needs-renames"
	}
	return group
}

// twinNearMisses pairs functions with equal masked streams but different
// operands, across modules, when few instructions differ.
func twinNearMisses(functions []*twinFunction, byKey map[string][]*twinFunction, options twinsOptions) []twinsNearMiss {
	byMask := map[string][]*twinFunction{}
	var masks []string
	for _, fn := range functions {
		key := twinKey(fn.mnemonics)
		if byMask[key] == nil {
			masks = append(masks, key)
		}
		byMask[key] = append(byMask[key], fn)
	}
	var misses []twinsNearMiss
	for _, mask := range masks {
		bucket := byMask[mask]
		if len(bucket) < 2 {
			continue
		}
		// One representative per exact stream keeps a bucket of shared
		// copies from pairing every copy with every other.
		seen := map[string]bool{}
		var reps []*twinFunction
		for _, fn := range bucket {
			key := twinKey(fn.tokens)
			if !seen[key] {
				seen[key] = true
				reps = append(reps, fn)
			}
		}
		if len(reps) < 2 || len(reps) > 40 {
			continue
		}
		for i := 0; i < len(reps); i++ {
			for j := i + 1; j < len(reps); j++ {
				a, b := reps[i], reps[j]
				if a.m.ID == b.m.ID || len(a.tokens) != len(b.tokens) {
					continue
				}
				// The effect overlays are parameter variants of one source
				// already; their pairwise differences are known.
				if strings.HasPrefix(a.m.ID, "effect-") && strings.HasPrefix(b.m.ID, "effect-") {
					continue
				}
				miss := twinsNearMiss{Bytes: max(a.f.Size, b.f.Size), Members: []twinsMember{twinMember(a), twinMember(b)}}
				for k := range a.tokens {
					if a.tokens[k] == b.tokens[k] {
						continue
					}
					miss.Differences++
					if len(miss.Lines) < 8 {
						miss.Lines = append(miss.Lines, map[string]string{
							"index": fmt.Sprint(k),
							a.m.ID:  twinExpand(a, a.tokens[k]),
							b.m.ID:  twinExpand(b, b.tokens[k]),
						})
					}
				}
				if miss.Differences <= options.maxNear {
					misses = append(misses, miss)
				}
			}
		}
	}
	sort.SliceStable(misses, func(i, j int) bool {
		if misses[i].Bytes != misses[j].Bytes {
			return misses[i].Bytes > misses[j].Bytes
		}
		return misses[i].Members[0].Name < misses[j].Members[0].Name
	})
	for index := range misses {
		misses[index].ID = fmt.Sprintf("n%04d", index+1)
	}
	return misses
}

// twinExpand replaces slot references in a token with the symbol names.
func twinExpand(fn *twinFunction, token string) string {
	for k := len(fn.symbols) - 1; k >= 0; k-- {
		token = strings.ReplaceAll(token, fmt.Sprintf("#%d", k), fn.symbols[k])
	}
	return strings.ReplaceAll(token, "\t", " ")
}

func renderTwinsMarkdown(report *twinsReport) string {
	var out strings.Builder
	t := report.Totals
	fmt.Fprintf(&out, "# Twin functions\n\nCandidates only: every group still needs a trial. %d functions scanned; %d groups (%d already shared, %d mergeable as named, %d needing renames); %d near misses of at most %d differing instructions; %d bytes of separate sources could merge.\n\n",
		t.Functions, t.Groups, t.Shared, t.Mergeable, t.NeedsRenames, t.NearMisses, report.Options.MaxNear, t.BytesUnlocked)
	out.WriteString("## Groups by bytes unlocked\n\n| id | status | bytes | unlocked | members | renames |\n|---|---|---:|---:|---|---:|\n")
	for _, group := range report.Groups {
		if group.Status == "shared" {
			continue
		}
		var members []string
		for _, member := range group.Members {
			members = append(members, member.Module+":"+member.Name)
		}
		fmt.Fprintf(&out, "| %s | %s | %d | %d | %s | %d |\n", group.ID, group.Status, group.Bytes, group.Unlocked, strings.Join(members, ", "), len(group.Renames))
	}
	out.WriteString("\n## Near misses\n\n| id | bytes | differences | members | first difference |\n|---|---:|---:|---|---|\n")
	for _, miss := range report.NearMisses {
		first := ""
		if len(miss.Lines) > 0 {
			var parts []string
			for _, key := range sortedKeys(mapKeysTrue(miss.Lines[0])) {
				if key != "index" {
					parts = append(parts, key+": "+miss.Lines[0][key])
				}
			}
			first = strings.Join(parts, " / ")
		}
		fmt.Fprintf(&out, "| %s | %d | %d | %s:%s, %s:%s | %s |\n", miss.ID, miss.Bytes, miss.Differences,
			miss.Members[0].Module, miss.Members[0].Name, miss.Members[1].Module, miss.Members[1].Name, strings.ReplaceAll(first, "|", "\\|"))
	}
	return out.String()
}

// twinTrials builds the trials that test a twins-report group both ways:
// each distinct source compiled at every member that uses another source,
// with that member's symbol names substituted for the source's.
func (p project) twinTrials(config *projectConfig, reportPath string, ids []string) ([]trialSpec, error) {
	path, err := projectPath(p.root, reportPath, "twins report")
	if err != nil {
		return nil, err
	}
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	var report twinsReport
	if err := yaml.Unmarshal(data, &report); err != nil {
		return nil, err
	}
	wanted := map[string]bool{}
	for _, id := range ids {
		wanted[strings.TrimSpace(id)] = true
	}
	var specs []trialSpec
	for _, group := range report.Groups {
		if !wanted[group.ID] {
			continue
		}
		delete(wanted, group.ID)
		specs = append(specs, twinGroupTrials(config, group)...)
	}
	if len(wanted) > 0 {
		return nil, fmt.Errorf("no twin groups %s in %s", strings.Join(sortedKeys(wanted), ", "), reportPath)
	}
	if len(specs) == 0 {
		return nil, errors.New("the selected groups are already shared")
	}
	return specs, nil
}

func twinGroupTrials(config *projectConfig, group twinsGroup) []trialSpec {
	var specs []trialSpec
	// The rename rows are keyed like memberKeys: module, or module:name.
	keyOf := func(member twinsMember) string {
		count := 0
		for _, other := range group.Members {
			if other.Module == member.Module {
				count++
			}
		}
		if count > 1 {
			return member.Module + ":" + member.Name
		}
		return member.Module
	}
	for _, from := range group.Members {
		for _, to := range group.Members {
			if to.Source == from.Source {
				continue
			}
			subst := map[string]string{}
			for _, row := range group.Renames {
				old, new := row[keyOf(from)], row[keyOf(to)]
				if old != "" && new != "" && old != new && validSymbol(old) && validSymbol(new) {
					subst[old] = new
				}
			}
			symbol := ""
			if m, ok := config.module(from.Module); ok {
				if f := m.function(from.Name); f != nil {
					symbol = f.sourceSymbol()
				}
			}
			specs = append(specs, trialSpec{Module: to.Module, Entry: to.Name, Source: from.Source, Symbol: symbol, Subst: subst,
				Label: fmt.Sprintf("%s %s %s <- %s:%s", group.ID, to.Module, to.Name, from.Module, from.Name)})
		}
	}
	// One trial per (source, entry) pair.
	seen := map[string]bool{}
	unique := specs[:0]
	for _, spec := range specs {
		key := spec.Module + "\x00" + spec.Entry + "\x00" + spec.Source
		if !seen[key] {
			seen[key] = true
			unique = append(unique, spec)
		}
	}
	return unique
}
