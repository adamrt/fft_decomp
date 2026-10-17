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

// placement reports candidate game-library locations from link position and
// call neighborhoods. Shared neighbors suggest a placement to review; they
// do not prove an original object boundary. The report is read-only;
// `symbols move` applies accepted rows.

const placementSchemaVersion = 1

type placementOptions struct {
	outDir   string
	minScore int
}

type placementRow struct {
	Module      string   `yaml:"module"`
	Function    string   `yaml:"function"`
	Addr        string   `yaml:"addr"`
	Size        int      `yaml:"size"`
	Hash        string   `yaml:"hash"`
	Unit        string   `yaml:"unit"`
	Source      string   `yaml:"source"`
	Verdict     string   `yaml:"verdict"`
	Score       int      `yaml:"score"`
	Subsystem   string   `yaml:"subsystem,omitempty"`
	NewName     string   `yaml:"new_name,omitempty"`
	Destination string   `yaml:"destination,omitempty"`
	Family      string   `yaml:"family,omitempty"`
	Apply       bool     `yaml:"apply"`
	Evidence    []string `yaml:"evidence,omitempty"`
	Note        string   `yaml:"note,omitempty"`
}

// placementMember is one body of a family: counterpart functions that the
// object sequences align, or that would take one name.
type placementMember struct {
	Module   string `yaml:"module"`
	Function string `yaml:"function"`
	Size     int    `yaml:"size"`
	Source   string `yaml:"source"`
	Body     string `yaml:"body"`
	Folder   string `yaml:"folder"`
	Note     string `yaml:"note,omitempty"`
}

// placementFamily proposes one name for counterpart bodies: a share when
// the twins comparison finds one body, otherwise the same name with one
// body per folder (twin bodies still share one file).
type placementFamily struct {
	ID        string            `yaml:"id"`
	Name      string            `yaml:"name"`
	Subsystem string            `yaml:"subsystem,omitempty"`
	Kind      string            `yaml:"kind"`
	Apply     bool              `yaml:"apply"`
	Note      string            `yaml:"note,omitempty"`
	Members   []placementMember `yaml:"members"`
}

type placementReport struct {
	SchemaVersion int            `yaml:"schema_version"`
	CandidateOnly bool           `yaml:"candidate_only"`
	Module        string         `yaml:"module"`
	Totals        map[string]int `yaml:"totals"`
	BySubsystem   map[string]int `yaml:"moves_by_subsystem"`
	Moves         []placementRow `yaml:"moves"`
	Review        []placementRow `yaml:"review"`
	Families      []string       `yaml:"families,omitempty"`
}

// placementSignals are the facts one function's verdict is scored from.
type placementSignals struct {
	prev, next   string         // shared subsystem of the address neighbors, "" when not shared
	unitDominant string         // shared subsystem holding at least half of the unit's functions
	callees      map[string]int // shared subsystem -> module functions this one names
	callers      map[string]int // shared subsystem -> module functions naming this one
	calleeTotal  int
	callerTotal  int
}

// placementScore picks the subsystem with the most evidence and says how much.
func placementScore(s placementSignals) (string, int, []string) {
	scores := map[string]int{}
	var evidence []string
	add := func(sub string, weight int, why string) {
		if sub == "" {
			return
		}
		scores[sub] += weight
		evidence = append(evidence, fmt.Sprintf("%s +%d %s", sub, weight, why))
	}
	if s.prev != "" && s.prev == s.next {
		add(s.prev, 3, "both address neighbors are shared")
	} else {
		add(s.prev, 1, "the previous function is shared")
		add(s.next, 1, "the next function is shared")
	}
	add(s.unitDominant, 2, "the unit is mostly shared")
	if sub, count := mostCommonCount(s.callees); sub != "" && count*2 >= s.calleeTotal {
		add(sub, 1, fmt.Sprintf("%d of %d named callees", count, s.calleeTotal))
	}
	if sub, count := mostCommonCount(s.callers); sub != "" && count*2 >= s.callerTotal {
		add(sub, 1, fmt.Sprintf("%d of %d module callers", count, s.callerTotal))
	}
	best, bestScore := "", 0
	for _, sub := range sortedKeys(scoresKeys(scores)) {
		score := scores[sub]
		if score > bestScore || (score == bestScore && sub == s.prev) {
			best, bestScore = sub, score
		}
	}
	return best, bestScore, evidence
}

func scoresKeys(scores map[string]int) map[string]bool {
	keys := map[string]bool{}
	for key := range scores {
		keys[key] = true
	}
	return keys
}

func mostCommonCount(counts map[string]int) (string, int) {
	best, bestCount := "", 0
	for _, key := range sortedKeys(scoresKeys(counts)) {
		if counts[key] > bestCount {
			best, bestCount = key, counts[key]
		}
	}
	return best, bestCount
}

// placementVerdict turns a score into move, review or keep: a move needs the
// evidence to include an address neighbor of the chosen subsystem.
func placementVerdict(sub string, score int, s placementSignals, minScore int) string {
	switch {
	case sub == "" || score < 2:
		return "keep"
	case score >= minScore && (s.prev == sub || s.next == sub):
		return "move"
	default:
		return "review"
	}
}

// variantSuffix marked a function whose body each module linked separately
// under one shared name; the placement report still recognizes it.
const variantSuffix = "_variant"

// placementNewName preserves the function's concept name when its source moves.
func placementNewName(name string) (string, string) {
	if strings.HasSuffix(name, variantSuffix) {
		return "", "a _variant body stays beside its module"
	}
	return name, ""
}

// programFolders are the top-level src/ folders that hold one program's own
// code rather than a subsystem.
var programFolders = map[string]bool{"battle": true, "world": true, "wldcore": true, "main": true, "open": true, "event": true, "effect": true, "lib": true, "platform": true}

// subsystemFolder returns the path below src for a game-library folder.
// Keep lib/<concept> intact so destinations preserve the nested source layout.
func subsystemFolder(source string) string {
	parts := strings.Split(source, "/")
	if len(parts) >= 3 && parts[0] == "src" && parts[1] == "lib" {
		if len(parts) == 3 {
			return "lib"
		}
		return strings.Join(parts[1:3], "/")
	}
	if len(parts) > 2 && parts[0] == "src" && !programFolders[parts[1]] {
		return parts[1]
	}
	return ""
}

// sharedSubsystemOf names the game-library folder of a source directly in it.
// Bodies in program subfolders stay with those programs. Function name tokens
// are not evidence of ownership or an original object boundary.
func sharedSubsystemOf(source string) string {
	sub := subsystemFolder(source)
	if sub == "" {
		return ""
	}
	if filepath.ToSlash(filepath.Dir(source)) != "src/"+sub {
		return ""
	}
	return sub
}

func (p project) placementCommand(args []string) error {
	flags := flag.NewFlagSet("placement", flag.ContinueOnError)
	module := flags.String("module", "", "modules to report (ids, aliases, event or effect)")
	minScore := flags.Int("min-score", 4, "lowest score that proposes a move")
	out := flags.String("out", "build/analysis/placement", "report directory under build/")
	if err := flags.Parse(args); err != nil {
		return err
	}
	usage := errors.New("usage: tools placement [--module=MODULES] [--min-score=N] [--out=build/DIR]")
	if flags.NArg() != 0 || *minScore < 1 {
		return usage
	}
	outDir := filepath.ToSlash(filepath.Clean(*out))
	if outDir != "build" && !strings.HasPrefix(outDir, "build/") {
		return errors.New("placement: --out must name a directory under build/")
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
	taken := map[string]bool{}
	for _, m := range config.Modules {
		for index := range m.Functions {
			taken[m.Functions[index].Name] = true
			taken[m.Functions[index].sourceSymbol()] = true
		}
		for _, symbol := range append(append([]symbolSpec(nil), m.Data...), m.Imports...) {
			taken[symbol.Name] = true
		}
	}
	analyses, err := p.reconstructUnits(config, unitsOptions{minConfidence: "low", maxOwners: 3, outDir: outDir})
	if err != nil {
		return err
	}
	dir := filepath.Join(p.root, filepath.FromSlash(outDir))
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return err
	}
	options := placementOptions{outDir: outDir, minScore: *minScore}
	reports := map[string]*placementReport{}
	for _, m := range config.Modules {
		report := p.placementReport(analyses[m.ID], options, taken)
		reports[m.ID] = &report
	}
	twinClass := map[string]string{}
	if twins, err := p.findTwins(config, twinsOptions{minBytes: 8, maxNear: 6}); err != nil {
		fmt.Fprintf(p.stdout(), "placement: twins unavailable (%v); families compare sources only\n", err)
	} else {
		for _, group := range twins.Groups {
			for _, member := range group.Members {
				twinClass[member.Module+":"+member.Name] = group.ID
			}
		}
	}
	families := placementFamilies(config, analyses, reports, twinClass)
	var selectedReports []placementReport
	for _, m := range selected {
		report := reports[m.ID]
		data, err := renderPlacementYAML(*report)
		if err != nil {
			return err
		}
		if err := atomicWrite(filepath.Join(dir, m.ID+".yaml"), data); err != nil {
			return err
		}
		selectedReports = append(selectedReports, *report)
		fmt.Fprintf(p.stdout(), "placement: %s: %d candidates, %d move, %d review, %d keep, %d variant -> %s\n",
			m.ID, report.Totals["candidates"], report.Totals["move"], report.Totals["review"], report.Totals["keep"], report.Totals["variant"], outDir+"/"+m.ID+".yaml")
	}
	data, err := renderPlacementFamiliesYAML(families)
	if err != nil {
		return err
	}
	if err := atomicWrite(filepath.Join(dir, "families.yaml"), data); err != nil {
		return err
	}
	fmt.Fprintf(p.stdout(), "placement: %d families -> %s/families.yaml\n", len(families), outDir)
	return atomicWrite(filepath.Join(dir, "summary.md"), renderPlacementMarkdown(selectedReports, families))
}

func (p project) placementReport(an *unitModuleAnalysis, options placementOptions, taken map[string]bool) placementReport {
	report := placementReport{SchemaVersion: placementSchemaVersion, CandidateOnly: true, Module: an.m.ID,
		Totals: map[string]int{"candidates": 0, "move": 0, "review": 0, "keep": 0, "variant": 0}, BySubsystem: map[string]int{},
		Moves: []placementRow{}, Review: []placementRow{}}
	callers := map[int][]int{}
	for caller, callees := range an.callees {
		for _, callee := range callees {
			callers[callee] = append(callers[callee], caller)
		}
	}
	subsystemOf := func(index int) string {
		if an.functions[index].library {
			return ""
		}
		return sharedSubsystemOf(an.functions[index].source)
	}
	for i := range an.functions {
		fn := &an.functions[i]
		if fn.library || fn.asm || fn.source == "" || subsystemFolder(fn.source) != "" {
			continue
		}
		report.Totals["candidates"]++
		group := &an.groups[fn.group]
		unit := &group.units[fn.unit]
		signals := placementSignals{callees: map[string]int{}, callers: map[string]int{}}
		if i-1 >= group.first {
			signals.prev = subsystemOf(i - 1)
		}
		if i+1 <= group.last {
			signals.next = subsystemOf(i + 1)
		}
		shared := map[string]int{}
		for _, j := range unit.functions {
			if sub := subsystemOf(j); sub != "" {
				shared[sub]++
			}
		}
		if sub, count := mostCommonCount(shared); sub != "" && count*2 >= len(unit.functions) {
			signals.unitDominant = sub
		}
		for _, j := range an.callees[i] {
			signals.calleeTotal++
			if sub := subsystemOf(j); sub != "" {
				signals.callees[sub]++
			}
		}
		for _, j := range callers[i] {
			signals.callerTotal++
			if sub := subsystemOf(j); sub != "" {
				signals.callers[sub]++
			}
		}
		sub, score, evidence := placementScore(signals)
		verdict := placementVerdict(sub, score, signals, options.minScore)
		if strings.HasSuffix(fn.f.sourceSymbol(), variantSuffix) {
			verdict = "variant"
		}
		row := placementRow{Module: an.m.ID, Function: fn.f.Name, Addr: hexAddr(fn.f.Addr), Size: fn.f.Size, Hash: fn.f.Hash, Unit: unit.id,
			Source: fn.source, Verdict: verdict, Score: score, Subsystem: sub, Evidence: evidence}
		for partner, name := range fn.aligned {
			row.Evidence = append(row.Evidence, fmt.Sprintf("aligned with %s:%s, separate body", partner, name))
		}
		sort.Strings(row.Evidence[len(evidence):])
		if verdict != "keep" && verdict != "variant" {
			var note string
			row.NewName, note = placementNewName(fn.f.sourceSymbol())
			row.Apply = row.NewName != ""
			if row.Apply {
				row.Destination = "src/" + sub + "/" + row.NewName + ".c"
				if row.NewName != fn.f.sourceSymbol() && taken[row.NewName] {
					row.Apply, note = false, "name "+row.NewName+" is taken"
				} else if _, err := os.Stat(filepath.Join(p.root, filepath.FromSlash(row.Destination))); err == nil {
					row.Apply, note = false, row.Destination+" exists"
				}
			}
			row.Note = note
		}
		report.Totals[verdict]++
		switch verdict {
		case "move":
			report.BySubsystem[sub]++
			report.Moves = append(report.Moves, row)
		case "review":
			report.Review = append(report.Review, row)
		}
	}
	return report
}

// placementFamilies joins counterpart bodies across modules: functions the
// object sequences align, rows that would take one name, and rows whose new
// name an existing definition holds. Members with one source, or in one
// twins group, are one body. A family with one body is a share; one with
// several keeps the same role name with a body per folder.
func placementFamilies(config *projectConfig, analyses map[string]*unitModuleAnalysis, reports map[string]*placementReport, twinClass map[string]string) []placementFamily {
	type node struct {
		module string
		fn     *unitFunction
	}
	nodes := map[string]node{}
	parent := map[string]string{}
	var find func(string) string
	find = func(key string) string {
		for parent[key] != key {
			parent[key] = parent[parent[key]]
			key = parent[key]
		}
		return key
	}
	union := func(a, b string) {
		if _, ok := nodes[a]; !ok {
			return
		}
		if _, ok := nodes[b]; !ok {
			return
		}
		ra, rb := find(a), find(b)
		if ra != rb {
			if ra > rb {
				ra, rb = rb, ra
			}
			parent[rb] = ra
		}
	}
	definitions := map[string][]string{}
	for _, m := range config.Modules {
		an := analyses[m.ID]
		for i := range an.functions {
			fn := &an.functions[i]
			if fn.library || fn.asm || fn.source == "" {
				continue
			}
			key := m.ID + ":" + fn.f.Name
			nodes[key] = node{m.ID, fn}
			parent[key] = key
			definitions[fn.f.sourceSymbol()] = append(definitions[fn.f.sourceSymbol()], key)
		}
	}
	for key, item := range nodes {
		for partner, name := range item.fn.aligned {
			union(key, partner+":"+name)
		}
	}
	byNewName := map[string][]string{}
	rowOf := map[string]*placementRow{}
	for _, m := range config.Modules {
		report := reports[m.ID]
		for _, rows := range [][]placementRow{report.Moves, report.Review} {
			for index := range rows {
				row := &rows[index]
				key := m.ID + ":" + row.Function
				rowOf[key] = row
				if row.NewName == "" {
					continue
				}
				byNewName[row.NewName] = append(byNewName[row.NewName], key)
				for _, other := range definitions[row.NewName] {
					union(key, other)
				}
			}
		}
	}
	for _, keys := range byNewName {
		for _, key := range keys[1:] {
			union(keys[0], key)
		}
	}
	groups := map[string][]string{}
	for key := range nodes {
		root := find(key)
		groups[root] = append(groups[root], key)
	}
	var families []placementFamily
	for _, root := range sortedKeys(scoresKeys(groupKeys(groups))) {
		keys := groups[root]
		if len(keys) < 2 {
			continue
		}
		sort.Strings(keys)
		// Body classes: one source, or one twins group, is one body.
		class := map[string]string{}
		classOf := func(key string) string {
			item := nodes[key]
			if group := twinClass[key]; group != "" {
				return "twins:" + group
			}
			return "source:" + item.fn.source
		}
		for _, key := range keys {
			class[key] = classOf(key)
		}
		// A twins group and a plain source can name the same body.
		for _, key := range keys {
			if group := twinClass[key]; group != "" {
				for _, other := range keys {
					if twinClass[other] == "" && nodes[other].fn.source == nodes[key].fn.source {
						class[other] = class[key]
					}
				}
			}
		}
		bodies := map[string][]string{}
		for _, key := range keys {
			bodies[class[key]] = append(bodies[class[key]], key)
		}
		candidates := 0
		subsystems := map[string]int{}
		newNames := map[string]int{}
		variantName, sharedName := "", ""
		for _, key := range keys {
			item := nodes[key]
			symbol := item.fn.f.sourceSymbol()
			if strings.HasSuffix(symbol, variantSuffix) && variantName == "" {
				variantName = symbol
			}
			if sub := sharedSubsystemOf(item.fn.source); sub != "" {
				subsystems[sub] += 2
				if sharedName == "" {
					sharedName = symbol
				}
			}
			if row := rowOf[key]; row != nil {
				candidates++
				subsystems[row.Subsystem]++
				if row.NewName != "" {
					newNames[row.NewName]++
				}
			}
		}
		if candidates == 0 || len(bodies) < 1 {
			continue
		}
		family := placementFamily{ID: fmt.Sprintf("f%03d", len(families)+1), Apply: true}
		family.Subsystem, _ = mostCommonCount(subsystems)
		common, _ := mostCommonCount(newNames)
		switch {
		case len(bodies) == 1:
			family.Kind = "share"
			family.Name = common
			if sharedName != "" {
				family.Name = sharedName
			}
		default:
			family.Kind = "variant"
			switch {
			case variantName != "":
				family.Name = variantName
			case sharedName != "":
				family.Name = sharedName
			default:
				family.Name = common
			}
		}
		if family.Name == "" {
			family.Name, family.Apply, family.Note = "?", false, "no name to propose"
		}
		shared := "src/" + family.Subsystem
		bodyIDs := map[string]string{}
		folderOf := map[string]string{}
		claimed := map[string]string{}
		for _, key := range keys {
			id := class[key]
			if bodyIDs[id] != "" {
				continue
			}
			bodyIDs[id] = fmt.Sprintf("b%d", len(bodyIDs)+1)
			folder := ""
			modules := map[string]bool{}
			for _, member := range bodies[id] {
				modules[nodes[member].module] = true
				if sub := sharedSubsystemOf(nodes[member].fn.source); sub != "" {
					folder = "src/" + sub
				}
			}
			if folder == "" && (len(modules) > 1 || family.Kind == "share") {
				folder = shared
			}
			if folder == "" {
				folder = filepath.ToSlash(filepath.Dir(nodes[bodies[id][0]].fn.source))
			}
			if other, taken := claimed[folder]; taken && other != id {
				family.Apply = false
				family.Note = appendNote(family.Note, "two bodies would sit in "+folder)
			}
			claimed[folder] = id
			folderOf[id] = folder
		}
		for _, key := range keys {
			item := nodes[key]
			id := class[key]
			member := placementMember{Module: item.module, Function: item.fn.f.Name, Size: item.fn.f.Size, Source: item.fn.source,
				Body: bodyIDs[id], Folder: folderOf[id]}
			family.Members = append(family.Members, member)
		}
		if len(bodies) == 1 && len(keys) > 1 {
			single := true
			for _, key := range keys {
				if nodes[key].fn.source != nodes[keys[0]].fn.source {
					single = false
				}
			}
			if single {
				continue
			}
		}
		families = append(families, family)
		for _, key := range keys {
			if row := rowOf[key]; row != nil {
				row.Family = family.ID
				reports[nodes[key].module].Families = appendUnique(reports[nodes[key].module].Families, family.ID)
			}
		}
	}
	return families
}

func appendNote(existing, note string) string {
	if existing == "" {
		return note
	}
	if strings.Contains(existing, note) {
		return existing
	}
	return existing + "; " + note
}

func groupKeys(groups map[string][]string) map[string]int {
	keys := map[string]int{}
	for key := range groups {
		keys[key] = 1
	}
	return keys
}

func appendUnique(list []string, item string) []string {
	for _, existing := range list {
		if existing == item {
			return list
		}
	}
	return append(list, item)
}

func renderPlacementFamiliesYAML(families []placementFamily) ([]byte, error) {
	var out bytes.Buffer
	out.WriteString("# Counterpart families across modules: one name per family, a body per folder.\n")
	encoder := yaml.NewEncoder(&out)
	encoder.SetIndent(2)
	if err := encoder.Encode(struct {
		Families []placementFamily `yaml:"families"`
	}{families}); err != nil {
		return nil, err
	}
	if err := encoder.Close(); err != nil {
		return nil, err
	}
	return out.Bytes(), nil
}

func renderPlacementYAML(report placementReport) ([]byte, error) {
	var out bytes.Buffer
	out.WriteString("# Subsystem placement of module-folder functions: candidates for review, derived from link order.\n")
	encoder := yaml.NewEncoder(&out)
	encoder.SetIndent(2)
	if err := encoder.Encode(report); err != nil {
		return nil, err
	}
	if err := encoder.Close(); err != nil {
		return nil, err
	}
	return out.Bytes(), nil
}

func renderPlacementMarkdown(reports []placementReport, families []placementFamily) []byte {
	var out strings.Builder
	out.WriteString("# Subsystem placement\n\n")
	out.WriteString("Candidates only. A function compiled between shared routines of one subsystem, inside a\n")
	out.WriteString("unit that is mostly that subsystem, came from the same source file as they did even when\n")
	out.WriteString("no other module links it. `move` rows have a shared address neighbor and a score of at\n")
	out.WriteString("least the threshold; `review` rows have weaker evidence. `apply` is false when the new name\n")
	out.WriteString("is taken, the destination exists or the body is a `_variant`.\n\n")
	out.WriteString("| module | candidates | move | review | keep | moves by subsystem |\n|---|---:|---:|---:|---:|---|\n")
	for _, report := range reports {
		var parts []string
		for _, sub := range sortedKeys(scoresKeys(report.BySubsystem)) {
			parts = append(parts, fmt.Sprintf("%s %d", sub, report.BySubsystem[sub]))
		}
		fmt.Fprintf(&out, "| %s | %d | %d | %d | %d | %s |\n", report.Module, report.Totals["candidates"],
			report.Totals["move"], report.Totals["review"], report.Totals["keep"], strings.Join(parts, ", "))
	}
	if len(families) > 0 {
		out.WriteString("\n## Families\n\nCounterpart bodies the object sequences align or that would take one name. `share` families\n")
		out.WriteString("are one body by the twins comparison; `variant` families take one `_variant` name with a body per folder.\n\n")
		out.WriteString("| id | kind | name | members | apply | note |\n|---|---|---|---|---|---|\n")
		for _, family := range families {
			var members []string
			for _, member := range family.Members {
				members = append(members, fmt.Sprintf("%s:%s (%d B, %s, %s)", member.Module, member.Function, member.Size, member.Body, member.Folder))
			}
			fmt.Fprintf(&out, "| %s | %s | %s | %s | %t | %s |\n", family.ID, family.Kind, family.Name, strings.Join(members, "; "), family.Apply, family.Note)
		}
	}
	for _, report := range reports {
		if len(report.Moves) == 0 && len(report.Review) == 0 {
			continue
		}
		fmt.Fprintf(&out, "\n## %s\n", report.Module)
		for _, section := range []struct {
			title string
			rows  []placementRow
		}{{"Moves", report.Moves}, {"Review", report.Review}} {
			if len(section.rows) == 0 {
				continue
			}
			fmt.Fprintf(&out, "\n### %s\n\n| function | size | unit | destination | score | family | apply | note |\n|---|---:|---|---|---:|---|---|---|\n", section.title)
			for _, row := range section.rows {
				destination := row.Destination
				if destination == "" {
					destination = row.Subsystem
				}
				fmt.Fprintf(&out, "| %s | %d | %s | %s | %d | %s | %t | %s |\n", row.Function, row.Size, row.Unit, destination, row.Score, row.Family, row.Apply, row.Note)
			}
		}
	}
	return []byte(out.String())
}
