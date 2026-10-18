// units.go proposes translation-unit boundaries using contiguous function runs
// and data ranges as a model. It scores cross-module sequence differences,
// configured compiler profiles and references in the current sources. These
// signals do not uniquely identify original files. The command writes reports
// under build/ and never touches target/ or src/.
package main

import (
	"errors"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
)

type unitsOptions struct {
	minConfidence string
	minScore      int    // when set, a cut needs at least this score (or certainty) to split
	evidence      string // when set, only required cuts, or required plus alignment/data-shift signals, split
	maxOwners     int
	outDir        string
}

// strongCutKinds adds unmatched or reordered alignment runs and data-distance
// shifts to the cuts required by the analysis. Each identifies a candidate cut,
// but source edits and changed data sizes can produce the same signals.
var strongCutKinds = map[string]bool{"alignment_divergence": true, "alignment_reorder": true, "data_shift": true}

// evidenceEligible reports whether a cut qualifies under an evidence mode.
func (cut *unitCut) evidenceEligible(mode string) bool {
	if cut.certain {
		return true
	}
	if mode != "strong" {
		return false
	}
	for _, evidence := range cut.evidence {
		if strongCutKinds[evidence.Kind] {
			return true
		}
	}
	return false
}

var unitConfidenceRank = map[string]int{"none": 0, "low": 1, "medium": 2, "high": 3}

const (
	// A data row referenced only by functions this close together behaves
	// like a file-static: a boundary between its users is unlikely.
	unitLocalOwnerSpan = 10
	// Stubs this small carry no compiler-profile signal.
	unitNeutralSize = 16
	// Rows at most this far apart form the small (gp-relative) data class.
	unitSmallSpacing = 8
	// Spacing-class windows: a section split needs this many rows on each side.
	unitSpacingWindow = 9
)

// unitModuleTokens are leading name tokens that identify the module rather
// than the subsystem.
var unitModuleTokens = map[string]bool{
	"battle": true, "world": true, "wldcore": true, "open": true, "main": true,
	"attack": true, "bunit": true, "card": true, "debugchr": true, "equip": true, "etc": true,
	"helpmenu": true, "jobstts": true, "option": true, "require": true, "small": true,
}

type unitFunction struct {
	index       int
	f           *functionSpec
	group       int
	source      string
	idents      map[string]struct{}
	profile     string
	base        string // optimization level, assembler and small-data class
	divcheck    bool
	hasDivision bool
	neutral     bool
	asm         bool
	library     bool
	prefix      string
	aligned     map[string]string // partner module -> partner function
	positional  map[string]bool   // aligned by position between anchors only
	unit        int
	jumpAlign   int // addr mod 8 of the function's first jump table, -1 without one
}

type unitDataRow struct {
	index   int
	addr    uint32
	names   []string
	owners  []int // referencing functions, ascending
	group   int
	kind    string // pre, rdata, asm, data or bss
	section int
	wide    bool
}

func (row *unitDataRow) lo() int { return row.owners[0] }
func (row *unitDataRow) hi() int { return row.owners[len(row.owners)-1] }

// local reports whether the row's references stay within one neighborhood,
// the way a file-static's do.
func (row *unitDataRow) local() bool {
	return !row.wide && len(row.owners) > 0 && row.hi()-row.lo() <= unitLocalOwnerSpan
}

type unitSection struct {
	id         string
	start, end uint32
	kind       string
	rows       []int
	monotone   bool
	resets     int
}

type unitEvidence struct {
	Kind   string
	Weight int
	Detail string
}

type unitCut struct {
	after      int // boundary between functions after and after+1
	score      int
	confidence string
	certain    bool // forced by the analysis, not proof of a historical source-file boundary
	evidence   []unitEvidence
}

type unitCandidate struct {
	id            string
	start, end    uint32
	confidence    string
	profile       string
	prefixes      map[string]int
	suggestedName string
	counterparts  []unitsReportCounterpart
	functions     []int
	rows          []int
}

type unitGroup struct {
	id                     int
	first, last            int
	textStart, textEnd     uint32
	rodataStart, rodataEnd uint32
	dataStart, dataEnd     uint32
	rows                   []int
	sections               []unitSection
	cuts                   []unitCut
	units                  []unitCandidate
}

func (g *unitGroup) sectionID(index int) string {
	if index < 0 || index >= len(g.sections) {
		return ""
	}
	return g.sections[index].id
}

// start is the lowest address the group owns: its read-only tables precede
// its code.
func (g *unitGroup) start() uint32 {
	if g.rodataEnd > g.rodataStart && g.rodataStart < g.textStart {
		return g.rodataStart
	}
	return g.textStart
}

type unitModuleAnalysis struct {
	m           *moduleSpec
	functions   []unitFunction
	byName      map[string]int
	rows        []unitDataRow
	groups      []unitGroup
	callees     map[int][]int          // function index -> module functions it names, ascending
	evidence    map[int][]unitEvidence // cut after a function index -> evidence from other modules
	alignments  []unitsReportAlign
	diagnostics unitsReportDiagnostic
}

func (p project) unitsCommand(args []string) error {
	flags := flag.NewFlagSet("units", flag.ContinueOnError)
	module := flags.String("module", "", "modules to report (ids, aliases, event or effect)")
	minConfidence := flags.String("min-confidence", "low", "lowest boundary confidence eligible to split units: low, medium or high")
	maxOwners := flags.Int("max-owners", 3, "data referenced by more functions than this is widely shared")
	minScore := flags.Int("min-score", 0, "when set, a cut needs at least this score (or certainty) to split a unit")
	evidence := flags.String("evidence", "", "when set, split only at required cuts, or at required plus alignment/data-shift signals (strong)")
	out := flags.String("out", "build/analysis/units", "report directory under build/")
	if err := flags.Parse(args); err != nil {
		return err
	}
	usage := errors.New("usage: tools units [--module=MODULES] [--min-confidence=low|medium|high] [--min-score=N] [--evidence=required|strong] [--max-owners=N] [--out=build/DIR]")
	if flags.NArg() != 0 || *maxOwners < 1 || unitConfidenceRank[*minConfidence] == 0 || (*evidence != "" && *evidence != "required" && *evidence != "strong") {
		return usage
	}
	outDir := filepath.ToSlash(filepath.Clean(*out))
	if outDir != "build" && !strings.HasPrefix(outDir, "build/") {
		return errors.New("units: --out must name a directory under build/")
	}
	config, err := p.loadProjectConfig()
	if err != nil {
		return err
	}
	selected := config.Modules
	if *module != "" {
		if selected, err = resolveModuleSelection(config, *module); err != nil {
			return err
		}
	}
	options := unitsOptions{minConfidence: *minConfidence, minScore: *minScore, evidence: *evidence, maxOwners: *maxOwners, outDir: outDir}
	analyses, err := p.reconstructUnits(config, options)
	if err != nil {
		return err
	}
	dir := filepath.Join(p.root, filepath.FromSlash(outDir))
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return err
	}
	var reports []unitsReport
	for _, m := range selected {
		report := unitsModuleReport(analyses[m.ID], options)
		data, err := renderUnitsYAML(report)
		if err != nil {
			return err
		}
		path := filepath.Join(dir, m.ID+".yaml")
		if err := atomicWrite(path, data); err != nil {
			return err
		}
		reports = append(reports, report)
		fmt.Fprintf(p.stdout(), "units: %s: %d groups, %d functions, %d data rows, %d units (high %d, medium %d, low %d) -> %s\n",
			m.ID, report.Totals.Groups, report.Totals.Functions, report.Totals.DataRows, report.Totals.Units,
			report.Totals.ByConfidence["high"], report.Totals.ByConfidence["medium"], report.Totals.ByConfidence["low"],
			outDir+"/"+m.ID+".yaml")
	}
	return atomicWrite(filepath.Join(dir, "summary.md"), renderUnitsMarkdown(reports))
}

// reconstructUnits analyzes every module (alignment needs the partners), then
// scores boundaries and forms units.
func (p project) reconstructUnits(config *projectConfig, options unitsOptions) (map[string]*unitModuleAnalysis, error) {
	resolver := newSymbolResolver(config)
	sourceCache := unitSourceCache{}
	analyses := map[string]*unitModuleAnalysis{}
	for _, m := range config.Modules {
		analyses[m.ID] = p.analyzeModuleLayout(m, resolver, sourceCache, options)
	}
	alignAllModules(config, analyses)
	for _, m := range config.Modules {
		analyses[m.ID].scoreAndFormUnits(options)
	}
	for _, m := range config.Modules {
		analyses[m.ID].findCounterparts(analyses)
	}
	return analyses, nil
}

func (p project) analyzeModuleLayout(m *moduleSpec, resolver symbolResolver, sourceCache unitSourceCache, options unitsOptions) *unitModuleAnalysis {
	an := &unitModuleAnalysis{m: m, byName: map[string]int{}, evidence: map[int][]unitEvidence{}}
	an.functions = p.loadUnitFunctions(m, sourceCache, &an.diagnostics)
	if image, err := p.readModuleImage(m); err == nil {
		for i := range an.functions {
			an.functions[i].jumpAlign = jumpTableAlignment(image, m, an.functions[i].f)
		}
	}
	for i := range an.functions {
		an.byName[an.functions[i].f.Name] = i
	}
	an.callees = map[int][]int{}
	for i := range an.functions {
		for name := range an.functions[i].idents {
			if j, ok := an.byName[name]; ok && j != i {
				an.callees[i] = append(an.callees[i], j)
			}
		}
		sort.Ints(an.callees[i])
	}
	an.groups = unitGroups(m, an.functions, an.evidence)
	for g := range an.groups {
		for i := an.groups[g].first; i <= an.groups[g].last; i++ {
			an.functions[i].group = g
		}
	}
	an.rows = unitDataRows(m, an.groups)
	for i := range an.rows {
		row := &an.rows[i]
		an.groups[row.group].rows = append(an.groups[row.group].rows, i)
	}
	ownersOf(an, resolver, options.maxOwners)
	for g := range an.groups {
		inferSections(an, &an.groups[g])
	}
	an.collectDiagnostics()
	return an
}

func (p project) loadUnitFunctions(m *moduleSpec, sourceCache unitSourceCache, diagnostics *unitsReportDiagnostic) []unitFunction {
	order := make([]*functionSpec, len(m.Functions))
	for i := range m.Functions {
		order[i] = &m.Functions[i]
	}
	sort.SliceStable(order, func(i, j int) bool { return order[i].Addr < order[j].Addr })
	functions := make([]unitFunction, len(order))
	for i, f := range order {
		source := m.sourceOf(f)
		fn := unitFunction{index: i, f: f, source: source, asm: f.Asm != "", aligned: map[string]string{}, positional: map[string]bool{}, unit: -1, jumpAlign: -1}
		fn.library = m.isLibraryFunction(f)
		fn.prefix = unitPrefix(f.Name)
		fn.profile = m.profileOf(f)
		if profile, ok := compilerProfileNamed(fn.profile); ok {
			fn.base = profile.optimization + "/aspsx-" + profile.aspsxVersion
			if profile.smallDataLimit != 0 {
				fn.base += "/G" + strconv.Itoa(profile.smallDataLimit)
			}
			fn.divcheck = profile.expandDiv
		} else {
			fn.base = fn.profile
		}
		if fn.asm {
			var err error
			fn.idents, err = sourceIdentifiers(filepath.Join(p.root, filepath.FromSlash(source)))
			if err != nil {
				diagnostics.UnreadableSources = append(diagnostics.UnreadableSources, source+": "+err.Error())
			}
		} else {
			view := p.unitSourceFunctions(m, source, sourceCache)
			if view.err != nil {
				diagnostics.UnreadableSources = append(diagnostics.UnreadableSources, source+": "+view.err.Error())
			} else if facts, ok := view.functions[f.sourceSymbol()]; ok {
				fn.idents, fn.hasDivision = facts.idents, facts.hasDivision
			} else {
				diagnostics.UnreadableSources = append(diagnostics.UnreadableSources, source+": no active definition of "+f.sourceSymbol())
			}
		}
		fn.neutral = f.Size <= unitNeutralSize
		functions[i] = fn
	}
	return functions
}

// unitPrefix is the subsystem token of a name with any module token removed.
func unitPrefix(name string) string {
	parts := strings.Split(strings.TrimLeft(name, "_"), "_")
	if len(parts) > 1 && unitModuleTokens[parts[0]] {
		parts = parts[1:]
	}
	return parts[0]
}

type unitCover struct {
	start, end uint32
	kind       string
}

// coveringIntervals are the code ranges that are not C entries but belong
// inside a group: hand-written routines, interior labels, padding, blocked
// code and library ranges.
func coveringIntervals(m *moduleSpec) []unitCover {
	var covers []unitCover
	for _, region := range m.Regions {
		switch region.Kind {
		case "handwritten", "interior", "padding", "blocked", "library":
			covers = append(covers, unitCover{region.Addr, region.End, region.Kind})
		}
	}
	for _, library := range m.Libraries {
		covers = append(covers, unitCover{library.Addr, library.End, "library"})
	}
	sort.Slice(covers, func(i, j int) bool { return covers[i].start < covers[j].start })
	return covers
}

// gapCoverage reports whether [start, end) is fully covered and which kinds
// cover it.
func gapCoverage(covers []unitCover, start, end uint32) (bool, map[string]bool) {
	kinds := map[string]bool{}
	at := start
	for _, cover := range covers {
		if cover.end <= at || cover.start > at {
			continue
		}
		kinds[cover.kind] = true
		if cover.end > at {
			at = cover.end
		}
		if at >= end {
			return true, kinds
		}
	}
	return at >= end, kinds
}

// unitGroups splits address-ordered functions at gaps not covered by configured
// regions. Covered gaps stay in the group; declared hand-written or library
// regions add required cuts. An uncovered gap need not be data.
func unitGroups(m *moduleSpec, functions []unitFunction, evidence map[int][]unitEvidence) []unitGroup {
	covers := coveringIntervals(m)
	var groups []unitGroup
	for i := range functions {
		f := functions[i].f
		if i == 0 {
			groups = append(groups, unitGroup{id: 1, first: 0, last: 0})
			continue
		}
		previous := functions[i-1].f
		previousEnd := previous.Addr + uint32(previous.Size)
		same := f.Addr <= previousEnd
		if !same {
			covered, kinds := gapCoverage(covers, previousEnd, f.Addr)
			same = covered
			if covered {
				detail := fmt.Sprintf("%d bytes of %s between the functions", f.Addr-previousEnd, strings.Join(sortedKeys(kinds), "+"))
				if kinds["handwritten"] {
					evidence[i-1] = append(evidence[i-1], unitEvidence{"asm_object", 5, detail})
				} else if kinds["library"] {
					evidence[i-1] = append(evidence[i-1], unitEvidence{"library", 5, detail})
				}
			}
		}
		if same {
			if functions[i-1].library != functions[i].library {
				evidence[i-1] = append(evidence[i-1], unitEvidence{"library", 5, "configured library code meets non-library code"})
			}
			if functions[i-1].asm != functions[i].asm {
				evidence[i-1] = append(evidence[i-1], unitEvidence{"language_change", 5, "current assembly source meets C source"})
			}
			groups[len(groups)-1].last = i
			continue
		}
		groups = append(groups, unitGroup{id: len(groups) + 1, first: i, last: i})
	}
	for g := range groups {
		group := &groups[g]
		group.textStart = functions[group.first].f.Addr
		group.textEnd = functions[group.last].f.Addr + uint32(functions[group.last].f.Size)
		for i := group.first; i <= group.last; i++ {
			for _, rodata := range functions[i].f.Rodata {
				end := rodata.Addr + uint32(rodata.Size)
				if group.rodataEnd == group.rodataStart || rodata.Addr < group.rodataStart {
					group.rodataStart = rodata.Addr
				}
				if end > group.rodataEnd {
					group.rodataEnd = end
				}
			}
		}
	}
	for g := range groups {
		group := &groups[g]
		group.dataStart = group.textEnd
		if g+1 < len(groups) {
			group.dataEnd = groups[g+1].start()
		} else {
			group.dataEnd = ^uint32(0)
		}
	}
	return groups
}

func sortedKeys(set map[string]bool) []string {
	var keys []string
	for key := range set {
		keys = append(keys, key)
	}
	sort.Strings(keys)
	return keys
}

// unitDataRows collapses aliases at one address and assigns each address to
// the group whose range holds it: read-only tables before the group's code,
// hand-written blocks inside it, data after it, and bss past the file end.
func unitDataRows(m *moduleSpec, groups []unitGroup) []unitDataRow {
	symbols := append([]symbolSpec(nil), m.Data...)
	sort.SliceStable(symbols, func(i, j int) bool {
		if symbols[i].Addr != symbols[j].Addr {
			return symbols[i].Addr < symbols[j].Addr
		}
		return symbols[i].Name < symbols[j].Name
	})
	var rows []unitDataRow
	for _, symbol := range symbols {
		if len(rows) > 0 && rows[len(rows)-1].addr == symbol.Addr {
			rows[len(rows)-1].names = append(rows[len(rows)-1].names, symbol.Name)
			continue
		}
		rows = append(rows, unitDataRow{index: len(rows), addr: symbol.Addr, names: []string{symbol.Name}, section: -1})
	}
	for i := range rows {
		row := &rows[i]
		row.group = 0
		for g := range groups {
			if groups[g].start() <= row.addr {
				row.group = g
			}
		}
		group := &groups[row.group]
		switch {
		case row.addr < group.start():
			row.kind = "pre"
		case row.addr < group.textStart:
			row.kind = "rdata"
		case row.addr < group.textEnd:
			row.kind = "asm"
		case uint64(row.addr) >= m.end():
			row.kind = "bss"
		default:
			row.kind = "data"
		}
	}
	return rows
}

// ownersOf records which functions reference each data row, resolving names
// the way the linker does for this module.
func ownersOf(an *unitModuleAnalysis, resolver symbolResolver, maxOwners int) {
	byAddr := map[uint32]int{}
	for i := range an.rows {
		byAddr[an.rows[i].addr] = i
	}
	for i := range an.functions {
		for name := range an.functions[i].idents {
			addr, ok := resolver.resolve(an.m, name)
			if !ok {
				continue
			}
			rowIndex, ok := byAddr[addr]
			if !ok {
				continue
			}
			row := &an.rows[rowIndex]
			if len(row.owners) == 0 || row.owners[len(row.owners)-1] != i {
				row.owners = append(row.owners, i)
			}
		}
	}
	for i := range an.rows {
		an.rows[i].wide = len(an.rows[i].owners) > maxOwners
	}
}

// inferSections estimates data sections from configured addresses and row
// spacing. The small-data class is a spacing heuristic, not evidence of
// gp-relative access or an original section declaration.
func inferSections(an *unitModuleAnalysis, group *unitGroup) {
	byKind := map[string][]int{}
	for _, rowIndex := range group.rows {
		kind := an.rows[rowIndex].kind
		byKind[kind] = append(byKind[kind], rowIndex)
	}
	add := func(kind string, rows []int) {
		if len(rows) == 0 {
			return
		}
		section := unitSection{id: fmt.Sprintf("g%d.s%d", group.id, len(group.sections)), kind: kind, rows: rows,
			start: an.rows[rows[0]].addr, end: an.rows[rows[len(rows)-1]].addr + 1}
		previousLo, resets := -1, 0
		section.monotone = true
		for _, rowIndex := range rows {
			row := &an.rows[rowIndex]
			row.section = len(group.sections)
			if row.wide || len(row.owners) == 0 {
				continue
			}
			if previousLo >= 0 && row.lo() < previousLo {
				section.monotone = false
				resets++
			}
			previousLo = row.lo()
		}
		section.resets = resets
		group.sections = append(group.sections, section)
	}
	add("pre", byKind["pre"])
	add("rdata", byKind["rdata"])
	add("asm", byKind["asm"])
	data := byKind["data"]
	small := make([]bool, len(data))
	for i := range data {
		next := i + 1
		if next >= len(data) {
			next = i - 1
		}
		if next < 0 {
			continue
		}
		spacing := an.rows[data[next]].addr - an.rows[data[i]].addr
		if next < i {
			spacing = an.rows[data[i]].addr - an.rows[data[next]].addr
		}
		small[i] = spacing <= unitSmallSpacing
	}
	countSmall := func(from, to int) int {
		n := 0
		for i := from; i < to; i++ {
			if small[i] {
				n++
			}
		}
		return n
	}
	start := 0
	for i := unitSpacingWindow; i+unitSpacingWindow <= len(data); i++ {
		if i-start < unitSpacingWindow {
			continue
		}
		before, after := countSmall(i-unitSpacingWindow, i), countSmall(i, i+unitSpacingWindow)
		if (before <= 2 && after >= 7) || (before >= 7 && after <= 2) {
			kind := "initialized"
			if countSmall(start, i)*2 > i-start {
				kind = "small"
			}
			add(kind, data[start:i])
			start = i
		}
	}
	if start < len(data) {
		kind := "initialized"
		if countSmall(start, len(data))*2 > len(data)-start {
			kind = "small"
		}
		add(kind, data[start:])
	}
	add("bss", byKind["bss"])
}

// profileEvidence marks where the optimization level, assembler or division
// checks change between neighboring non-neutral functions.
func (an *unitModuleAnalysis) profileEvidence(group *unitGroup) {
	previous := -1
	for i := group.first; i <= group.last; i++ {
		fn := &an.functions[i]
		if fn.neutral || fn.library {
			continue
		}
		if previous >= 0 {
			prior := &an.functions[previous]
			var kind, detail string
			weight := 0
			if prior.base != fn.base {
				kind, weight, detail = "profile_run", 3, fmt.Sprintf("%s -> %s", prior.base, fn.base)
			} else if prior.hasDivision && fn.hasDivision && prior.divcheck != fn.divcheck {
				kind, weight, detail = "profile_run", 2, fmt.Sprintf("division checks %t -> %t", prior.divcheck, fn.divcheck)
			}
			if weight > 0 {
				cuts := i - previous
				if cuts > 1 {
					kind, weight = kind+"_interval", 1
					detail += fmt.Sprintf(" (somewhere across %d cuts)", cuts)
				} else if prior.base != fn.base {
					// Different configured optimization, assembler or small-data
					// settings require separate compiles under the current contracts.
					kind, weight = "profile_object", 6
				}
				for cut := previous; cut < i; cut++ {
					an.evidence[cut] = append(an.evidence[cut], unitEvidence{kind, weight, detail})
				}
			}
		}
		previous = i
	}
}

// scoreAndFormUnits combines the evidence for every cut inside each group
// and splits the group at the accepted boundaries.
func (an *unitModuleAnalysis) scoreAndFormUnits(options unitsOptions) {
	for g := range an.groups {
		group := &an.groups[g]
		an.profileEvidence(group)
		an.rodataAlignmentEvidence(group)
		group.cuts = make([]unitCut, 0, group.last-group.first)
		for i := group.first; i < group.last; i++ {
			cut := unitCut{after: i}
			cut.evidence = append(cut.evidence, an.evidence[i]...)
			cut.evidence = append(cut.evidence, an.ownershipEvidence(group, i)...)
			cut.evidence = append(cut.evidence, an.callEvidence(group, i)...)
			for _, evidence := range cut.evidence {
				cut.score += evidence.Weight
				switch evidence.Kind {
				case "asm_object", "library", "language_change", "rodata_object", "profile_object":
					cut.certain = true
				}
			}
			switch {
			case cut.certain, cut.score >= 6:
				cut.confidence = "high"
			case cut.score >= 4:
				cut.confidence = "medium"
			case cut.score >= 1:
				cut.confidence = "low"
			default:
				cut.confidence = "none"
			}
			group.cuts = append(group.cuts, cut)
		}
		an.formUnits(group, options)
	}
}

// ownershipEvidence scores one cut against the data: rows referenced on both
// sides by near neighbors argue against it; a section whose rows owned
// before the cut mostly precede the rows owned after it argues for it.
func (an *unitModuleAnalysis) ownershipEvidence(group *unitGroup, after int) []unitEvidence {
	var evidence []unitEvidence
	straddling := 0
	var names []string
	for _, rowIndex := range group.rows {
		row := &an.rows[rowIndex]
		if !row.local() || len(row.owners) < 2 || row.lo() > after || row.hi() <= after {
			continue
		}
		straddling++
		if len(names) < 3 {
			names = append(names, row.names[0])
		}
	}
	if straddling > 0 {
		weight := -3 * straddling
		if weight < -6 {
			weight = -6
		}
		evidence = append(evidence, unitEvidence{"ownership_straddle", weight,
			fmt.Sprintf("%d local rows referenced on both sides: %s", straddling, strings.Join(names, ", "))})
	}
	support := 0
	for _, section := range group.sections {
		if section.monotone || (section.kind != "initialized" && section.kind != "small" && section.kind != "rdata" && section.kind != "bss") {
			continue
		}
		var before, afterRows []uint32
		var beforeLo, afterLo []int
		for _, rowIndex := range section.rows {
			row := &an.rows[rowIndex]
			if row.wide || len(row.owners) == 0 {
				continue
			}
			switch {
			case row.hi() <= after:
				before = append(before, row.addr)
				beforeLo = append(beforeLo, row.lo())
			case row.lo() > after:
				afterRows = append(afterRows, row.addr)
				afterLo = append(afterLo, row.lo())
			}
		}
		if len(before) < 2 || len(afterRows) < 2 {
			continue
		}
		// Pairs out of order: an address owned before the cut lying past one
		// owned after it. A tenth of the pairs is tolerated because the
		// section boundaries are themselves inferred.
		bad := 0
		for _, addr := range before {
			bad += len(afterRows) - sort.Search(len(afterRows), func(i int) bool { return afterRows[i] > addr })
		}
		total := len(before) * len(afterRows)
		if bad*10 > total {
			continue
		}
		// Separation only means something where the rows on each side are
		// themselves out of owner order: a file's statics sit in whatever
		// order they were written, while a staircase of one table per
		// function separates at every cut.
		if ownerResets(beforeLo[max(0, len(beforeLo)-6):]) == 0 || ownerResets(afterLo[:min(6, len(afterLo))]) == 0 {
			continue
		}
		weight := 2
		support++
		evidence = append(evidence, unitEvidence{"ownership_order", weight,
			fmt.Sprintf("%s: %d rows owned before the cut precede %d owned after it (%d of %d pairs out of order)",
				section.id, len(before), len(afterRows), bad, total)})
		if support == 2 {
			break
		}
	}
	return evidence
}

// callEvidence scores nearby references to function names, including callback
// references. References confined to each side support a cut; crossing
// references oppose it. Neither pattern establishes an original file.
func (an *unitModuleAnalysis) callEvidence(group *unitGroup, after int) []unitEvidence {
	const window = 20
	crossing, left, right := 0, 0, 0
	for caller := after - window + 1; caller <= after+window; caller++ {
		if caller < group.first || caller > group.last {
			continue
		}
		for _, callee := range an.callees[caller] {
			if callee < group.first || callee > group.last {
				continue
			}
			low, high := caller, callee
			if low > high {
				low, high = high, low
			}
			if high-low > window {
				continue
			}
			switch {
			case low <= after && after < high:
				crossing++
			case high <= after:
				left++
			default:
				right++
			}
		}
	}
	switch {
	case crossing >= 3:
		return []unitEvidence{{"call_crossing", -2, fmt.Sprintf("%d nearby references to function names cross the cut", crossing)}}
	case crossing == 0 && left >= 2 && right >= 2:
		return []unitEvidence{{"call_locality", 1, fmt.Sprintf("%d nearby references to function names stay before the cut and %d after it, none cross", left, right)}}
	}
	return nil
}

// unitSizePenalty expresses the prior that an original file held a handful
// to a few dozen functions: single-function units and very long runs need
// correspondingly more evidence.
func unitSizePenalty(size int) int {
	switch {
	case size <= 1:
		return 3
	case size == 2:
		return 2
	case size == 3:
		return 1
	case size > 40:
		return (size - 40) / 10
	}
	return 0
}

// formUnits chooses the segmentation of the group that maximizes the summed
// evidence of its boundaries less the size penalties of its units. Certain
// boundaries are always taken; other cuts are eligible from the requested
// confidence upwards.
func (an *unitModuleAnalysis) formUnits(group *unitGroup, options unitsOptions) {
	minimum := unitConfidenceRank[options.minConfidence]
	n := group.last - group.first + 1
	// cut k (1 <= k < n) separates local functions k-1 and k.
	score := make([]int, n)
	eligible := make([]bool, n)
	forced := make([]bool, n)
	for k := 1; k < n; k++ {
		cut := &group.cuts[k-1]
		forced[k] = cut.certain
		eligible[k] = cut.certain || (cut.score > 0 && unitConfidenceRank[cut.confidence] >= minimum && cut.score >= options.minScore)
		if options.evidence != "" {
			// Evidence mode takes every qualifying cut and no others;
			// each unit spans the remaining interval under that policy.
			forced[k] = cut.evidenceEligible(options.evidence)
			eligible[k] = forced[k]
		}
		score[k] = cut.score
		if cut.certain && score[k] < 6 {
			score[k] = 6
		}
	}
	const unreachable = -1 << 30
	best := make([]int, n+1)
	back := make([]int, n+1)
	lastForced := 0
	for j := 1; j <= n; j++ {
		best[j] = unreachable
		for i := lastForced; i < j; i++ {
			if i > 0 && !eligible[i] {
				continue
			}
			candidate := best[i] - unitSizePenalty(j-i)
			if i > 0 {
				candidate += score[i]
			}
			if candidate > best[j] {
				best[j], back[j] = candidate, i
			}
		}
		if j < n && forced[j] {
			lastForced = j
		}
	}
	var starts []int
	for j := n; j > 0; j = back[j] {
		starts = append(starts, back[j])
	}
	sort.Ints(starts)
	for u, localStart := range starts {
		localEnd := n - 1
		if u+1 < len(starts) {
			localEnd = starts[u+1] - 1
		}
		start, end := group.first+localStart, group.first+localEnd
		unit := unitCandidate{id: fmt.Sprintf("%s.g%d.u%03d", an.m.ID, group.id, len(group.units)+1), prefixes: map[string]int{}}
		unit.start = an.functions[start].f.Addr
		unit.end = an.functions[end].f.Addr + uint32(an.functions[end].f.Size)
		unit.confidence = "high"
		if localStart > 0 && unitConfidenceRank[group.cuts[localStart-1].confidence] < unitConfidenceRank[unit.confidence] {
			unit.confidence = group.cuts[localStart-1].confidence
		}
		if localEnd+1 < n && unitConfidenceRank[group.cuts[localEnd].confidence] < unitConfidenceRank[unit.confidence] {
			unit.confidence = group.cuts[localEnd].confidence
		}
		profiles := map[string]int{}
		shared := map[string]int{}
		for j := start; j <= end; j++ {
			fn := &an.functions[j]
			fn.unit = len(group.units)
			unit.functions = append(unit.functions, j)
			unit.prefixes[fn.prefix]++
			profiles[fn.profile]++
			if sub := sharedSubsystemOf(fn.source); sub != "" {
				shared["src/"+sub]++
			}
		}
		unit.profile = mostCommon(profiles)
		unit.suggestedName = mostCommon(unit.prefixes)
		if subsystem := mostCommon(shared); subsystem != "" && shared[subsystem]*2 >= len(unit.functions) {
			unit.suggestedName += " (" + subsystem + ")"
		}
		for _, rowIndex := range group.rows {
			row := &an.rows[rowIndex]
			if row.wide || len(row.owners) == 0 || row.lo() < start || row.hi() > end {
				continue
			}
			unit.rows = append(unit.rows, rowIndex)
		}
		group.units = append(group.units, unit)
	}
}

// ownerResets counts how often the owning function index decreases along a
// run of rows in address order.
func ownerResets(lows []int) int {
	resets := 0
	for i := 1; i < len(lows); i++ {
		if lows[i] < lows[i-1] {
			resets++
		}
	}
	return resets
}

func mostCommon(counts map[string]int) string {
	best, bestCount := "", 0
	for key, count := range counts {
		if count > bestCount || (count == bestCount && key < best) {
			best, bestCount = key, count
		}
	}
	return best
}

// findCounterparts links each unit to partner-module units that hold at
// least three of its aligned functions.
func (an *unitModuleAnalysis) findCounterparts(analyses map[string]*unitModuleAnalysis) {
	for g := range an.groups {
		group := &an.groups[g]
		for u := range group.units {
			unit := &group.units[u]
			counts := map[[2]string]int{}
			for _, index := range unit.functions {
				for partnerID, name := range an.functions[index].aligned {
					partner := analyses[partnerID]
					partnerIndex, ok := partner.byName[name]
					if !ok {
						continue
					}
					fn := &partner.functions[partnerIndex]
					if fn.unit < 0 {
						continue
					}
					counts[[2]string{partnerID, partner.groups[fn.group].units[fn.unit].id}]++
				}
			}
			var keys [][2]string
			for key, count := range counts {
				if count >= 3 {
					keys = append(keys, key)
				}
			}
			sort.Slice(keys, func(i, j int) bool {
				if counts[keys[i]] != counts[keys[j]] {
					return counts[keys[i]] > counts[keys[j]]
				}
				return keys[i][1] < keys[j][1]
			})
			for _, key := range keys {
				unit.counterparts = append(unit.counterparts, unitsReportCounterpart{Module: key[0], Unit: key[1], Matched: counts[key]})
			}
		}
	}
}

func (an *unitModuleAnalysis) collectDiagnostics() {
	for i := range an.rows {
		row := &an.rows[i]
		switch {
		case len(row.owners) == 0:
			an.diagnostics.UnownedData = append(an.diagnostics.UnownedData, strings.Join(row.names, "/"))
		case row.wide:
			an.diagnostics.WidelyReferenced = append(an.diagnostics.WidelyReferenced, unitsReportWide{Name: row.names[0], Owners: len(row.owners)})
		case row.local() && len(row.owners) >= 2 && row.hi() > row.lo():
			an.diagnostics.Straddlers = append(an.diagnostics.Straddlers, unitsReportStraddlerRow{Name: row.names[0],
				First: an.functions[row.lo()].f.Name, Last: an.functions[row.hi()].f.Name})
		}
	}
	for g := range an.groups {
		group := &an.groups[g]
		var previous uint32
		for i := group.first; i <= group.last; i++ {
			for _, rodata := range an.functions[i].f.Rodata {
				if rodata.Addr < previous {
					an.diagnostics.RodataOrderViolation = append(an.diagnostics.RodataOrderViolation,
						unitsReportRodataOrder{Function: an.functions[i].f.Name, Rodata: hexAddr(rodata.Addr), Previous: hexAddr(previous)})
				}
				if rodata.Addr > previous {
					previous = rodata.Addr
				}
			}
		}
	}
}
