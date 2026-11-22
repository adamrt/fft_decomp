// order.go reports whether the functions several modules share keep one
// relative order. A source file compiled once per program with conditionals
// keeps its functions in one order everywhere; functions copied into each
// program's own source do not have to. The report lists the order changes
// between module pairs, the shared sources whose functions change order, and
// the same-named functions that modules compile from different files, with
// how far their instructions differ. It writes under build/ and never
// touches target/ or src/.
package main

import (
	"bytes"
	"errors"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"

	"gopkg.in/yaml.v3"
)

const orderSchemaVersion = 1

// orderCompareLimit caps the instruction comparison's table: the product of
// the two instruction counts.
const orderCompareLimit = 40_000_000

type orderReport struct {
	SchemaVersion int           `yaml:"schema_version"`
	Totals        orderTotals   `yaml:"totals"`
	Pairs         []orderPair   `yaml:"pairs"`
	Sources       []orderSource `yaml:"sources"`
	Splits        []orderSplit  `yaml:"splits"`
}

type orderTotals struct {
	Modules          int `yaml:"modules"`
	Pairs            int `yaml:"pairs"`
	PairsReordered   int `yaml:"pairs_reordered"`
	Sources          int `yaml:"shared_sources"`
	SourcesReordered int `yaml:"shared_sources_reordered"`
	Splits           int `yaml:"splits"`
	SplitsOrdered    int `yaml:"splits_ordered"`
}

// orderPair compares the functions two modules name alike.
type orderPair struct {
	A      string       `yaml:"a"`
	B      string       `yaml:"b"`
	Shared int          `yaml:"shared"`
	Breaks []orderBreak `yaml:"breaks,omitempty"`
}

// orderBreak is one place where two functions adjacent among the shared names
// in module A are not adjacent in that order in module B.
type orderBreak struct {
	After  string `yaml:"after"`
	Before string `yaml:"before"`
}

// orderSource is one source file compiled into two or more modules.
type orderSource struct {
	Source       string   `yaml:"source"`
	Modules      []string `yaml:"modules"`
	Functions    int      `yaml:"functions"`
	Partial      int      `yaml:"partial"` // functions only some of the modules compile
	Conditionals int      `yaml:"conditionals"`
	Order        string   `yaml:"order"` // same or differs
	Detail       string   `yaml:"detail,omitempty"`
	Runs         []string `yaml:"runs,omitempty"` // modules in which the file is not one address run
}

// orderSplit is a set of files that hold same-named functions for different
// modules: candidates for one file with program conditionals when the order
// agrees and the instructions differ in few places.
type orderSplit struct {
	Files   []orderSplitFile `yaml:"files"`
	Shared  int              `yaml:"shared"`
	Order   string           `yaml:"order"`
	Detail  string           `yaml:"detail,omitempty"`
	Members []orderSplitName `yaml:"members"`
}

type orderSplitFile struct {
	Module    string `yaml:"module"`
	Source    string `yaml:"source"`
	Functions int    `yaml:"functions"`
}

// orderSplitName compares one shared name between the first two modules that
// hold it in different files.
type orderSplitName struct {
	Name       string `yaml:"name"`
	A          string `yaml:"a"`
	B          string `yaml:"b"`
	SizeA      int    `yaml:"size_a"`
	SizeB      int    `yaml:"size_b"`
	Compared   bool   `yaml:"compared"`
	Places     int    `yaml:"places"` // separate runs of differing operations (registers and operands ignored)
	OnlyA      int    `yaml:"only_a"` // instructions with no counterpart in B
	OnlyB      int    `yaml:"only_b"`
	Count      int    `yaml:"instructions"` // of the longer side, traps stripped
	ProfileGap bool   `yaml:"profile_gap,omitempty"`
}

// orderModule is one module's game functions in address order.
type orderModule struct {
	m         *moduleSpec
	functions []*functionSpec
	position  map[string]int
}

func (p project) orderCommand(args []string) error {
	flags := flag.NewFlagSet("order", flag.ContinueOnError)
	modules := flags.String("module", "", "modules to compare (default every module but the effects)")
	out := flags.String("out", "build/analysis/order", "output directory under build/")
	if err := flags.Parse(args); err != nil {
		return err
	}
	if flags.NArg() != 0 {
		return errors.New("order accepts named flags only")
	}
	if clean := filepath.ToSlash(filepath.Clean(*out)); clean != "build" && !strings.HasPrefix(clean, "build/") {
		return fmt.Errorf("order output must lie under build/: %s", *out)
	}
	config, err := p.loadProjectConfig()
	if err != nil {
		return err
	}
	var selected []*moduleSpec
	if *modules != "" {
		if selected, err = resolveModuleSelection(config, *modules); err != nil {
			return err
		}
	} else {
		// The effect files share their callbacks by design, one or two each.
		for _, m := range config.Modules {
			if !strings.HasPrefix(m.ID, "effect-") {
				selected = append(selected, m)
			}
		}
	}
	report := buildOrderReport(config, selected, p.root, func(m *moduleSpec) ([]byte, error) { return p.readModuleImage(m) })
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
	if err := atomicWrite(filepath.Join(dir, "order.yaml"), buffer.Bytes()); err != nil {
		return err
	}
	if err := atomicWrite(filepath.Join(dir, "summary.md"), []byte(renderOrderMarkdown(report))); err != nil {
		return err
	}
	t := report.Totals
	fmt.Printf("order: %d module pairs share functions, %d with order changes; %d shared sources, %d reordered; %d split sets, %d in one order\n",
		t.Pairs, t.PairsReordered, t.Sources, t.SourcesReordered, t.Splits, t.SplitsOrdered)
	fmt.Printf("  report: %s/order.yaml, %s/summary.md\n", *out, *out)
	return nil
}

// buildOrderReport compares the selected modules. readImage may fail (no
// extracted disc): the instruction comparison is then left out.
func buildOrderReport(config *projectConfig, selected []*moduleSpec, root string, readImage func(*moduleSpec) ([]byte, error)) *orderReport {
	var modules []*orderModule
	for _, m := range selected {
		om := &orderModule{m: m, position: map[string]int{}}
		for index := range m.Functions {
			f := &m.Functions[index]
			if m.isLibraryFunction(f) {
				continue
			}
			om.functions = append(om.functions, f)
		}
		sort.SliceStable(om.functions, func(i, j int) bool { return om.functions[i].Addr < om.functions[j].Addr })
		for index, f := range om.functions {
			om.position[f.Name] = index
		}
		modules = append(modules, om)
	}
	report := &orderReport{SchemaVersion: orderSchemaVersion}
	report.Totals.Modules = len(modules)
	report.Pairs = orderPairs(modules)
	report.Sources = orderSources(modules, root)
	report.Splits = orderSplits(config, modules, readImage)
	report.Totals.Pairs = len(report.Pairs)
	for _, pair := range report.Pairs {
		if len(pair.Breaks) > 0 {
			report.Totals.PairsReordered++
		}
	}
	report.Totals.Sources = len(report.Sources)
	for _, source := range report.Sources {
		if source.Order != "same" {
			report.Totals.SourcesReordered++
		}
	}
	report.Totals.Splits = len(report.Splits)
	for _, split := range report.Splits {
		if split.Order == "same" {
			report.Totals.SplitsOrdered++
		}
	}
	return report
}

// orderBreaks walks the names both lists hold, in a's order, and reports
// each adjacent pair that b does not keep adjacent in that order.
func orderBreaks(a, b []string) (int, []orderBreak) {
	inB := map[string]int{}
	for index, name := range b {
		inB[name] = index
	}
	var common []string
	for _, name := range a {
		if _, ok := inB[name]; ok {
			common = append(common, name)
		}
	}
	byB := append([]string(nil), common...)
	sort.SliceStable(byB, func(i, j int) bool { return inB[byB[i]] < inB[byB[j]] })
	rank := map[string]int{}
	for index, name := range byB {
		rank[name] = index
	}
	var breaks []orderBreak
	for index := 1; index < len(common); index++ {
		if rank[common[index]] != rank[common[index-1]]+1 {
			breaks = append(breaks, orderBreak{After: common[index-1], Before: common[index]})
		}
	}
	return len(common), breaks
}

func (om *orderModule) names() []string {
	names := make([]string, len(om.functions))
	for index, f := range om.functions {
		names[index] = f.Name
	}
	return names
}

func orderPairs(modules []*orderModule) []orderPair {
	var pairs []orderPair
	for i, a := range modules {
		for _, b := range modules[i+1:] {
			shared, breaks := orderBreaks(a.names(), b.names())
			if shared < 2 {
				continue
			}
			pairs = append(pairs, orderPair{A: a.m.ID, B: b.m.ID, Shared: shared, Breaks: breaks})
		}
	}
	return pairs
}

// orderSources lists every source two or more modules compile, with the
// order of its functions as each module holds them.
func orderSources(modules []*orderModule, root string) []orderSource {
	type use struct {
		om    *orderModule
		names []string // source symbols in address order, first occurrence
		runs  int
	}
	uses := map[string][]*use{}
	var sources []string
	for _, om := range modules {
		current := map[string]*use{}
		previous := ""
		seen := map[string]map[string]bool{}
		for _, f := range om.functions {
			source := om.m.sourceOf(f)
			if f.Asm != "" || source == "" {
				previous = source
				continue
			}
			u := current[source]
			if u == nil {
				u = &use{om: om}
				current[source] = u
				seen[source] = map[string]bool{}
				if len(uses[source]) == 0 {
					sources = append(sources, source)
				}
				uses[source] = append(uses[source], u)
			}
			if source != previous {
				u.runs++
			}
			previous = source
			if symbol := f.sourceSymbol(); !seen[source][symbol] {
				seen[source][symbol] = true
				u.names = append(u.names, symbol)
			}
		}
	}
	sort.Strings(sources)
	var result []orderSource
	for _, source := range sources {
		list := uses[source]
		if len(list) < 2 {
			continue
		}
		entry := orderSource{Source: source, Order: "same"}
		var lists [][]string
		count := map[string]int{}
		for _, u := range list {
			entry.Modules = append(entry.Modules, u.om.m.ID)
			lists = append(lists, u.names)
			for _, name := range u.names {
				count[name]++
			}
			if u.runs > 1 {
				entry.Runs = append(entry.Runs, fmt.Sprintf("%s:%d", u.om.m.ID, u.runs))
			}
		}
		entry.Functions = len(count)
		for _, n := range count {
			if n != len(list) {
				entry.Partial++
			}
		}
		if _, err := mergeMemberOrders(lists); err != nil {
			entry.Order = "differs"
			entry.Detail = err.Error()
		}
		if data, err := os.ReadFile(filepath.Join(root, filepath.FromSlash(source))); err == nil {
			entry.Conditionals = programConditionalCount(string(data))
		}
		result = append(result, entry)
	}
	return result
}

// programConditionalCount counts the directives that open a conditional on a
// program macro or a VARIANT_ name.
func programConditionalCount(source string) int {
	count := 0
	for _, line := range strings.Split(source, "\n") {
		trimmed := strings.TrimSpace(line)
		if !strings.HasPrefix(trimmed, "#") {
			continue
		}
		if word, _ := directiveWord(trimmed); word != "if" && word != "ifdef" && word != "ifndef" {
			continue
		}
		for _, name := range conditionalIdentifiers(trimmed) {
			if strings.HasPrefix(name, moduleMacroPrefix) || strings.HasPrefix(name, definePrefix) {
				count++
				break
			}
		}
	}
	return count
}

// orderSplits groups the files that hold same-named functions for different
// modules and compares each shared name.
func orderSplits(config *projectConfig, modules []*orderModule, readImage func(*moduleSpec) ([]byte, error)) []orderSplit {
	type holder struct {
		om *orderModule
		f  *functionSpec
	}
	byName := map[string][]holder{}
	var names []string
	for _, om := range modules {
		for _, f := range om.functions {
			if f.Asm != "" {
				continue
			}
			if len(byName[f.Name]) == 0 {
				names = append(names, f.Name)
			}
			byName[f.Name] = append(byName[f.Name], holder{om, f})
		}
	}
	// Union the files that a shared name ties together.
	parent := map[string]string{}
	var find func(string) string
	find = func(key string) string {
		if parent[key] == "" || parent[key] == key {
			parent[key] = key
			return key
		}
		root := find(parent[key])
		parent[key] = root
		return root
	}
	splitNames := map[string]bool{}
	for _, name := range names {
		holders := byName[name]
		first := holders[0].om.m.sourceOf(holders[0].f)
		differs := false
		for _, h := range holders[1:] {
			if h.om.m.sourceOf(h.f) != first {
				differs = true
			}
		}
		if !differs {
			continue
		}
		splitNames[name] = true
		for _, h := range holders[1:] {
			parent[find(h.om.m.sourceOf(h.f))] = find(first)
		}
	}
	groups := map[string][]string{} // root file -> split names, in first-seen order
	var roots []string
	for _, name := range names {
		if !splitNames[name] {
			continue
		}
		root := find(byName[name][0].om.m.sourceOf(byName[name][0].f))
		if len(groups[root]) == 0 {
			roots = append(roots, root)
		}
		groups[root] = append(groups[root], name)
	}
	images := map[*moduleSpec][]byte{}
	scopes := map[*moduleSpec]*twinScope{}
	stream := func(om *orderModule, f *functionSpec) []string {
		image, loaded := images[om.m]
		if !loaded {
			image, _ = readImage(om.m)
			images[om.m] = image
		}
		start := om.m.offset(f.Addr)
		if image == nil || start < 0 || start+f.Size > len(image) {
			return nil
		}
		if scopes[om.m] == nil {
			scopes[om.m] = newTwinScope(config, om.m)
		}
		// Operations only: one changed line can move every later value to
		// another register, which is no difference in the code's structure.
		masked := twinSymbolize(om.m, f, image[start:start+f.Size], scopes[om.m]).mnemonics
		operations := make([]string, len(masked))
		for index, token := range masked {
			operations[index], _, _ = strings.Cut(token, " ")
		}
		return operations
	}
	var splits []orderSplit
	for _, root := range roots {
		split := orderSplit{Order: "same", Shared: len(groups[root])}
		files := map[string]bool{}
		for _, name := range groups[root] {
			for _, h := range byName[name] {
				files[h.om.m.ID+"\x00"+h.om.m.sourceOf(h.f)] = true
			}
		}
		// Each module's functions from the set's files, in address order.
		var lists [][]string
		for _, om := range modules {
			var list []string
			perFile := map[string]int{}
			for _, f := range om.functions {
				source := om.m.sourceOf(f)
				if !files[om.m.ID+"\x00"+source] {
					continue
				}
				list = append(list, f.Name)
				perFile[source]++
			}
			if len(list) == 0 {
				continue
			}
			lists = append(lists, list)
			var sources []string
			for source := range perFile {
				sources = append(sources, source)
			}
			sort.Strings(sources)
			for _, source := range sources {
				split.Files = append(split.Files, orderSplitFile{Module: om.m.ID, Source: source, Functions: perFile[source]})
			}
		}
		if _, err := mergeMemberOrders(lists); err != nil {
			split.Order = "differs"
			split.Detail = err.Error()
		}
		for _, name := range groups[root] {
			holders := byName[name]
			a := holders[0]
			b := holders[1]
			for _, h := range holders[1:] {
				if h.om.m.sourceOf(h.f) != a.om.m.sourceOf(a.f) {
					b = h
					break
				}
			}
			member := orderSplitName{Name: name, A: a.om.m.ID, B: b.om.m.ID, SizeA: a.f.Size, SizeB: b.f.Size,
				ProfileGap: profileLevel(a.om.m.profileOf(a.f)) != profileLevel(b.om.m.profileOf(b.f))}
			sa, sb := stream(a.om, a.f), stream(b.om, b.f)
			if sa != nil && sb != nil && len(sa)*len(sb) <= orderCompareLimit {
				member.Compared = true
				member.Places, member.OnlyA, member.OnlyB = instructionDifferences(sa, sb)
				member.Count = max(len(sa), len(sb))
			}
			split.Members = append(split.Members, member)
		}
		splits = append(splits, split)
	}
	sort.SliceStable(splits, func(i, j int) bool {
		if (splits[i].Order == "same") != (splits[j].Order == "same") {
			return splits[i].Order == "same"
		}
		return splits[i].Files[0].Source < splits[j].Files[0].Source
	})
	return splits
}

// profileLevel extracts the optimization level of a profile name such as
// gcc-2.6.3_O1_aspsx-2.34: two builds of one source at different levels
// differ throughout, so their instruction comparison says little.
func profileLevel(profile string) string {
	for _, part := range strings.Split(profile, "_") {
		if len(part) == 2 && part[0] == 'O' {
			return part
		}
	}
	return ""
}

// instructionDifferences counts the separate runs of differing instructions
// between two operand-masked streams and the instructions on each side that
// have no counterpart.
func instructionDifferences(a, b []string) (places, onlyA, onlyB int) {
	for _, op := range levenshteinOpcodes(a, b) {
		if op.tag == 'e' {
			continue
		}
		places++
		onlyA += op.i2 - op.i1
		onlyB += op.j2 - op.j1
	}
	return places, onlyA, onlyB
}

func renderOrderMarkdown(report *orderReport) string {
	var out strings.Builder
	t := report.Totals
	fmt.Fprintf(&out, "# Function order across modules\n\n")
	fmt.Fprintf(&out, "Functions compiled once per program from one source keep one relative order. An order change means the pieces were ordered separately: separate objects, or functions copied into each program's source. %d modules compared.\n\n", t.Modules)
	fmt.Fprintf(&out, "## Module pairs\n\n%d pairs share two or more function names; %d change their order.\n\n", t.Pairs, t.PairsReordered)
	out.WriteString("| a | b | shared | order changes |\n|---|---|---:|---:|\n")
	pairs := append([]orderPair(nil), report.Pairs...)
	sort.SliceStable(pairs, func(i, j int) bool { return pairs[i].Shared > pairs[j].Shared })
	for _, pair := range pairs {
		fmt.Fprintf(&out, "| %s | %s | %d | %d |\n", pair.A, pair.B, pair.Shared, len(pair.Breaks))
	}
	fmt.Fprintf(&out, "\n## Shared sources\n\n%d sources compile into two or more modules; %d change the order of their functions between modules.\n\n", t.Sources, t.SourcesReordered)
	out.WriteString("| source | modules | functions | in some modules only | conditionals | order |\n|---|---:|---:|---:|---:|---|\n")
	for _, source := range report.Sources {
		if source.Order == "same" {
			continue
		}
		fmt.Fprintf(&out, "| %s | %d | %d | %d | %d | %s |\n", source.Source, len(source.Modules), source.Functions, source.Partial, source.Conditionals, source.Detail)
	}
	fmt.Fprintf(&out, "\n## Same name, different files\n\n%d sets of files hold same-named functions for different modules; %d keep one order and could become one file under program conditionals. Places are separate runs of differing operations, with registers and operands ignored and division traps stripped; a level gap marks builds at different optimization levels, whose instructions differ throughout.\n\n", t.Splits, t.SplitsOrdered)
	for _, split := range report.Splits {
		var files []string
		for _, file := range split.Files {
			files = append(files, fmt.Sprintf("%s (%s, %d)", file.Source, file.Module, file.Functions))
		}
		fmt.Fprintf(&out, "### %s\n\n", strings.Join(files, " + "))
		if split.Order == "same" {
			fmt.Fprintf(&out, "One order; %d shared names.\n\n", split.Shared)
		} else {
			fmt.Fprintf(&out, "Order differs: %s. %d shared names.\n\n", split.Detail, split.Shared)
		}
		out.WriteString("| function | a | b | bytes | places | instructions only in a / b | of |\n|---|---|---|---|---:|---|---:|\n")
		for _, member := range split.Members {
			places, only, count := "-", "-", "-"
			if member.Compared {
				places = fmt.Sprint(member.Places)
				only = fmt.Sprintf("%d / %d", member.OnlyA, member.OnlyB)
				count = fmt.Sprint(member.Count)
			}
			if member.ProfileGap {
				places += " (level gap)"
			}
			fmt.Fprintf(&out, "| %s | %s | %s | %d / %d | %s | %s | %s |\n", member.Name, member.A, member.B, member.SizeA, member.SizeB, places, only, count)
		}
		out.WriteString("\n")
	}
	return out.String()
}
