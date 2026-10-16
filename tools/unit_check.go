// unit_check.go compiles a merged translation-unit candidate once per module
// instance and compares every function in it with the original bytes. It is
// a prototype driven by a scratch spec under build/, so candidate units can be
// tried without changing the target metadata or weakening any check.
package main

import (
	"bytes"
	"debug/elf"
	"errors"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"

	"gopkg.in/yaml.v3"
)

type unitSpecYAML struct {
	Name      string             `yaml:"name"`
	Source    string             `yaml:"source"`
	Functions []string           `yaml:"functions"`
	Rodata    []rodataYAML       `yaml:"rodata"`
	Instances []unitInstanceYAML `yaml:"instances"`
}

type unitInstanceYAML struct {
	Module    string   `yaml:"module"`
	Functions []string `yaml:"functions"`
	Profile   string   `yaml:"profile"`
}

// unitSpec names a merged source and the original entries, in file order,
// that each module instance compiles from it.
type unitSpec struct {
	name      string
	source    string // project-relative, under build/
	functions []string
	rodata    []rodataSpec // tables the unit defines itself
	instances []unitInstanceYAML
}

func loadUnitSpec(root, path string) (*unitSpec, error) {
	full, err := projectPath(root, path, "unit spec")
	if err != nil {
		return nil, err
	}
	data, err := os.ReadFile(full)
	if err != nil {
		return nil, err
	}
	return parseUnitSpec(path, data)
}

func parseUnitSpec(path string, data []byte) (*unitSpec, error) {
	decoder := yaml.NewDecoder(bytes.NewReader(data))
	decoder.KnownFields(true)
	var file unitSpecYAML
	if err := decoder.Decode(&file); err != nil {
		return nil, fmt.Errorf("%s: %w", path, err)
	}
	spec := &unitSpec{name: file.Name, source: file.Source, functions: file.Functions, instances: file.Instances}
	for _, rodata := range file.Rodata {
		spec.rodata = append(spec.rodata, rodataSpec(rodata))
	}
	if spec.name == "" {
		spec.name = strings.TrimSuffix(filepath.Base(path), filepath.Ext(path))
	}
	if !validSymbol(spec.name) {
		return nil, fmt.Errorf("%s: unit name %q is not a symbol", path, spec.name)
	}
	if spec.source == "" {
		spec.source = "build/units/" + spec.name + "/" + spec.name + ".c"
	}
	spec.source = filepath.ToSlash(filepath.Clean(spec.source))
	if !strings.HasPrefix(spec.source, "build/") {
		return nil, fmt.Errorf("%s: source %q must lie under build/", path, spec.source)
	}
	if len(spec.instances) == 0 {
		return nil, fmt.Errorf("%s: at least one instance is required", path)
	}
	for _, instance := range spec.instances {
		if instance.Module == "" {
			return nil, fmt.Errorf("%s: every instance names a module", path)
		}
		names := instance.Functions
		if names == nil {
			names = spec.functions
		}
		if err := uniqueSymbols(names); err != nil {
			return nil, fmt.Errorf("%s: instance %s: %w", path, instance.Module, err)
		}
	}
	return spec, nil
}

func uniqueSymbols(names []string) error {
	if len(names) == 0 {
		return errors.New("no functions listed")
	}
	seen := map[string]bool{}
	for _, name := range names {
		if !validSymbol(name) || seen[name] {
			return fmt.Errorf("function list has an invalid or repeated name %q", name)
		}
		seen[name] = true
	}
	return nil
}

// unitInstance is one module's compile of the merged source.
type unitInstance struct {
	spec    *unitSpec
	module  *moduleSpec
	members []*functionSpec
	profile compilerProfile
	addr    uint32
	size    int
	rodata  []rodataSpec
	data    []rodataSpec // initialized tables the unit defines, its .data
}

// symbols lists the definitions the unit source must hold, one per member
// in order. An alias entry (source_symbol) becomes its own definition inside
// a unit: the original object held the duplicate body under that name.
func (unit *unitInstance) symbols() []string {
	symbols := make([]string, 0, len(unit.members))
	for _, f := range unit.members {
		symbols = append(symbols, f.Name)
	}
	return symbols
}

// resolveUnitInstance checks that the listed entries exist, are C, follow one
// another without gaps in the listed order, share one profile and declare
// contiguous read-only data.
func resolveUnitInstance(config *projectConfig, spec *unitSpec, instance unitInstanceYAML) (*unitInstance, error) {
	m, err := resolveModuleName(config, instance.Module)
	if err != nil {
		return nil, err
	}
	names := instance.Functions
	if names == nil {
		names = spec.functions
	}
	unit := &unitInstance{spec: spec, module: m}
	for _, name := range names {
		f := m.function(name)
		if f == nil {
			return nil, fmt.Errorf("%s: no function %s in %s", spec.name, name, m.ID)
		}
		if f.Asm != "" {
			return nil, fmt.Errorf("%s: %s is assembly, not C", spec.name, name)
		}
		if len(unit.members) > 0 {
			previous := unit.members[len(unit.members)-1]
			expected := previous.Addr + uint32(previous.Size)
			if f.Addr != expected {
				return nil, fmt.Errorf("%s: %s at 0x%08x does not follow %s, which ends at 0x%08x", spec.name, name, f.Addr, previous.Name, expected)
			}
		}
		unit.members = append(unit.members, f)
		unit.size += f.Size
		for _, span := range f.Rodata {
			// Several members may share one literal (a constant in the
			// object's read-only data): one range, declared once. A later
			// member may also use only the first literals of an earlier
			// member's run, so a range inside a declared one is that run.
			shared := false
			for _, existing := range unit.rodata {
				if existing == span || (span.Addr >= existing.Addr && span.Addr+uint32(span.Size) <= existing.Addr+uint32(existing.Size)) {
					shared = true
					break
				}
			}
			if !shared {
				unit.rodata = append(unit.rodata, span)
			}
		}
	}
	// Tables the unit defines itself sit among its functions' literals.
	for _, span := range spec.rodata {
		unit.rodata = append(unit.rodata, span)
	}
	unit.addr = unit.members[0].Addr
	profileName, err := unitProfile(spec, m, unit.members)
	if err != nil {
		return nil, err
	}
	if instance.Profile != "" && instance.Profile != profileName {
		return nil, fmt.Errorf("%s: instance %s asserts profile %s but its functions use %s", spec.name, m.ID, instance.Profile, profileName)
	}
	unit.profile, _ = compilerProfileNamed(profileName)
	sort.Slice(unit.rodata, func(i, j int) bool { return unit.rodata[i].Addr < unit.rodata[j].Addr })
	for i := 1; i < len(unit.rodata); i++ {
		previous := unit.rodata[i-1]
		end := previous.Addr + uint32(previous.Size)
		// A hole shorter than 8 bytes is the compiler's own padding (a jump
		// table aligns to 8 within its object; a literal may leave a
		// 4-byte hole): the compile must reproduce it, which the section
		// comparison verifies by total length and per-range hashes.
		padding := unit.rodata[i].Addr >= end && unit.rodata[i].Addr-end < 8
		if unit.rodata[i].Addr != end && !padding {
			return nil, fmt.Errorf("%s: rodata ranges are not contiguous at 0x%08x", spec.name, unit.rodata[i].Addr)
		}
	}
	return unit, nil
}

// unitInstance resolves one of the module's configured units: its members
// are the rows naming it, in address order, and they must follow one
// another, share a profile class and declare contiguous read-only data.
func (m *moduleSpec) unitInstance(config *projectConfig, name string) (*unitInstance, error) {
	unit := m.unit(name)
	if unit == nil {
		return nil, fmt.Errorf("%s has no unit %s", m.ID, name)
	}
	var functions []string
	for _, f := range m.unitMembers(name) {
		functions = append(functions, f.Name)
	}
	spec := &unitSpec{name: unit.Name, source: unit.Source, functions: functions, rodata: unit.Rodata}
	instance, err := resolveUnitInstance(config, spec, unitInstanceYAML{Module: m.ID})
	if err != nil {
		return nil, err
	}
	instance.data = append([]rodataSpec(nil), unit.Data...)
	for i := 1; i < len(instance.data); i++ {
		previous := instance.data[i-1]
		end := previous.Addr + uint32(previous.Size)
		if instance.data[i].Addr < end || instance.data[i].Addr-end >= 8 {
			return nil, fmt.Errorf("%s: data ranges are not contiguous at 0x%08x", unit.Name, instance.data[i].Addr)
		}
	}
	return instance, nil
}

// unitProfile is the one profile a unit instance compiles with. Members may
// differ only in division checks: that flag belonged to the whole build, and
// a function without a division compiles identically either way, so the
// checked variant serves the unit.
func unitProfile(spec *unitSpec, m *moduleSpec, members []*functionSpec) (string, error) {
	chosen := m.profileOf(members[0])
	chosenProfile, ok := compilerProfileNamed(chosen)
	if !ok {
		return "", fmt.Errorf("unknown compiler profile %q", chosen)
	}
	for _, f := range members[1:] {
		name := m.profileOf(f)
		if name == chosen {
			continue
		}
		profile, ok := compilerProfileNamed(name)
		if !ok {
			return "", fmt.Errorf("unknown compiler profile %q", name)
		}
		same := profile.compilerPath == chosenProfile.compilerPath && profile.optimization == chosenProfile.optimization &&
			profile.aspsxVersion == chosenProfile.aspsxVersion && profile.smallDataLimit == chosenProfile.smallDataLimit
		if !same {
			return "", fmt.Errorf("%s: profile disagreement: %s uses %s, %s uses %s", spec.name, members[0].Name, chosen, f.Name, name)
		}
		if profile.expandDiv && !chosenProfile.expandDiv {
			chosen, chosenProfile = name, profile
		}
	}
	return chosen, nil
}

// unitLinkerBindings binds the names the merged source uses the way the
// linker does for the module, then applies the members' overrides: one object
// has one address per symbol, so an override of any member governs the unit,
// and two members whose overrides disagree could not have been one object.
// A member's own symbol is never bound; its body must come from the object.
func unitLinkerBindings(resolver symbolResolver, unit *unitInstance, used map[string]struct{}) (map[string]uint32, error) {
	bindings := map[string]uint32{}
	for name, addr := range resolver.own[unit.module.ID] {
		if compilerEmitted(name) {
			bindings[name] = addr
		}
	}
	for name := range used {
		if addr, ok := resolver.resolve(unit.module, name); ok {
			bindings[name] = addr
		}
	}
	owner := map[string]string{}
	for _, f := range unit.members {
		for _, override := range unit.module.Overrides {
			if override.Function != f.Name {
				continue
			}
			if previous, ok := owner[override.Name]; ok && bindings[override.Name] != override.Addr {
				return nil, fmt.Errorf("%s and %s bind %s to different addresses (0x%08x and 0x%08x)",
					previous, f.Name, override.Name, bindings[override.Name], override.Addr)
			}
			bindings[override.Name], owner[override.Name] = override.Addr, f.Name
		}
	}
	for _, f := range unit.members {
		// Module resolution of a member's own symbol can reach another
		// module (an aliased entry is not named after its definition), but
		// the object defines it; only an override can disagree.
		symbol := f.Name
		if addr, ok := bindings[symbol]; ok && owner[symbol] != "" && addr != f.Addr {
			return nil, fmt.Errorf("%s binds unit member %s to 0x%08x, but it is at 0x%08x", owner[symbol], symbol, addr, f.Addr)
		}
		delete(bindings, symbol)
	}
	if unit.profile.smallDataLimit != 0 {
		bindings["_gp"] = unit.profile.globalPointer
	}
	return bindings, nil
}

// unitLinkerScript places the merged .text (and .rodata) at the unit's
// addresses and, when assert is set, requires every member to begin at its
// original entry.
func unitLinkerScript(unit *unitInstance, bindings map[string]uint32, assert bool) string {
	return unitLinkerScriptSkipping(unit, bindings, assert, nil)
}

// unitLinkerScriptSkipping is unitLinkerScript without assertions for the
// named members: a static member is a local symbol the script cannot see,
// and the section comparison still checks its bytes at its address.
func unitLinkerScriptSkipping(unit *unitInstance, bindings map[string]uint32, assert bool, static map[string]bool) string {
	names := make([]string, 0, len(bindings))
	for name := range bindings {
		names = append(names, name)
	}
	sort.Strings(names)
	var script strings.Builder
	script.WriteString("OUTPUT_ARCH(mips)\n")
	for _, name := range names {
		if name == "_gp" {
			fmt.Fprintf(&script, "_gp = 0x%08x;\n", bindings[name])
			continue
		}
		fmt.Fprintf(&script, "PROVIDE(%s = 0x%08x);\n", name, bindings[name])
	}
	script.WriteString("SECTIONS {\n")
	if len(unit.rodata) > 0 {
		fmt.Fprintf(&script, "  .rodata 0x%x : SUBALIGN(4) { *(.rodata) *(.rdata) }\n", unit.rodata[0].Addr)
	}
	if len(unit.data) > 0 {
		fmt.Fprintf(&script, "  .data 0x%x : SUBALIGN(4) { *(.data) }\n", unit.data[0].Addr)
	}
	fmt.Fprintf(&script, "  .text 0x%x : SUBALIGN(4) { *(.text) }\n", unit.addr)
	script.WriteString("  /DISCARD/ : { *(.reginfo) *(.pdr) *(.comment) *(.gnu.attributes) *(.MIPS.abiflags) }\n}\n")
	if assert {
		for _, f := range unit.members {
			if static[f.Name] {
				continue
			}
			fmt.Fprintf(&script, "ASSERT(%s == 0x%08x, \"%s must begin at 0x%08x\");\n", f.Name, f.Addr, f.Name, f.Addr)
		}
	}
	return script.String()
}

// unitStaticMembers names the members the source defines static, as the
// module sees the file.
func unitStaticMembers(unit *unitInstance, source string) (map[string]bool, error) {
	data, err := os.ReadFile(source)
	if err != nil {
		return nil, err
	}
	active, err := activeConditionalText(string(data), unit.module.macros())
	if err != nil {
		return nil, err
	}
	declarations, _, err := scanFileScopeDeclarations(active)
	if err != nil {
		return nil, err
	}
	members := map[string]bool{}
	for _, f := range unit.members {
		members[f.Name] = true
	}
	static := map[string]bool{}
	for _, declaration := range declarations {
		// A static prototype before a plain definition makes it static too.
		if declaration.static && members[declaration.symbol] && functionDeclarationSignature(declaration.signature) {
			static[declaration.symbol] = true
		}
	}
	return static, nil
}

// compileUnitInstance runs the historical pipeline on the merged source for
// one module instance under workDir/<module>.
func (p project) compileUnitInstance(resolver symbolResolver, unit *unitInstance, source, workDir string) (map[string][]byte, error) {
	used, err := sourceIdentifiers(source)
	if err != nil {
		return nil, err
	}
	bindings, err := unitLinkerBindings(resolver, unit, used)
	if err != nil {
		return nil, err
	}
	static, err := unitStaticMembers(unit, source)
	if err != nil {
		return nil, err
	}
	script := unitLinkerScriptSkipping(unit, bindings, true, static)
	linker, err := writeLinkerScript(workDir, unit.module.ID, script)
	if err != nil {
		return nil, err
	}
	buildDir := filepath.Join(workDir, unit.module.ID)
	defines := unit.module.defineFlags()
	sections, err := p.compileHistoricalSections(source, linker, buildDir, unit.profile, defines...)
	if err == nil {
		return sections, nil
	}
	// Header macros can name symbols the source scan misses; bind the
	// object's actual undefined symbols once, as compileFunction does.
	object := filepath.Join(buildDir, unit.profile.name+".o")
	if undefined, objectErr := undefinedObjectIdentifiers(object); objectErr == nil {
		for name := range used {
			undefined[name] = struct{}{}
		}
		if expanded, bindErr := unitLinkerBindings(resolver, unit, undefined); bindErr == nil {
			if rescript := unitLinkerScriptSkipping(unit, expanded, true, static); rescript != script {
				if _, writeErr := writeLinkerScript(workDir, unit.module.ID, rescript); writeErr == nil {
					if sections, retryErr := p.compileHistoricalSections(source, linker, buildDir, unit.profile, defines...); retryErr == nil {
						return sections, nil
					}
				}
			}
			bindings = expanded
		}
		if report := p.unitPlacementReport(unit, bindings, buildDir, object); report != "" {
			err = fmt.Errorf("%w\n%s", err, report)
		}
	}
	return nil, err
}

// unitPlacementReport relinks the object without address assertions and
// reports where each member actually landed.
func (p project) unitPlacementReport(unit *unitInstance, bindings map[string]uint32, buildDir, object string) string {
	if _, err := os.Stat(object); err != nil {
		return ""
	}
	script := filepath.Join(buildDir, "diagnostic.ld")
	if err := os.WriteFile(script, []byte(unitLinkerScript(unit, bindings, false)), 0o644); err != nil {
		return ""
	}
	linked := filepath.Join(buildDir, "diagnostic.elf")
	if err := linkHistoricalObject(p.root, script, object, linked); err != nil {
		return ""
	}
	file, err := elf.Open(linked)
	if err != nil {
		return ""
	}
	defer file.Close()
	symbols, err := file.Symbols()
	if err != nil {
		return ""
	}
	placed := map[string]uint64{}
	for _, symbol := range symbols {
		placed[symbol.Name] = symbol.Value
	}
	var lines []string
	for _, f := range unit.members {
		addr, ok := placed[f.Name]
		switch {
		case !ok:
			lines = append(lines, fmt.Sprintf("  %s: not defined by the merged object", f.Name))
		case addr != uint64(f.Addr):
			lines = append(lines, fmt.Sprintf("  %s: placed at 0x%08x, expected 0x%08x (%+d bytes)", f.Name, addr, f.Addr, int64(addr)-int64(f.Addr)))
		}
	}
	if len(lines) == 0 {
		return ""
	}
	return "placement without address assertions:\n" + strings.Join(lines, "\n")
}

type unitMemberResult struct {
	f      *functionSpec
	ok     bool
	detail string
}

// compareUnitSections slices the merged .text by the configured addresses and
// sizes and applies the same hash criterion as check; .rodata ranges and any
// stray section are unit-level problems.
func compareUnitSections(unit *unitInstance, sections map[string][]byte) ([]unitMemberResult, []string) {
	text := sections[".text"]
	var results []unitMemberResult
	for _, f := range unit.members {
		offset := int(f.Addr - unit.addr)
		result := unitMemberResult{f: f}
		switch {
		case offset+f.Size > len(text):
			result.detail = fmt.Sprintf("compiled .text ends after %d bytes, before this function", len(text))
		case shortHash(text[offset:offset+f.Size]) != f.Hash:
			result.detail = fmt.Sprintf("hash %s, want %s", shortHash(text[offset:offset+f.Size]), f.Hash)
		default:
			result.ok = true
		}
		results = append(results, result)
	}
	var problems []string
	if len(text) != unit.size {
		problems = append(problems, fmt.Sprintf("compiled .text is %d bytes, the unit's functions cover %d", len(text), unit.size))
	}
	problems = append(problems, compareUnitRanges(".rodata", unit.rodata, sections[".rodata"])...)
	problems = append(problems, compareUnitRanges(".data", unit.data, sections[".data"])...)
	names := make([]string, 0, len(sections))
	for name := range sections {
		names = append(names, name)
	}
	sort.Strings(names)
	for _, name := range names {
		if name != ".text" && (name != ".rodata" || len(unit.rodata) == 0) && (name != ".data" || len(unit.data) == 0) {
			problems = append(problems, fmt.Sprintf("emitted undeclared allocatable section %s", name))
		}
	}
	return results, problems
}

// mergeUnitSources concatenates per-function sources into one file: the
// include lines first, in first-seen order and once each, then the bodies in
// unit order with their comments.
func mergeUnitSources(root string, sources []string) ([]byte, error) {
	var includes, bodies []string
	seen := map[string]bool{}
	for _, source := range sources {
		data, err := os.ReadFile(filepath.Join(root, filepath.FromSlash(source)))
		if err != nil {
			return nil, err
		}
		var body []string
		for _, line := range strings.Split(string(data), "\n") {
			trimmed := strings.TrimSpace(line)
			if strings.HasPrefix(trimmed, "#include") {
				if !seen[trimmed] {
					seen[trimmed] = true
					includes = append(includes, trimmed)
				}
				continue
			}
			body = append(body, line)
		}
		bodies = append(bodies, strings.TrimSpace(strings.Join(body, "\n")))
	}
	return []byte(strings.Join(includes, "\n") + "\n\n" + strings.Join(bodies, "\n\n") + "\n"), nil
}

// validateUnitSource requires the merged file to define exactly the unit's
// public symbols, in unit order, with no program conditionals resolved.
func validateUnitSource(root, source string, symbols []string) error {
	data, err := os.ReadFile(filepath.Join(root, filepath.FromSlash(source)))
	if err != nil {
		return err
	}
	return validateUnitSourceData(source, data, symbols)
}

// validateUnitSourceFor requires the merged file, as one module compiles it
// (its program macros and defines applied to the conditionals), to define
// exactly that module's members in order.
func validateUnitSourceFor(root string, m *moduleSpec, source string, symbols []string) error {
	data, err := os.ReadFile(filepath.Join(root, filepath.FromSlash(source)))
	if err != nil {
		return err
	}
	return validateUnitSourceDataFor(m, source, data, symbols)
}

// validateUnitSourceDataFor is validateUnitSourceData on the text one module
// sees after its conditionals are resolved.
func validateUnitSourceDataFor(m *moduleSpec, source string, data []byte, symbols []string) error {
	active, err := activeConditionalText(string(data), m.macros())
	if err != nil {
		return fmt.Errorf("%s: %w", source, err)
	}
	if err := validateUnitSourceData(source, []byte(active), symbols); err != nil {
		return fmt.Errorf("for %s: %w", m.ID, err)
	}
	return nil
}

// validateUnitSourceData checks merged source text before it is written.
func validateUnitSourceData(source string, data []byte, symbols []string) error {
	declarations, _, err := scanFileScopeDeclarations(string(data))
	if err != nil {
		return fmt.Errorf("%s: %w", source, err)
	}
	expected := map[string]bool{}
	for _, symbol := range symbols {
		expected[symbol] = true
	}
	var definitions []string
	for _, declaration := range declarations {
		// A static member is a function the original object kept private; a
		// static non-member is an inlined helper that emits no code of its own.
		if declaration.definition && functionDeclarationSignature(declaration.signature) && (!declaration.static || expected[declaration.symbol]) {
			definitions = append(definitions, declaration.symbol)
		}
	}
	if strings.Join(definitions, " ") != strings.Join(symbols, " ") {
		return fmt.Errorf("%s defines %v; the unit expects %v in that order", source, definitions, symbols)
	}
	return nil
}

func (p project) unitCheckCommand(args []string) error {
	flags := flag.NewFlagSet("unit-check", flag.ContinueOnError)
	specPath := flags.String("spec", "", "unit spec (YAML) under build/")
	module := flags.String("module", "", "instances to check (module ids, aliases, event or effect)")
	merge := flags.Bool("merge", false, "write the merged source from the instances' per-function sources first")
	if err := flags.Parse(args); err != nil {
		return err
	}
	if flags.NArg() != 0 || *specPath == "" {
		return errors.New("usage: tools unit-check --spec=build/units/NAME.yaml [--module=MODULES] [--merge]")
	}
	config, err := p.loadProjectConfig()
	if err != nil {
		return err
	}
	spec, err := loadUnitSpec(p.root, *specPath)
	if err != nil {
		return err
	}
	var selected map[string]bool
	if *module != "" {
		modules, err := resolveModuleSelection(config, *module)
		if err != nil {
			return err
		}
		selected = map[string]bool{}
		for _, m := range modules {
			selected[m.ID] = true
		}
	}
	var instances []*unitInstance
	for _, instanceSpec := range spec.instances {
		unit, err := resolveUnitInstance(config, spec, instanceSpec)
		if err != nil {
			return err
		}
		if selected != nil && !selected[unit.module.ID] {
			continue
		}
		instances = append(instances, unit)
	}
	if len(instances) == 0 {
		return fmt.Errorf("%s: no instance selected", spec.name)
	}
	symbols := instances[0].symbols()
	for _, unit := range instances[1:] {
		if strings.Join(unit.symbols(), " ") != strings.Join(symbols, " ") {
			return fmt.Errorf("%s: instances define different symbols: %s has %v, %s has %v",
				spec.name, instances[0].module.ID, symbols, unit.module.ID, unit.symbols())
		}
	}
	sourcePath := filepath.Join(p.root, filepath.FromSlash(spec.source))
	if *merge {
		var sources []string
		for _, f := range instances[0].members {
			sources = append(sources, instances[0].module.sourceOf(f))
		}
		for _, unit := range instances[1:] {
			var other []string
			for _, f := range unit.members {
				other = append(other, unit.module.sourceOf(f))
			}
			if strings.Join(other, " ") != strings.Join(sources, " ") {
				return fmt.Errorf("%s: %s and %s compile from different sources", spec.name, instances[0].module.ID, unit.module.ID)
			}
		}
		merged, err := mergeUnitSources(p.root, sources)
		if err != nil {
			return err
		}
		if err := os.MkdirAll(filepath.Dir(sourcePath), 0o755); err != nil {
			return err
		}
		if err := atomicWrite(sourcePath, merged); err != nil {
			return err
		}
		fmt.Fprintf(p.stdout(), "unit-check: wrote %s from %d sources\n", spec.source, len(sources))
	}
	if err := validateUnitSource(p.root, spec.source, symbols); err != nil {
		return err
	}
	resolver := newSymbolResolver(config)
	workDir := filepath.Join(p.root, "build", "units", "work", spec.name)
	failed, total, problemCount := 0, 0, 0
	for _, unit := range instances {
		unlock, err := fileLock(filepath.Join(workDir, unit.module.ID+".lock"))
		if err != nil {
			return err
		}
		fmt.Fprintf(p.stdout(), "unit %s (%s): %d functions, %d bytes at 0x%08x, profile %s\n",
			spec.name, unit.module.ID, len(unit.members), unit.size, unit.addr, unit.profile.name)
		sections, err := p.compileUnitInstance(resolver, unit, sourcePath, workDir)
		if err != nil {
			fmt.Fprintf(p.stdout(), "  FAIL compile: %v\n", err)
			failed += len(unit.members)
			total += len(unit.members)
		} else {
			results, problems := compareUnitSections(unit, sections)
			for _, result := range results {
				total++
				if result.ok {
					fmt.Fprintf(p.stdout(), "  ok   0x%08x %6d %s\n", result.f.Addr, result.f.Size, result.f.Name)
				} else {
					failed++
					fmt.Fprintf(p.stdout(), "  FAIL 0x%08x %6d %s: %s\n", result.f.Addr, result.f.Size, result.f.Name, result.detail)
				}
			}
			for _, problem := range problems {
				problemCount++
				fmt.Fprintf(p.stdout(), "  PROBLEM %s\n", problem)
			}
		}
		if err := unlock(); err != nil {
			return err
		}
	}
	if failed == 0 && problemCount == 0 {
		fmt.Fprintf(p.stdout(), "unit-check: %d functions in %d instances match\n", total, len(instances))
		return nil
	}
	return fmt.Errorf("unit-check: %d of %d functions differ, %d unit-level problems", failed, total, problemCount)
}

// compareUnitRanges checks a compiled section against the ranges the unit
// declares for it: the section spans exactly from the first range to the end
// of the last, and every range hashes to its original bytes.
func compareUnitRanges(section string, ranges []rodataSpec, data []byte) []string {
	if len(ranges) == 0 {
		return nil
	}
	last := ranges[len(ranges)-1]
	total := int(last.Addr + uint32(last.Size) - ranges[0].Addr)
	if section == ".data" && len(data) > total && len(data)-total < 8 && allZero(data[total:]) {
		// The section is padded to its alignment after the last table.
		data = data[:total]
	}
	if len(data) != total {
		return []string{fmt.Sprintf("compiled %s is %d bytes, the declared ranges cover %d", section, len(data), total)}
	}
	var problems []string
	for _, span := range ranges {
		start := int(span.Addr - ranges[0].Addr)
		if shortHash(data[start:start+span.Size]) != span.Hash {
			problems = append(problems, fmt.Sprintf("%s at 0x%08x differs", section, span.Addr))
		}
	}
	return problems
}

func allZero(data []byte) bool {
	for _, b := range data {
		if b != 0 {
			return false
		}
	}
	return true
}
