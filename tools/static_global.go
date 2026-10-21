package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"
)

// The static command turns a file-private extern global into a static that its
// file owns, or explains why it cannot be one and what it is instead.
//
//	tools static [--dry-run] [--size N] [--no-note] NAME
//	tools static --restore [--failed] NAME
//
// It prints one line per check. When every check passes it backs up the
// files it changes under build/static-backup/, rewrites the declaration, adds
// the unit's data: or bss: row with the original bytes' hash, and prints
// "static-applied:", "static-source:" and "static-modules:" lines for
// tools/static, which then formats the file and runs make check. --restore puts
// the backup back; with --failed it also leaves a `Not static:` comment.

const staticNote = "/* Not static: "

var staticScalarSizes = map[string]int{
	"u8": 1, "s8": 1, "char": 1, "u16": 2, "s16": 2, "short": 2, "u32": 4, "s32": 4, "int": 4, "long": 4,
	"SVECTOR": 8, "VECTOR": 16, "MATRIX": 32, "CVECTOR": 4, "RECT": 8, "DVECTOR": 4,
}

var staticSigned = map[string]bool{"s8": true, "s16": true, "s32": true, "short": true, "int": true, "long": true, "char": true}

type staticReport struct {
	lines   []string
	reasons []string
}

func (r *staticReport) ok(format string, args ...any) {
	r.lines = append(r.lines, "  ok   "+fmt.Sprintf(format, args...))
}

func (r *staticReport) no(reason, format string, args ...any) {
	r.lines = append(r.lines, "  NO   "+fmt.Sprintf(format, args...))
	r.reasons = append(r.reasons, reason)
}

func (r *staticReport) info(format string, args ...any) {
	r.lines = append(r.lines, "       "+fmt.Sprintf(format, args...))
}

// staticDeclaration is the file-scope extern line being rewritten.
type staticDeclaration struct {
	path       string
	text       string
	start, end int // byte range of the whole line, without its newline
	typeText   string
	dimensions string
	trailing   string
}

// staticPlan is one program's row for the new static.
type staticPlan struct {
	module *moduleSpec
	unit   *unitDef
	addr   uint32
	kind   string // "data" or "bss"
	init   string // C initializer, including " = ", or empty
	hash   string
	// anchor is the variable the new row sits next to; the definition goes
	// right after it (after) or right before it, because a unit's rows follow
	// the order of its definitions. Empty for a unit's first bss row.
	anchors []string // nearest first
	after   bool
}

func (p project) staticCommand(args []string) error {
	flags := flag.NewFlagSet("static", flag.ContinueOnError)
	dryRun := flags.Bool("dry-run", false, "report only; change no files")
	size := flags.Int("size", 0, "size in bytes, when the type's size cannot be worked out")
	noNote := flags.Bool("no-note", false, "do not add a `Not static:` comment when it cannot be static")
	restore := flags.Bool("restore", false, "put back the files the last apply changed")
	failed := flags.Bool("failed", false, "with --restore: also note that the static did not match")
	survey := flags.Bool("survey", false, "check every file-scope extern in src/ (no NAME); change no files")
	batch := flags.Bool("batch", false, "convert every name the survey finds, round by round (no NAME); no checks, no notes")
	if err := flags.Parse(args); err != nil {
		return err
	}
	if *survey || *batch {
		if flags.NArg() != 0 {
			return fmt.Errorf("usage: tools static --survey | --batch")
		}
		return p.staticSurvey(*batch)
	}
	if flags.NArg() != 1 {
		return fmt.Errorf("usage: tools static [--dry-run] [--size N] [--no-note] NAME | --restore [--failed] NAME | --survey")
	}
	name := flags.Arg(0)
	if err := validateRenameIdentifier(name); err != nil {
		return err
	}
	if *restore {
		return p.staticRestore(name, *failed)
	}
	config, err := p.loadProjectConfig()
	if err != nil {
		return err
	}
	fmt.Println("static: " + name)
	analysis, err := p.staticAnalyze(config, name, *size)
	if err != nil {
		return err
	}
	report, declaration, plans, byteSize := analysis.report, analysis.declaration, analysis.plans, analysis.size
	fmt.Println(strings.Join(report.lines, "\n"))

	if len(report.reasons) != 0 {
		fmt.Println("static: cannot be static: " + strings.Join(report.reasons, "; "))
		if *dryRun || *noNote || declaration == nil {
			return nil
		}
		added, err := staticAddNote(p.root, declaration, report.reasons)
		if err != nil {
			return err
		}
		if added {
			fmt.Println("static: added a `Not static:` comment above the extern")
			fmt.Println("static-note: " + declaration.path)
		}
		return nil
	}

	newLine := fmt.Sprintf("static %s %s%s%s;%s", declaration.typeText, name, strings.TrimRight(declaration.dimensions, " "), plans[0].init, declaration.trailing)
	fmt.Println("static: can be static: " + strings.TrimSpace(newLine))
	if *dryRun {
		return nil
	}
	return p.staticApply(config, declaration, newLine, plans, byteSize)
}

// staticAnalysis is the outcome of every check for one name.
type staticAnalysis struct {
	report      *staticReport
	declaration *staticDeclaration
	plans       []staticPlan
	files       []string
	size        int
}

func (p project) staticAnalyze(config *projectConfig, name string, size int) (*staticAnalysis, error) {
	report := &staticReport{}
	files, err := staticFilesSpelling(p.root, name)
	if err != nil {
		return nil, err
	}
	if len(files) == 0 {
		return nil, fmt.Errorf("%s is not spelled in src/ or include/", name)
	}
	headers, err := staticHeaderText(p.root)
	if err != nil {
		return nil, err
	}

	// Who uses the name, and the one declaration to rewrite.
	var sources []string
	for _, path := range files {
		if strings.HasSuffix(path, ".c") {
			sources = append(sources, path)
		}
	}
	source := ""
	if len(files) == 1 && len(sources) == 1 {
		source = sources[0]
		report.ok("only %s uses it", source)
	} else {
		report.no(fmt.Sprintf("%d files use it (%s)", len(files), strings.Join(files, ", ")), "used in %s", strings.Join(files, ", "))
	}
	declarations, err := staticExternDeclarations(p.root, sources, name)
	if err != nil {
		return nil, err
	}
	var declaration *staticDeclaration
	if len(declarations) == 1 {
		declaration = &declarations[0]
		report.ok("declared as `%s`", strings.TrimSpace(declaration.text[declaration.start:declaration.end]))
	} else if len(declarations) == 0 {
		report.no("declared in a header, not as a file-scope extern in one .c file", "no file-scope `extern` line in a .c file")
	} else {
		report.no("declared as an extern in several .c files", "%d file-scope `extern` lines in .c files", len(declarations))
	}

	// Size.
	byteSize, count, element := size, 0, 0
	if declaration != nil {
		count, element = staticTypeSize(declaration.typeText, declaration.dimensions, headers, declaration.text)
		if byteSize == 0 && count > 0 && element > 0 {
			byteSize = count * element
		}
		if byteSize == 0 {
			report.no("its size is unknown (pass --size N)", "size of `%s %s%s` unknown", declaration.typeText, name, strings.TrimSpace(declaration.dimensions))
		} else {
			report.ok("%d bytes", byteSize)
			if count == 0 && element > 0 {
				count = byteSize / element
			}
		}
	}

	// Per program.
	var plans []staticPlan
	if source != "" && declaration != nil && byteSize > 0 {
		plans, err = p.staticPrograms(config, report, name, source, declaration, byteSize, count, element)
		if err != nil {
			return nil, err
		}
	}
	if len(report.reasons) == 0 && len(plans) == 0 {
		report.no("no program compiles it", "no program compiles %s as a unit", source)
	}
	if len(report.reasons) == 0 {
		for _, plan := range plans[1:] {
			if plan.init != plans[0].init {
				report.no("the programs that compile it need different initial values", "the programs compiling %s need different initializers", source)
				break
			}
		}
	}
	if len(report.reasons) != 0 {
		alternativeSize := byteSize
		if alternativeSize == 0 {
			alternativeSize = staticDeclaredSize(p.root, files, name, headers)
		}
		p.staticAlternatives(config, report, name, alternativeSize, files)
	}
	return &staticAnalysis{report, declaration, plans, files, byteSize}, nil
}

func (p project) staticPrograms(config *projectConfig, report *staticReport, name, source string, declaration *staticDeclaration, size, count, element int) ([]staticPlan, error) {
	var plans []staticPlan
	compiled := false
	for _, m := range config.Modules {
		var unit *unitDef
		for index := range m.Units {
			if m.Units[index].Source == source {
				unit = &m.Units[index]
			}
		}
		perFunction := false
		for index := range m.Functions {
			f := &m.Functions[index]
			if f.Unit == "" && m.sourceOf(f) == source {
				perFunction = true
			}
		}
		if perFunction {
			report.no(fmt.Sprintf("%s is not one unit in %s", source, m.ID), "%s compiles %s function by function", m.ID, source)
			continue
		}
		if unit == nil {
			continue
		}
		compiled = true
		if profile, ok := compilerProfileNamed(staticUnitProfile(m, unit)); ok && profile.smallDataLimit != 0 {
			report.no(fmt.Sprintf("unit %s uses gp-relative small data, where a new static changes the code", unit.Name),
				"%s: unit %s compiles with small data (%s)", m.ID, unit.Name, profile.name)
			continue
		}
		report.info("%s (unit %s):", m.ID, unit.Name)
		addr, ok := staticLabel(m, name)
		if !ok {
			report.no(fmt.Sprintf("%s has no address label for it", m.ID), "  no label in %s", m.path)
			continue
		}
		report.ok("  label at 0x%08x", addr)
		if !m.contains(addr, size) {
			report.no(fmt.Sprintf("it lives outside %s's memory", m.ID), "  0x%08x is outside %s's own memory", addr, m.ID)
			continue
		}
		if owner, row, kind, inside := staticOwnedAt(m, addr, size); inside {
			report.no(fmt.Sprintf("it is a second name for bytes unit %s already owns (see QUIRKS.md Declaration leads)", owner.Name),
				"  0x%08x is inside unit %s's %s row at 0x%08x (%d bytes)", addr, owner.Name, kind, row.Addr, row.Size)
			continue
		}
		image, err := p.staticImage(m)
		if err != nil {
			return nil, fmt.Errorf("%s: %w (run `make extract`)", m.ID, err)
		}
		bytes := image[m.offset(addr) : m.offset(addr)+size]
		zero := true
		for _, value := range bytes {
			if value != 0 {
				zero = false
			}
		}
		report.info("  original bytes: %s", staticHex(bytes))
		dataEnd, haveData := staticEnd(unit.Data)
		bssStart, bssEnd, haveBss := staticSpan(unit.Bss)
		switch {
		case haveData && addr == staticAlignUp(dataEnd, staticAlignment(element, size)):
			init, err := staticInitializer(bytes, declaration.typeText, declaration.dimensions, count, element)
			if err != nil {
				report.no("its original value needs a hand-written initializer", "  continues the data block, but %v", err)
				continue
			}
			report.ok("  continues the unit's data block (ends 0x%08x): data row, initializer `%s`", dataEnd, strings.TrimPrefix(init, " = "))
			plans = append(plans, staticPlan{m, unit, addr, "data", init, shortHash(bytes), staticLabelsInRows(m, unit.Data, name, true), true})
		// bss rows must touch exactly (the config check allows no padding), and
		// this compiler starts every bss variable on a 4-byte boundary.
		case zero && addr%4 == 0 && (!haveBss || addr == bssEnd || addr+uint32(size) == bssStart):
			if haveBss {
				report.ok("  zero, and continues the unit's bss block: bss row")
			} else {
				report.ok("  zero, and the unit has no bss yet: bss row")
			}
			plan := staticPlan{module: m, unit: unit, addr: addr, kind: "bss", hash: shortHash(bytes)}
			if haveBss && addr+uint32(size) != bssStart {
				plan.anchors, plan.after = staticLabelsInRows(m, unit.Bss, name, true), true
			} else if haveBss {
				plan.anchors = staticLabelsInRows(m, unit.Bss, name, false)
			}
			plans = append(plans, plan)
		case zero && addr%4 != 0:
			report.no(fmt.Sprintf("it is at 0x%08x in %s, not 4-aligned, and this compiler 4-aligns every bss variable", addr, m.ID),
				"  zero, but 0x%08x is not 4-aligned, so it cannot be a bss row", addr)
		case !zero && haveData && addr > dataEnd && addr-dataEnd < 8:
			report.no(fmt.Sprintf("%d unknown byte(s) sit between unit %s's data and it in %s", addr-dataEnd, unit.Name, m.ID),
				"  not zero, and %d byte(s) after the data block (ends 0x%08x) that its alignment does not explain", addr-dataEnd, dataEnd)
		case !zero:
			report.no(fmt.Sprintf("its original bytes in %s are not zero and do not follow unit %s's data", m.ID, unit.Name),
				"  not zero, and not right after the unit's data block")
		default:
			report.no(fmt.Sprintf("it is not next to unit %s's other data in %s", unit.Name, m.ID),
				"  zero, but continues neither the unit's data block nor its bss block")
		}
	}
	if !compiled {
		report.no(fmt.Sprintf("%s is not a unit in any program", source), "%s is not a unit in any program", source)
	}
	return plans, nil
}

// staticAlternatives says what a name that cannot be its file's static is instead.
func (p project) staticAlternatives(config *projectConfig, report *staticReport, name string, size int, files []string) {
	if size <= 0 {
		size = 1
	}
	report.info("")
	report.info("what it is instead:")
	type holder struct {
		module *moduleSpec
		addr   uint32
	}
	var holders []holder
	var users []string
	for _, m := range config.Modules {
		if addr, ok := staticLabel(m, name); ok && m.contains(addr, 1) {
			holders = append(holders, holder{m, addr})
			continue
		}
		if staticModuleCompilesAny(m, files) {
			users = append(users, m.ID)
		}
	}
	sort.Strings(users)
	if len(holders) != 0 && len(users) != 0 {
		var names []string
		for _, h := range holders {
			names = append(names, h.module.ID)
		}
		verb := "use"
		if len(users) == 1 {
			verb = "uses"
		}
		report.info("  lives in %s's memory; %s %s that copy through an extern", strings.Join(names, ", "), strings.Join(users, ", "), verb)
	}
	if len(holders) == 0 {
		report.info("  no program's own memory holds it")
	}
	for _, h := range holders {
		m, addr := h.module, h.addr
		if owner, row, _, inside := staticOwnedAt(m, addr, 1); inside {
			// The owner is another label at the same address, else the nearest
			// label before it inside the owned row.
			base, baseAddr := "", row.Addr
			for _, symbol := range m.Data {
				if symbol.Name == name || symbol.Addr < row.Addr || symbol.Addr > addr {
					continue
				}
				if base == "" || symbol.Addr > baseAddr || (symbol.Addr == baseAddr && symbol.Name < base) {
					base, baseAddr = symbol.Name, symbol.Addr
				}
			}
			if base == "" {
				report.info("  %s: already defined in C by unit %s (%s), which owns these bytes; the other files rightly use an extern from a header",
					m.ID, owner.Name, owner.Source)
				continue
			}
			where := base
			if addr != baseAddr {
				where = fmt.Sprintf("%s + %d", base, addr-baseAddr)
			}
			report.info("  %s: a second name for %s, owned by unit %s (%s); rewrite its uses through %s and delete the alias,",
				m.ID, where, owner.Name, owner.Source, base)
			report.info("    or keep it with a comment if make check needs the second name")
			continue
		}
		found := false
		for index := range m.Units {
			unit := &m.Units[index]
			if end, ok := staticEnd(unit.Data); ok && addr >= end && addr-end < 8 {
				report.info("  %s: unit %s (%s) can own it as a data row after its block; define it there%s", m.ID, unit.Name, unit.Source, staticHeaderHint(files))
				found = true
			}
			if start, end, ok := staticSpan(unit.Bss); ok && (addr == end || addr+uint32(size) == start) {
				report.info("  %s: unit %s (%s) can own it as a bss row next to its block; define it there%s", m.ID, unit.Name, unit.Source, staticHeaderHint(files))
				found = true
			}
		}
		if found {
			continue
		}
		var nearest *unitDef
		var nearestEnd uint32
		var nearestKind string
		for index := range m.Units {
			unit := &m.Units[index]
			for _, rows := range []struct {
				kind string
				list []rodataSpec
			}{{"data", unit.Data}, {"bss", unit.Bss}, {"rodata", unit.Rodata}} {
				for _, row := range rows.list {
					end := row.Addr + uint32(row.Size)
					if end <= addr && end > nearestEnd {
						nearest, nearestEnd, nearestKind = unit, end, rows.kind
					}
				}
			}
		}
		if nearest == nil {
			report.info("  %s: no unit owns anything before it", m.ID)
			continue
		}
		report.info("  %s: no unit's block reaches it; the nearest is unit %s (%s), whose %s ends at 0x%08x, %d bytes before it",
			m.ID, nearest.Name, nearest.Source, nearestKind, nearestEnd, addr-nearestEnd)
		var gap []symbolSpec
		for _, symbol := range m.Data {
			if symbol.Addr >= nearestEnd && symbol.Addr < addr {
				gap = append(gap, symbol)
			}
		}
		sort.Slice(gap, func(i, j int) bool { return gap[i].Addr < gap[j].Addr })
		if len(gap) != 0 {
			var labels []string
			for index, symbol := range gap {
				if index == 8 {
					labels = append(labels, "...")
					break
				}
				labels = append(labels, symbol.Name)
			}
			report.info("    %d unowned label(s) sit in between: %s", len(gap), strings.Join(labels, ", "))
			report.info("    those need an owner first (tools/lookup each one); the run may be one missing object's state")
		}
	}
}

// staticModuleCompilesAny reports whether the module compiles any of the files.
func staticModuleCompilesAny(m *moduleSpec, files []string) bool {
	wanted := make(map[string]bool)
	for _, path := range files {
		wanted[path] = true
	}
	for index := range m.Units {
		if wanted[m.Units[index].Source] {
			return true
		}
	}
	for index := range m.Functions {
		if wanted[m.sourceOf(&m.Functions[index])] {
			return true
		}
	}
	return false
}

// staticUnitProfile is the profile a unit compiles with: its own, or its
// members' (the first member's when they agree, as the build requires).
func staticUnitProfile(m *moduleSpec, unit *unitDef) string {
	if unit.Profile != "" {
		return unit.Profile
	}
	for index := range m.Functions {
		if m.Functions[index].Unit == unit.Name {
			return m.profileOf(&m.Functions[index])
		}
	}
	return m.Profile
}

// staticAlignment is the alignment GCC gives a variable: its element size
// for numbers and pointers, and for a record the largest power of two up to
// 4 that divides its size.
func staticAlignment(element, size int) int {
	unit := element
	if unit <= 0 {
		unit = size
	}
	for _, alignment := range []int{4, 2} {
		if unit%alignment == 0 {
			return alignment
		}
	}
	return 1
}

func staticAlignUp(addr uint32, alignment int) uint32 {
	mask := uint32(alignment - 1)
	return (addr + mask) &^ mask
}

// staticLabelsInRows lists the labels inside rows, highest address first
// (descending) or lowest first.
func staticLabelsInRows(m *moduleSpec, rows []rodataSpec, except string, descending bool) []string {
	var found []symbolSpec
	for _, symbol := range m.Data {
		if symbol.Name == except {
			continue
		}
		for _, row := range rows {
			if symbol.Addr >= row.Addr && symbol.Addr < row.Addr+uint32(row.Size) {
				found = append(found, symbol)
				break
			}
		}
	}
	sort.Slice(found, func(i, j int) bool {
		if descending {
			return found[i].Addr > found[j].Addr
		}
		return found[i].Addr < found[j].Addr
	})
	var names []string
	for _, symbol := range found {
		names = append(names, symbol.Name)
	}
	return names
}

// staticDefinitionEdge finds the file-scope definition of name in text and
// returns the offset just after its statement's line (after) or the start of
// its first line, including a comment block directly above it (before).
func staticDefinitionEdge(text, name string, after bool) (int, bool) {
	pattern := regexp.MustCompile(`(?m)^[A-Za-z_][^;(\n]*\b` + regexp.QuoteMeta(name) + `\b\s*(?:\[[^\]\n]*\]\s*)*(?:=|;)`)
	for _, match := range pattern.FindAllStringIndex(text, -1) {
		line := text[match[0]:match[1]]
		if strings.HasPrefix(line, "extern") || strings.HasPrefix(line, "typedef") {
			continue
		}
		if !after {
			start := match[0]
			for {
				previous := staticPreviousLine(text, start)
				trimmed := strings.TrimSpace(previous.text)
				if previous.text == "" || !(strings.HasPrefix(trimmed, "/*") || strings.HasPrefix(trimmed, "*")) {
					break
				}
				start = previous.start
			}
			return start, true
		}
		depth := 0
		for index := match[0]; index < len(text); index++ {
			switch text[index] {
			case '{':
				depth++
			case '}':
				depth--
			case ';':
				if depth == 0 {
					if newline := strings.IndexByte(text[index:], '\n'); newline >= 0 {
						return index + newline + 1, true
					}
					return len(text), true
				}
			}
		}
	}
	return 0, false
}

func staticHeaderHint(files []string) string {
	if len(files) > 1 {
		return ", and declare it extern in a header for the other users"
	}
	return ""
}

func (p project) staticApply(config *projectConfig, declaration *staticDeclaration, newLine string, plans []staticPlan, size int) error {
	return p.withTargetLock(func() error {
		changed := map[string]bool{declaration.path: true}
		for _, plan := range plans {
			changed[plan.module.path] = true
		}
		backup := filepath.Join(p.root, "build", "static-backup")
		if !staticBatching {
			if err := os.RemoveAll(backup); err != nil {
				return err
			}
		}
		for path := range changed {
			target := filepath.Join(backup, path)
			if _, err := os.Stat(target); err == nil {
				continue // a batch keeps each file as it was before the batch
			}
			data, err := os.ReadFile(filepath.Join(p.root, path))
			if err != nil {
				return err
			}
			if err := os.MkdirAll(filepath.Dir(target), 0o755); err != nil {
				return err
			}
			if err := os.WriteFile(target, data, 0o644); err != nil {
				return err
			}
		}
		for _, plan := range plans {
			row := rodataSpec{Addr: plan.addr, Size: size, Hash: plan.hash}
			if plan.kind == "data" {
				plan.unit.Data = append(plan.unit.Data, row)
				sort.Slice(plan.unit.Data, func(i, j int) bool { return plan.unit.Data[i].Addr < plan.unit.Data[j].Addr })
			} else {
				plan.unit.Bss = append(plan.unit.Bss, row)
				sort.Slice(plan.unit.Bss, func(i, j int) bool { return plan.unit.Bss[i].Addr < plan.unit.Bss[j].Addr })
			}
		}
		transaction := newSymbolFileTransaction()
		for path, data := range renderConfigFiles(config) {
			full := filepath.Join(p.root, filepath.FromSlash(path))
			info, original, err := readRegularSymbolFile(full)
			if err != nil {
				return err
			}
			if err := transaction.addExistingSnapshot(full, original, data, info.Mode()); err != nil {
				return err
			}
		}
		text := declaration.text
		start := declaration.start
		if previous := staticPreviousLine(text, start); strings.HasPrefix(strings.TrimSpace(previous.text), staticNote) {
			start = previous.start
		}
		end := declaration.end
		if end < len(text) && text[end] == '\n' {
			end++
		}
		updated := text[:start] + newLine + "\n" + text[end:]
		// Remove the extern, then define the static beside the nearest
		// neighbor this file defines (an alias label has no definition).
		removed := text[:start] + text[end:]
		for _, anchor := range plans[0].anchors {
			if at, ok := staticDefinitionEdge(removed, anchor, plans[0].after); ok {
				updated = removed[:at] + newLine + "\n" + removed[at:]
				break
			}
		}
		full := filepath.Join(p.root, declaration.path)
		info, original, err := readRegularSymbolFile(full)
		if err != nil {
			return err
		}
		if err := transaction.addExistingSnapshot(full, original, []byte(updated), info.Mode()); err != nil {
			return err
		}
		if err := transaction.commit(); err != nil {
			return err
		}
		var modules []string
		for _, plan := range plans {
			modules = append(modules, plan.module.ID)
		}
		fmt.Println("static-applied: " + strings.TrimSpace(newLine))
		fmt.Println("static-source: " + declaration.path)
		fmt.Println("static-modules: " + strings.Join(modules, " "))
		return nil
	})
}

func (p project) staticRestore(name string, failed bool) error {
	backup := filepath.Join(p.root, "build", "static-backup")
	var restored []string
	err := filepath.WalkDir(backup, func(path string, entry os.DirEntry, walkErr error) error {
		if walkErr != nil || entry.IsDir() {
			return walkErr
		}
		relative, err := filepath.Rel(backup, path)
		if err != nil {
			return err
		}
		data, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		restored = append(restored, relative)
		return os.WriteFile(filepath.Join(p.root, relative), data, 0o644)
	})
	if err != nil {
		return fmt.Errorf("no backup to restore: %w", err)
	}
	if err := os.RemoveAll(backup); err != nil {
		return err
	}
	sort.Strings(restored)
	fmt.Println("static: restored " + strings.Join(restored, ", "))
	if !failed {
		return nil
	}
	for _, path := range restored {
		if !strings.HasSuffix(path, ".c") {
			continue
		}
		declarations, err := staticExternDeclarations(p.root, []string{path}, name)
		if err != nil || len(declarations) != 1 {
			return err
		}
		if added, err := staticAddNote(p.root, &declarations[0], []string{"as a static it no longer matches the original bytes (make check)"}); err != nil {
			return err
		} else if added {
			fmt.Println("static-note: " + path)
		}
	}
	return nil
}

func staticAddNote(root string, declaration *staticDeclaration, reasons []string) (bool, error) {
	text := declaration.text
	if previous := staticPreviousLine(text, declaration.start); strings.HasPrefix(strings.TrimSpace(previous.text), staticNote) {
		return false, nil
	}
	note := staticNote + strings.Join(reasons, "; ") + ". */\n"
	full := filepath.Join(root, declaration.path)
	info, err := os.Stat(full)
	if err != nil {
		return false, err
	}
	return true, os.WriteFile(full, []byte(text[:declaration.start]+note+text[declaration.start:]), info.Mode())
}

type staticLine struct {
	start int
	text  string
}

func staticPreviousLine(text string, start int) staticLine {
	if start == 0 {
		return staticLine{}
	}
	end := start - 1
	begin := strings.LastIndex(text[:end], "\n") + 1
	return staticLine{begin, text[begin:end]}
}

// The survey checks every name in one run, so it fills these caches once.
var (
	staticBatching      bool // --batch: keep the first backup of every file across applies
	staticSpellingIndex map[string][]string
	staticHeaderCache   *string
	staticImageCache    = map[string][]byte{}
)

func (p project) staticImage(m *moduleSpec) ([]byte, error) {
	if image, ok := staticImageCache[m.ID]; ok {
		return image, nil
	}
	image, err := p.readModuleImage(m)
	if err == nil {
		staticImageCache[m.ID] = image
	}
	return image, err
}

func staticFilesSpelling(root, name string) ([]string, error) {
	if staticSpellingIndex != nil {
		return staticSpellingIndex[name], nil
	}
	pattern := regexp.MustCompile(`\b` + regexp.QuoteMeta(name) + `\b`)
	var found []string
	for _, directory := range []string{"src", "include"} {
		err := filepath.WalkDir(filepath.Join(root, directory), func(path string, entry os.DirEntry, walkErr error) error {
			if walkErr != nil || entry.IsDir() || !sourceExtensions[filepath.Ext(path)] {
				return walkErr
			}
			data, err := os.ReadFile(path)
			if err != nil {
				return err
			}
			if pattern.Match(data) {
				relative, err := filepath.Rel(root, path)
				if err != nil {
					return err
				}
				found = append(found, filepath.ToSlash(relative))
			}
			return nil
		})
		if err != nil {
			return nil, err
		}
	}
	sort.Strings(found)
	return found, nil
}

func staticHeaderText(root string) (string, error) {
	if staticHeaderCache != nil {
		return *staticHeaderCache, nil
	}
	var builder strings.Builder
	for _, directory := range []string{"include/fft", "include/psx"} {
		entries, err := os.ReadDir(filepath.Join(root, directory))
		if err != nil {
			continue
		}
		for _, entry := range entries {
			if strings.HasSuffix(entry.Name(), ".h") {
				data, err := os.ReadFile(filepath.Join(root, directory, entry.Name()))
				if err != nil {
					return "", err
				}
				builder.Write(data)
				builder.WriteByte('\n')
			}
		}
	}
	text := builder.String()
	staticHeaderCache = &text
	return text, nil
}

func staticExternPattern(name string) *regexp.Regexp {
	return regexp.MustCompile(`(?m)^extern\s+([^;=()\n]*?)\b` + regexp.QuoteMeta(name) + `\s*((?:\[[^\]\n]*\]\s*)*);(.*)$`)
}

func staticExternDeclarations(root string, sources []string, name string) ([]staticDeclaration, error) {
	pattern := staticExternPattern(name)
	var found []staticDeclaration
	for _, path := range sources {
		data, err := os.ReadFile(filepath.Join(root, path))
		if err != nil {
			return nil, err
		}
		text := string(data)
		for _, match := range pattern.FindAllStringSubmatchIndex(text, -1) {
			found = append(found, staticDeclaration{
				path: path, text: text, start: match[0], end: match[1],
				typeText:   strings.TrimSpace(text[match[2]:match[3]]),
				dimensions: text[match[4]:match[5]],
				trailing:   text[match[6]:match[7]],
			})
		}
	}
	return found, nil
}

// staticDeclaredSize reads the name's extern declaration in any file, for the
// alternatives report of a name several files use.
func staticDeclaredSize(root string, files []string, name, headers string) int {
	pattern := staticExternPattern(name)
	for _, path := range files {
		data, err := os.ReadFile(filepath.Join(root, path))
		if err != nil {
			continue
		}
		if match := pattern.FindStringSubmatch(string(data)); match != nil {
			count, element := staticTypeSize(strings.TrimSpace(match[1]), match[2], headers, string(data))
			if count > 0 && element > 0 {
				return count * element
			}
		}
	}
	return 0
}

// staticTypeSize returns the element count and element size of a declaration,
// or 0 for whichever cannot be worked out.
func staticTypeSize(typeText, dimensions, headers, source string) (int, int) {
	count := 1
	for _, match := range regexp.MustCompile(`\[([^\]]*)\]`).FindAllStringSubmatch(dimensions, -1) {
		value, ok := staticEvaluate(strings.TrimSpace(match[1]), headers, source)
		if !ok {
			count = 0
			break
		}
		count *= value
	}
	return count, staticElementSize(typeText, headers)
}

func staticElementSize(typeText, headers string) int {
	if strings.Contains(typeText, "*") {
		return 4
	}
	var words []string
	for _, word := range strings.Fields(typeText) {
		switch word {
		case "const", "volatile", "struct", "unsigned", "signed":
		default:
			words = append(words, word)
		}
	}
	if len(words) == 0 {
		return 4
	}
	base := words[len(words)-1]
	if size, ok := staticScalarSizes[base]; ok {
		return size
	}
	stem := strings.TrimSuffix(base, "_t")
	if match := regexp.MustCompile(`\b` + regexp.QuoteMeta(stem) + `_size_must_be_(\d+)\b`).FindStringSubmatch(headers); match != nil {
		size, _ := strconv.Atoi(match[1])
		return size
	}
	if strings.HasSuffix(base, "_e") {
		return 4
	}
	return 0
}

// staticEvaluate evaluates an array dimension: numbers, named constants and + - * / ( ).
func staticEvaluate(expression, headers, source string) (int, bool) {
	tokens := regexp.MustCompile(`[A-Za-z_][A-Za-z0-9_]*|0[xX][0-9a-fA-F]+|\d+|[-+*/()]`).FindAllString(expression, -1)
	if strings.Join(tokens, "") != strings.ReplaceAll(expression, " ", "") || len(tokens) == 0 {
		return 0, false
	}
	position := 0
	var sum, product, atom func() (int, bool)
	atom = func() (int, bool) {
		if position >= len(tokens) {
			return 0, false
		}
		token := tokens[position]
		position++
		if token == "(" {
			value, ok := sum()
			if !ok || position >= len(tokens) || tokens[position] != ")" {
				return 0, false
			}
			position++
			return value, true
		}
		if value, err := strconv.ParseInt(token, 0, 64); err == nil {
			return int(value), true
		}
		for _, text := range []string{source, headers} {
			if match := regexp.MustCompile(`\b` + regexp.QuoteMeta(token) + `\s*=\s*(0[xX][0-9a-fA-F]+|\d+)\b`).FindStringSubmatch(text); match != nil {
				value, _ := strconv.ParseInt(match[1], 0, 64)
				return int(value), true
			}
		}
		return 0, false
	}
	product = func() (int, bool) {
		value, ok := atom()
		for ok && position < len(tokens) && (tokens[position] == "*" || tokens[position] == "/") {
			operator := tokens[position]
			position++
			var right int
			right, ok = atom()
			if ok && operator == "*" {
				value *= right
			} else if ok && right != 0 {
				value /= right
			} else {
				ok = false
			}
		}
		return value, ok
	}
	sum = func() (int, bool) {
		value, ok := product()
		for ok && position < len(tokens) && (tokens[position] == "+" || tokens[position] == "-") {
			operator := tokens[position]
			position++
			var right int
			right, ok = product()
			if operator == "+" {
				value += right
			} else {
				value -= right
			}
		}
		return value, ok
	}
	value, ok := sum()
	return value, ok && position == len(tokens)
}

func staticInitializer(bytes []byte, typeText, dimensions string, count, element int) (string, error) {
	zero := true
	for _, value := range bytes {
		if value != 0 {
			zero = false
		}
	}
	array := strings.Contains(dimensions, "[")
	words := strings.Fields(typeText)
	base := ""
	if len(words) != 0 {
		base = words[len(words)-1]
	}
	scalarSize, scalar := staticScalarSizes[base]
	scalar = scalar && scalarSize <= 4 && !strings.Contains(typeText, "*")
	if zero {
		if scalar && !array {
			return " = 0", nil
		}
		return " = { 0 }", nil
	}
	if !scalar || element <= 0 || count <= 0 {
		return "", fmt.Errorf("the original bytes are not zero and the type is not a plain number; write the initializer by hand")
	}
	var values []string
	for index := 0; index < count; index++ {
		var value int64
		for offset := element - 1; offset >= 0; offset-- {
			value = value<<8 | int64(bytes[index*element+offset])
		}
		if staticSigned[base] && value >= 1<<(uint(element)*8-1) {
			value -= 1 << (uint(element) * 8)
		}
		if value > 9 {
			values = append(values, fmt.Sprintf("0x%x", value))
		} else {
			values = append(values, strconv.FormatInt(value, 10))
		}
	}
	if !array {
		return " = " + values[0], nil
	}
	return " = { " + strings.Join(values, ", ") + " }", nil
}

func staticLabel(m *moduleSpec, name string) (uint32, bool) {
	for _, symbol := range append(append([]symbolSpec(nil), m.Data...), m.Imports...) {
		if symbol.Name == name {
			return symbol.Addr, true
		}
	}
	return 0, false
}

func staticOwnedAt(m *moduleSpec, addr uint32, size int) (*unitDef, rodataSpec, string, bool) {
	for index := range m.Units {
		unit := &m.Units[index]
		for _, rows := range []struct {
			kind string
			list []rodataSpec
		}{{"data", unit.Data}, {"bss", unit.Bss}, {"rodata", unit.Rodata}} {
			for _, row := range rows.list {
				if row.Addr < addr+uint32(size) && addr < row.Addr+uint32(row.Size) {
					return unit, row, rows.kind, true
				}
			}
		}
	}
	return nil, rodataSpec{}, "", false
}

func staticEnd(rows []rodataSpec) (uint32, bool) {
	_, end, ok := staticSpan(rows)
	return end, ok
}

func staticSpan(rows []rodataSpec) (uint32, uint32, bool) {
	if len(rows) == 0 {
		return 0, 0, false
	}
	start, end := rows[0].Addr, rows[0].Addr+uint32(rows[0].Size)
	for _, row := range rows[1:] {
		if row.Addr < start {
			start = row.Addr
		}
		if row.Addr+uint32(row.Size) > end {
			end = row.Addr + uint32(row.Size)
		}
	}
	return start, end, true
}

func staticHex(bytes []byte) string {
	var parts []string
	for index, value := range bytes {
		if index == 16 {
			parts = append(parts, "...")
			break
		}
		parts = append(parts, fmt.Sprintf("%02x", value))
	}
	return strings.Join(parts, " ")
}

// staticSurvey runs every check on every file-scope extern in a game .c file
// and writes build/static-survey.yaml. It changes no source or target file.
// A name that becomes possible only after a neighbor is converted (its block
// then reaches it) is counted in a later round.
func (p project) staticSurvey(apply bool) error {
	config, err := p.loadProjectConfig()
	if err != nil {
		return err
	}
	if apply {
		if err := os.RemoveAll(filepath.Join(p.root, "build", "static-backup")); err != nil {
			return err
		}
		staticBatching = true
		defer func() { staticBatching = false }()
	}
	index := make(map[string][]string)
	externs := make(map[string]bool)
	identifier := regexp.MustCompile(`[A-Za-z_][A-Za-z0-9_]*`)
	declaration := regexp.MustCompile(`(?m)^extern\s+[^;=()\n]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]\n]*\]\s*)*;`)
	for _, directory := range []string{"src", "include"} {
		err := filepath.WalkDir(filepath.Join(p.root, directory), func(path string, entry os.DirEntry, walkErr error) error {
			if walkErr != nil || entry.IsDir() || !sourceExtensions[filepath.Ext(path)] {
				return walkErr
			}
			data, err := os.ReadFile(path)
			if err != nil {
				return err
			}
			relative, err := filepath.Rel(p.root, path)
			if err != nil {
				return err
			}
			relative = filepath.ToSlash(relative)
			seen := make(map[string]bool)
			for _, token := range identifier.FindAllString(string(data), -1) {
				if !seen[token] {
					seen[token] = true
					index[token] = append(index[token], relative)
				}
			}
			if strings.HasSuffix(path, ".c") && !strings.HasPrefix(relative, "src/platform/") {
				for _, match := range declaration.FindAllStringSubmatch(string(data), -1) {
					externs[match[1]] = true
				}
			}
			return nil
		})
		if err != nil {
			return err
		}
	}
	for name := range index {
		sort.Strings(index[name])
	}
	staticSpellingIndex = index
	defer func() { staticSpellingIndex = nil }()

	var names []string
	for name := range externs {
		names = append(names, name)
	}
	sort.Strings(names)
	type outcome struct {
		name, category, reason, source, kind string
		round                                int
	}
	results := make(map[string]*outcome)
	pending := names
	for round := 1; len(pending) != 0; round++ {
		var next []string
		converted := 0
		for _, name := range pending {
			analysis, err := p.staticAnalyze(config, name, 0)
			if err != nil {
				results[name] = &outcome{name: name, category: "error", reason: err.Error()}
				continue
			}
			if len(analysis.report.reasons) == 0 {
				result := &outcome{name: name, category: "can be static", source: analysis.declaration.path, kind: analysis.plans[0].kind, round: round}
				results[name] = result
				converted++
				if apply {
					declaration := analysis.declaration
					newLine := fmt.Sprintf("static %s %s%s%s;%s", declaration.typeText, name, strings.TrimRight(declaration.dimensions, " "),
						analysis.plans[0].init, declaration.trailing)
					// staticApply records the row in the unit, so later rounds see it.
					if err := p.staticApply(config, declaration, newLine, analysis.plans, analysis.size); err != nil {
						return fmt.Errorf("%s: %w", name, err)
					}
					continue
				}
				// Later rounds see the unit's block grown by this row.
				for _, plan := range analysis.plans {
					row := rodataSpec{Addr: plan.addr, Size: analysis.size, Hash: plan.hash}
					if plan.kind == "data" {
						plan.unit.Data = append(plan.unit.Data, row)
					} else {
						plan.unit.Bss = append(plan.unit.Bss, row)
					}
				}
				continue
			}
			reason := analysis.report.reasons[0]
			results[name] = &outcome{name: name, category: staticCategory(reason), reason: reason}
			if strings.Contains(reason, "not next to") {
				next = append(next, name)
			}
		}
		if converted == 0 {
			break
		}
		pending = next
	}

	counts := make(map[string]int)
	rounds := make(map[int]int)
	for _, result := range results {
		counts[result.category]++
		if result.category == "can be static" {
			rounds[result.round]++
		}
	}
	var categories []string
	for category := range counts {
		categories = append(categories, category)
	}
	sort.Slice(categories, func(i, j int) bool {
		if counts[categories[i]] != counts[categories[j]] {
			return counts[categories[i]] > counts[categories[j]]
		}
		return categories[i] < categories[j]
	})
	var out strings.Builder
	fmt.Fprintf(&out, "# tools static --survey: every file-scope extern in a game .c file, checked against the current tree.\n")
	fmt.Fprintf(&out, "# Nothing was changed. Round 2+ names become possible once round 1 names are converted.\nexterns: %d\nsummary:\n", len(names))
	for _, category := range categories {
		fmt.Fprintf(&out, "  %q: %d\n", category, counts[category])
	}
	fmt.Println(fmt.Sprintf("static: surveyed %d file-scope externs in game sources", len(names)))
	for _, category := range categories {
		fmt.Printf("  %5d  %s\n", counts[category], category)
	}
	var roundList []int
	for round := range rounds {
		roundList = append(roundList, round)
	}
	sort.Ints(roundList)
	for _, round := range roundList {
		fmt.Printf("         round %d: %d can be static\n", round, rounds[round])
	}
	for _, category := range categories {
		fmt.Fprintf(&out, "%s:\n", strings.ReplaceAll(category, " ", "_"))
		for _, name := range names {
			result := results[name]
			if result.category != category {
				continue
			}
			if category == "can be static" {
				fmt.Fprintf(&out, "  - {name: %s, source: %s, row: %s, round: %d}\n", name, result.source, result.kind, result.round)
			} else {
				fmt.Fprintf(&out, "  - {name: %s, why: %q}\n", name, result.reason)
			}
		}
	}
	path := filepath.Join(p.root, "build", "static-survey.yaml")
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return err
	}
	if err := os.WriteFile(path, []byte(out.String()), 0o644); err != nil {
		return err
	}
	fmt.Println("static: details in build/static-survey.yaml")
	return nil
}

func staticCategory(reason string) string {
	switch {
	case strings.Contains(reason, "files use it"), strings.Contains(reason, "declared in a header"), strings.Contains(reason, "several .c files"):
		return "used by more than one file"
	case strings.Contains(reason, "second name"):
		return "alias of bytes another variable owns"
	case strings.Contains(reason, "outside"):
		return "lives in another program's memory"
	case strings.Contains(reason, "not next to"):
		return "zero, but no unit block reaches it"
	case strings.Contains(reason, "not 4-aligned"):
		return "zero, but not 4-aligned for a bss row"
	case strings.Contains(reason, "small data"):
		return "unit uses gp-relative small data"
	case strings.Contains(reason, "unknown byte"):
		return "unknown bytes between it and the unit's data"
	case strings.Contains(reason, "not zero"):
		return "nonzero, and not after the unit's data"
	case strings.Contains(reason, "not one unit"), strings.Contains(reason, "not a unit"), strings.Contains(reason, "no program compiles"):
		return "file is not compiled as one unit"
	case strings.Contains(reason, "size is unknown"):
		return "size unknown"
	case strings.Contains(reason, "no address label"):
		return "no address label"
	case strings.Contains(reason, "initializer"), strings.Contains(reason, "initial values"):
		return "needs a hand-written initializer"
	}
	return "other"
}
