package main

import (
	"errors"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
)

// unitMemberBody is one member's text as one module compiles it: the
// per-function file, or its piece of an existing unit file.
type unitMemberBody struct {
	key      string // identity of the text: the file, or unit file + member
	includes []string
	text     string
}

// unitSymbolsCommand merges the listed entries' sources into one unit source
// and makes the entries members of a unit of that name in each of their
// modules. Members may be per-function files or members of existing units,
// which the new unit absorbs whole. The members of one module must follow
// one another in address order and the modules' orders must agree. A member
// that only some of the unit's modules compile, or whose body differs per
// module, is wrapped in the program macros of the modules that compile that
// body, so each module still defines exactly its own members in order.
func (p project) unitSymbolsCommand(args []string) error {
	flags := flag.NewFlagSet("symbols unit", flag.ContinueOnError)
	name := flags.String("name", "", "unit name")
	source := flags.String("source", "", "merged source path under src/, such as src/event/thread.c")
	comment := flags.String("comment", "", "leading file comment: a brief description of what the file holds")
	ownRodata := flags.Bool("own-rodata", false, "define the read-only tables that lie among the members' literals inside the unit (one module only)")
	dryRun := flags.Bool("dry-run", false, "validate and report without writing")
	if err := flags.Parse(args); err != nil {
		return err
	}
	usage := errors.New("usage: tools symbols unit --name NAME --source src/DIR/NAME.c [--comment TEXT] [--dry-run] MODULE:ENTRY...")
	if *name == "" || *source == "" || flags.NArg() == 0 {
		return usage
	}
	if err := validateRenameIdentifier(*name); err != nil {
		return err
	}
	clean := filepath.ToSlash(filepath.Clean(*source))
	if !strings.HasPrefix(clean, "src/") || filepath.Ext(clean) != ".c" {
		return fmt.Errorf("--source must name a .c file under src/: %q", *source)
	}
	var result string
	err := p.withTargetLock(func() error {
		config, err := p.loadProjectConfig()
		if err != nil {
			return err
		}
		byModule := map[*moduleSpec][]*functionSpec{}
		var order []*moduleSpec
		listed := map[string]bool{}
		for _, text := range flags.Args() {
			moduleID, entry, ok := strings.Cut(text, ":")
			if !ok {
				return fmt.Errorf("%q is not MODULE:ENTRY", text)
			}
			m, err := resolveModuleName(config, moduleID)
			if err != nil {
				return err
			}
			f := m.function(entry)
			if f == nil {
				return fmt.Errorf("no function %s in %s", entry, m.ID)
			}
			if f.Asm != "" {
				return fmt.Errorf("%s:%s must be a C definition, not assembly", m.ID, entry)
			}
			if listed[m.ID+":"+f.Name] {
				return fmt.Errorf("%s:%s is listed twice", m.ID, entry)
			}
			listed[m.ID+":"+f.Name] = true
			if _, seen := byModule[m]; !seen {
				order = append(order, m)
			}
			byModule[m] = append(byModule[m], f)
		}
		// Absorbed units must come whole, and old unit files must be plain
		// (no conditionals) so their pieces can be cut per member.
		absorbed := map[string]*unitDef{} // "module:unit" -> def
		unitTexts := map[string]unitPieces{}
		for _, m := range order {
			if existing := m.unit(*name); existing != nil {
				// The name may be reused when that unit is absorbed whole.
				for _, member := range m.unitMembers(*name) {
					if !listed[m.ID+":"+member.Name] {
						return fmt.Errorf("%s already has a unit %s", m.ID, *name)
					}
				}
			}
			for _, f := range byModule[m] {
				if f.Unit == "" {
					continue
				}
				for _, member := range m.unitMembers(f.Unit) {
					if !listed[m.ID+":"+member.Name] {
						return fmt.Errorf("%s:%s is a member of unit %s; list all of that unit's members or none", m.ID, f.Name, f.Unit)
					}
				}
				absorbed[m.ID+":"+f.Unit] = m.unit(f.Unit)
				unitSource := m.unit(f.Unit).Source
				if _, done := unitTexts[unitSource]; done {
					continue
				}
				pieces, err := readUnitPieces(p.root, m, f.Unit)
				if err != nil {
					return err
				}
				unitTexts[unitSource] = pieces
			}
		}
		// The path may be reused when it belongs to a unit absorbed whole.
		if _, err := os.Stat(filepath.Join(p.root, filepath.FromSlash(clean))); err == nil {
			if _, absorbedPath := unitTexts[clean]; !absorbedPath {
				return fmt.Errorf("%s already exists", clean)
			}
		}
		// Keep the original units' section contracts in source order. Their
		// declarations remain in the member pieces; forgetting the ranges here
		// would leave initialized storage without its address and hash contract.
		mergedUnits := map[*moduleSpec]unitDef{}
		for _, m := range order {
			list := byModule[m]
			sort.Slice(list, func(i, j int) bool { return list[i].Addr < list[j].Addr })
			merged := unitDef{Name: *name, Source: clean}
			seen := map[string]bool{}
			for _, f := range list {
				if f.Unit == "" || seen[f.Unit] {
					continue
				}
				seen[f.Unit] = true
				original := m.unit(f.Unit)
				merged.Data = append(merged.Data, original.Data...)
				merged.Rodata = append(merged.Rodata, original.Rodata...)
			}
			for i := 1; i < len(merged.Data); i++ {
				previous := merged.Data[i-1]
				end := previous.Addr + uint32(previous.Size)
				if merged.Data[i].Addr < end || merged.Data[i].Addr-end >= 8 {
					return fmt.Errorf("%s:%s: data ranges are not contiguous in source order at 0x%08x", m.ID, *name, merged.Data[i].Addr)
				}
			}
			mergedUnits[m] = merged
		}
		// Tables the unit will define sit among the members' literals; they
		// must be known before the read-only ranges are checked for contiguity.
		var owned []rodataSpec
		var fillers map[string]string // member name -> filler text emitted after that member
		if *ownRodata {
			if len(order) != 1 {
				return errors.New("--own-rodata applies to a unit of one module")
			}
			image, err := p.readModuleImage(order[0])
			if err != nil {
				return fmt.Errorf("--own-rodata needs the module image: %w", err)
			}
			list := byModule[order[0]]
			sort.Slice(list, func(i, j int) bool { return list[i].Addr < list[j].Addr })
			owned, fillers, err = ownedRodataTables(image, order[0], list, mergedUnits[order[0]].Rodata)
			if err != nil {
				return err
			}
		}
		// Per-module member lists in address order, checked for contiguity,
		// profile agreement and jump-table alignment.
		members := map[*moduleSpec][]string{}
		for _, m := range order {
			list := byModule[m]
			sort.Slice(list, func(i, j int) bool { return list[i].Addr < list[j].Addr })
			var functions []string
			for _, f := range list {
				functions = append(functions, f.Name)
			}
			merged := mergedUnits[m]
			merged.Rodata = append(merged.Rodata, owned...)
			sort.Slice(merged.Rodata, func(i, j int) bool { return merged.Rodata[i].Addr < merged.Rodata[j].Addr })
			spec := &unitSpec{name: *name, source: clean, functions: functions, rodata: merged.Rodata}
			instance, err := resolveUnitInstance(config, spec, unitInstanceYAML{Module: m.ID})
			if err != nil {
				return err
			}
			profile := m.profileOf(list[0])
			for _, f := range list[1:] {
				if current := m.profileOf(f); current != profile {
					return fmt.Errorf("%s:%s: original profile disagreement: %s uses %s, %s uses %s", m.ID, *name, list[0].Name, profile, f.Name, current)
				}
			}
			// Resolve before removing any absorbed units: their profiles can be
			// the only place the historical compiler flags are recorded.
			if instance.profile.name != m.Profile {
				merged.Profile = instance.profile.name
			}
			mergedUnits[m] = merged
			if image, err := p.readModuleImage(m); err == nil {
				if conflict := unitJumpTableConflict(image, m, list); conflict != "" {
					return fmt.Errorf("%s: %s", m.ID, conflict)
				}
			}
			members[m] = functions
		}
		var lists [][]string
		for _, m := range order {
			lists = append(lists, members[m])
		}
		merged, err := mergeMemberOrders(lists)
		if err != nil {
			return err
		}
		// Bodies per member per module, keyed by text identity.
		bodies := map[string]map[string]*unitMemberBody{} // function -> body key -> body
		bodyModules := map[string]map[string][]*moduleSpec{}
		memberFiles := map[string]bool{}
		for _, m := range order {
			for _, f := range byModule[m] {
				body, err := memberBody(p.root, m, f, unitTexts)
				if err != nil {
					return err
				}
				// An alias entry takes a copy of its target's body; the
				// target's own file stays with the target unless it joins too.
				if f.Unit == "" && f.SourceSymbol == "" {
					memberFiles[m.sourceOf(f)] = true
				}
				if bodies[f.Name] == nil {
					bodies[f.Name] = map[string]*unitMemberBody{}
					bodyModules[f.Name] = map[string][]*moduleSpec{}
				}
				bodies[f.Name][body.key] = body
				bodyModules[f.Name][body.key] = append(bodyModules[f.Name][body.key], m)
			}
		}
		for file := range memberFiles {
			if file == clean {
				return fmt.Errorf("%s is already a member source; choose a new path for the unit", clean)
			}
		}
		// Every entry compiled from a member file must join the unit: a file
		// left behind would be a second body of the same function.
		for _, m := range config.Modules {
			for index := range m.Functions {
				f := &m.Functions[index]
				if f.Unit == "" && memberFiles[m.sourceOf(f)] && !listed[m.ID+":"+f.Name] {
					return fmt.Errorf("%s:%s also compiles %s; list it in the unit or leave that function out", m.ID, f.Name, m.sourceOf(f))
				}
			}
		}
		text, conditional := renderUnitSource(order, merged, bodies, bodyModules, fillers)
		if *comment != "" {
			if strings.Contains(*comment, "*/") {
				return errors.New("--comment must not contain */")
			}
			text = append([]byte("/* "+strings.TrimSpace(*comment)+" */\n\n"), text...)
		}
		for _, m := range order {
			if err := validateUnitSourceDataFor(m, clean, text, members[m]); err != nil {
				return err
			}
		}
		for _, m := range order {
			var kept []unitDef
			for _, unit := range m.Units {
				if absorbed[m.ID+":"+unit.Name] == nil {
					kept = append(kept, unit)
				}
			}
			m.Units = append(kept, mergedUnits[m])
			aliases := map[string]bool{}
			for _, f := range byModule[m] {
				if f.SourceSymbol != "" {
					aliases[f.Name] = true
				}
				f.Unit = *name
				f.Source = ""
				f.SourceSymbol = ""
			}
			// A materialized duplicate names its symbols directly; the
			// override rows that rebound them for the alias entry are spent.
			if len(aliases) > 0 {
				var overrides []overrideSpec
				for _, override := range m.Overrides {
					if !aliases[override.Function] {
						overrides = append(overrides, override)
					}
				}
				m.Overrides = overrides
			}
		}
		stillUsed := map[string]bool{}
		for _, m := range config.Modules {
			for index := range m.Functions {
				stillUsed[m.sourceOf(&m.Functions[index])] = true
			}
		}
		transaction := newSymbolFileTransaction()
		if err := os.MkdirAll(filepath.Dir(filepath.Join(p.root, filepath.FromSlash(clean))), 0o755); err != nil {
			return err
		}
		if _, reused := unitTexts[clean]; reused {
			full := filepath.Join(p.root, filepath.FromSlash(clean))
			info, original, err := readRegularSymbolFile(full)
			if err != nil {
				return err
			}
			if err := transaction.addExistingSnapshot(full, original, text, info.Mode()); err != nil {
				return err
			}
		} else if err := transaction.addCreation(filepath.Join(p.root, filepath.FromSlash(clean)), text, 0o644); err != nil {
			return err
		}
		var oldFiles []string
		for file := range memberFiles {
			oldFiles = append(oldFiles, file)
		}
		for file := range unitTexts {
			if file != clean {
				oldFiles = append(oldFiles, file)
			}
		}
		sort.Strings(oldFiles)
		removed := 0
		for _, old := range oldFiles {
			if stillUsed[old] {
				continue
			}
			path := filepath.Join(p.root, filepath.FromSlash(old))
			info, data, err := readRegularSymbolFile(path)
			if err != nil {
				return err
			}
			if err := transaction.addRemovalSnapshot(path, data, info.Mode()); err != nil {
				return err
			}
			removed++
		}
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
		result = fmt.Sprintf("symbols: unit %s at %s holds %d functions in %d module(s), %d under program conditionals; %d file(s) removed",
			*name, clean, len(merged), len(order), conditional, removed)
		if *dryRun {
			result = "planned: " + result
			return nil
		}
		return transaction.commit()
	})
	if err != nil {
		return err
	}
	fmt.Println(result)
	return nil
}

// unitPieces is an existing unit file cut per member.
type unitPieces struct {
	includes []string
	pieces   map[string]string
}

// readUnitPieces cuts a module's existing unit file into one piece per
// member. A file with conditionals cannot be cut reliably.
func readUnitPieces(root string, m *moduleSpec, unitName string) (unitPieces, error) {
	unit := m.unit(unitName)
	data, err := os.ReadFile(filepath.Join(root, filepath.FromSlash(unit.Source)))
	if err != nil {
		return unitPieces{}, err
	}
	if number := programConditionalLine(string(data)); number > 0 {
		return unitPieces{}, fmt.Errorf("%s:%d: unit %s has program conditionals; split it by hand before merging", unit.Source, number, unitName)
	}
	var names []string
	for _, f := range m.unitMembers(unitName) {
		names = append(names, f.Name)
	}
	pieces, includes, err := splitUnitSource(string(data), names)
	if err != nil {
		return unitPieces{}, fmt.Errorf("%s: %w", unit.Source, err)
	}
	result := unitPieces{pieces: map[string]string{}}
	for _, include := range strings.Split(includes, "\n") {
		if include = strings.TrimSpace(include); include != "" {
			result.includes = append(result.includes, include)
		}
	}
	for index, name := range names {
		result.pieces[name] = strings.TrimSpace(pieces[index])
	}
	return result, nil
}

// memberBody reads one member's text as module m compiles it. An alias
// entry takes its target's body with the function name replaced: the
// original object held that duplicate under the alias's name.
func memberBody(root string, m *moduleSpec, f *functionSpec, units map[string]unitPieces) (*unitMemberBody, error) {
	if f.SourceSymbol != "" {
		target := m.function(f.SourceSymbol)
		if target == nil {
			return nil, fmt.Errorf("%s: alias %s names no function %s", m.ID, f.Name, f.SourceSymbol)
		}
		body, err := memberBody(root, m, target, units)
		if err != nil {
			return nil, err
		}
		text := regexp.MustCompile(`\b`+regexp.QuoteMeta(f.SourceSymbol)+`\b`).ReplaceAllString(body.text, f.Name)
		// The alias entry's override rows rebind names to other symbols; the
		// duplicate names those symbols directly, so one object needs no
		// per-member binding.
		for _, override := range m.Overrides {
			if override.Function != f.Name {
				continue
			}
			replacement := ""
			if row := m.dataAt(override.Addr); row != nil {
				replacement = row.Name
			} else {
				for index := range m.Functions {
					if m.Functions[index].Addr == override.Addr {
						replacement = m.Functions[index].Name
						break
					}
				}
			}
			if replacement == "" {
				return nil, fmt.Errorf("%s: alias %s binds %s to 0x%08x, which names no symbol of the module", m.ID, f.Name, override.Name, override.Addr)
			}
			text = regexp.MustCompile(`\b`+regexp.QuoteMeta(override.Name)+`\b`).ReplaceAllString(text, replacement)
		}
		text = fmt.Sprintf("/* The original wrote the body of %s a second time as %s. */\n", f.SourceSymbol, f.Name) + text
		return &unitMemberBody{key: body.key + "@" + f.Name, includes: body.includes, text: text}, nil
	}
	if f.Unit != "" {
		unit := m.unit(f.Unit)
		pieces := units[unit.Source]
		return &unitMemberBody{key: unit.Source + "#" + f.Name, includes: pieces.includes, text: pieces.pieces[f.Name]}, nil
	}
	source := m.sourceOf(f)
	data, err := os.ReadFile(filepath.Join(root, filepath.FromSlash(source)))
	if err != nil {
		return nil, err
	}
	body := &unitMemberBody{key: source}
	var lines []string
	for _, line := range strings.Split(string(data), "\n") {
		trimmed := strings.TrimSpace(line)
		if strings.HasPrefix(trimmed, "#include") {
			body.includes = append(body.includes, trimmed)
			continue
		}
		lines = append(lines, line)
	}
	body.text = strings.TrimSpace(strings.Join(lines, "\n"))
	return body, nil
}

// mergeMemberOrders merges the modules' member lists into one order that
// keeps every list's order, or reports the first disagreement.
func mergeMemberOrders(lists [][]string) ([]string, error) {
	at := make([]int, len(lists))
	position := make([]map[string]int, len(lists))
	total := map[string]bool{}
	for index, list := range lists {
		position[index] = map[string]int{}
		for i, name := range list {
			position[index][name] = i
			total[name] = true
		}
	}
	var merged []string
	for len(merged) < len(total) {
		placed := false
		for index, list := range lists {
			if at[index] >= len(list) {
				continue
			}
			candidate := list[at[index]]
			ready := true
			for other := range lists {
				if pos, ok := position[other][candidate]; ok && pos != at[other] {
					ready = false
					break
				}
			}
			if !ready {
				continue
			}
			merged = append(merged, candidate)
			for other := range lists {
				if pos, ok := position[other][candidate]; ok && pos == at[other] {
					at[other]++
				}
			}
			placed = true
			break
		}
		if !placed {
			var heads []string
			for index, list := range lists {
				if at[index] < len(list) {
					heads = append(heads, list[at[index]])
				}
			}
			return nil, fmt.Errorf("the modules order the members differently around %s", strings.Join(heads, ", "))
		}
	}
	return merged, nil
}

// renderUnitSource writes the merged unit: the include block, then every
// member in merged order. A body that only some modules compile is wrapped
// in those modules' program macros; a member with several bodies gets one
// wrapped block per body, in module order. Returns the text and how many
// members sit under conditionals.
func renderUnitSource(order []*moduleSpec, merged []string, bodies map[string]map[string]*unitMemberBody, bodyModules map[string]map[string][]*moduleSpec, fillers map[string]string) ([]byte, int) {
	moduleIndex := map[*moduleSpec]int{}
	for index, m := range order {
		moduleIndex[m] = index
	}
	var includes []string
	seen := map[string]bool{}
	addIncludes := func(list []string) {
		for _, include := range list {
			if !seen[include] {
				seen[include] = true
				includes = append(includes, include)
			}
		}
	}
	var blocks []string
	conditional := 0
	seenConstant := map[string]bool{}
	for _, name := range merged {
		keys := make([]string, 0, len(bodies[name]))
		for key := range bodies[name] {
			keys = append(keys, key)
		}
		sort.Slice(keys, func(i, j int) bool {
			return moduleIndex[bodyModules[name][keys[i]][0]] < moduleIndex[bodyModules[name][keys[j]][0]]
		})
		for _, key := range keys {
			body := bodies[name][key]
			addIncludes(body.includes)
			modules := bodyModules[name][key]
			text := dropRepeatedConstants(body.text, seenConstant)
			if len(keys) == 1 && len(modules) == len(order) {
				blocks = append(blocks, text)
				continue
			}
			conditional++
			var tests []string
			for _, m := range modules {
				tests = append(tests, "defined("+moduleMacro(m.ID)+")")
			}
			guard := "#if " + strings.Join(tests, " || ")
			if len(modules) == 1 {
				guard = "#ifdef " + moduleMacro(modules[0].ID)
			}
			blocks = append(blocks, guard+"\n"+text+"\n#endif")
		}
		if filler := fillers[name]; filler != "" {
			blocks = append(blocks, filler)
		}
	}
	text := strings.Join(blocks, "\n\n") + "\n"
	if len(includes) > 0 {
		text = strings.Join(includes, "\n") + "\n\n" + text
	}
	return []byte(text), conditional
}

// ownedRodataTables finds the module's data rows that lie among the members'
// read-only ranges and are covered by none of them: the object's own tables.
// Each becomes a range the unit declares and a byte-exact filler definition
// emitted after the last member whose literals precede it. A table's size
// runs to the next data row or member range. The filler keeps the original
// bytes under a derived name; a later pass can turn it into the typed
// definition and drop the header's extern.
func ownedRodataTables(image []byte, m *moduleSpec, members []*functionSpec, inherited []rodataSpec) ([]rodataSpec, map[string]string, error) {
	type span struct {
		start, end uint32
		member     string
	}
	var spans []span
	for _, f := range members {
		for _, r := range f.Rodata {
			spans = append(spans, span{r.Addr, r.Addr + uint32(r.Size), f.Name})
		}
	}
	if len(spans) == 0 {
		return nil, nil, nil
	}
	sort.Slice(spans, func(i, j int) bool { return spans[i].start < spans[j].start })
	lo, hi := spans[0].start, spans[len(spans)-1].end
	var rowAddrs []uint32
	for _, row := range m.Data {
		rowAddrs = append(rowAddrs, row.Addr)
	}
	sort.Slice(rowAddrs, func(i, j int) bool { return rowAddrs[i] < rowAddrs[j] })
	covered := func(addr uint32) bool {
		for _, r := range inherited {
			if r.Addr <= addr && addr < r.Addr+uint32(r.Size) {
				return true
			}
		}
		for _, sp := range spans {
			if sp.start <= addr && addr < sp.end {
				return true
			}
		}
		return false
	}
	nextBoundary := func(addr uint32) uint32 {
		next := hi
		for _, r := range inherited {
			if r.Addr > addr && r.Addr < next {
				next = r.Addr
			}
		}
		for _, sp := range spans {
			if sp.start > addr && sp.start < next {
				next = sp.start
			}
		}
		for _, row := range rowAddrs {
			if row > addr && row < next {
				next = row
			}
		}
		return next
	}
	var owned []rodataSpec
	fillers := map[string]string{}
	for _, addr := range rowAddrs {
		if addr < lo || addr >= hi || covered(addr) {
			continue
		}
		end := nextBoundary(addr)
		size := int(end - addr)
		offset := m.offset(addr)
		if offset < 0 || offset+size > len(image) {
			return nil, nil, fmt.Errorf("table at 0x%08x lies outside the image", addr)
		}
		bytes := image[offset : offset+size]
		row := m.dataAt(addr)
		owned = append(owned, rodataSpec{Addr: addr, Size: size, Hash: shortHash(bytes)})
		// emit after the last member whose literal ends at or before the table
		after := ""
		for _, sp := range spans {
			if sp.end <= addr {
				after = sp.member
			}
		}
		if after == "" {
			return nil, nil, fmt.Errorf("table %s at 0x%08x precedes every member literal", row.Name, addr)
		}
		if fillers[after] != "" {
			fillers[after] += "\n\n"
		}
		fillers[after] += renderRodataFiller(row.Name, addr, bytes)
	}
	return owned, fillers, nil
}

// renderRodataFiller writes a table's original bytes as a static const
// array sized and aligned like the original (words when 4-aligned and a
// multiple of 4, else halfwords, else bytes).
func renderRodataFiller(name string, addr uint32, bytes []byte) string {
	var out strings.Builder
	fmt.Fprintf(&out, "/* %s: read-only table of this object (%d bytes at 0x%08x), kept byte for byte\n * until it is written as its typed definition. */\n", name, len(bytes), addr)
	switch {
	case addr%4 == 0 && len(bytes)%4 == 0:
		fmt.Fprintf(&out, "static const u32 %s_rodata[%d] = {", name, len(bytes)/4)
		for i := 0; i < len(bytes); i += 4 {
			if i%32 == 0 {
				out.WriteString("\n   ")
			}
			fmt.Fprintf(&out, " 0x%08x,", uint32(bytes[i])|uint32(bytes[i+1])<<8|uint32(bytes[i+2])<<16|uint32(bytes[i+3])<<24)
		}
	case addr%2 == 0 && len(bytes)%2 == 0:
		fmt.Fprintf(&out, "static const u16 %s_rodata[%d] = {", name, len(bytes)/2)
		for i := 0; i < len(bytes); i += 2 {
			if i%32 == 0 {
				out.WriteString("\n   ")
			}
			fmt.Fprintf(&out, " 0x%04x,", uint32(bytes[i])|uint32(bytes[i+1])<<8)
		}
	default:
		fmt.Fprintf(&out, "static const u8 %s_rodata[%d] = {", name, len(bytes))
		for i := 0; i < len(bytes); i++ {
			if i%16 == 0 {
				out.WriteString("\n   ")
			}
			fmt.Fprintf(&out, " 0x%02x,", bytes[i])
		}
	}
	out.WriteString("\n};")
	return out.String()
}

// dropRepeatedConstants removes file-scope `#define` lines and `enum {...};`
// statements whose text already appeared in an earlier member: several
// per-function files carried the same local constant, which one object
// defines once.
func dropRepeatedConstants(text string, seen map[string]bool) string {
	pattern := regexp.MustCompile(`(?ms)^(?:#define[^\n]*|enum\s*\{[^}]*\}\s*;)[ \t]*\n?`)
	return pattern.ReplaceAllStringFunc(text, func(statement string) string {
		key := strings.TrimSpace(statement)
		if seen[key] {
			return ""
		}
		seen[key] = true
		return statement
	})
}
