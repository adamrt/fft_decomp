// build.go compiles configured functions and verifies them: against the
// original module images (`build`), against the configured hashes without
// the disc (`check`), and records those hashes from the original bytes
// (`checksums`).
package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"runtime"
	"sort"
	"strconv"
	"strings"
	"sync"
)

// compileSlots, when non-nil, bounds concurrent compiles across modules
// that buildDisc builds in parallel.
var compileSlots chan struct{}

// toolJobs is the compile parallelism: TOOLS_JOBS, or one per CPU.
func toolJobs() (int, error) {
	jobs := runtime.NumCPU()
	if value := os.Getenv("TOOLS_JOBS"); value != "" {
		parsed, err := strconv.Atoi(value)
		if err != nil || parsed < 1 {
			return 0, fmt.Errorf("invalid TOOLS_JOBS %q", value)
		}
		jobs = parsed
	}
	return jobs, nil
}

// shortHash is the configured checksum: the first 16 hex digits of SHA-256.
func shortHash(data []byte) string {
	sum := sha256.Sum256(data)
	return hex.EncodeToString(sum[:])[:hashDigits]
}

var effectAlias = regexp.MustCompile(`^effect([0-9]{3})$`)

// resolveModuleName accepts a module id, an event overlay's short name
// (attack) or an effect number (effect259).
func resolveModuleName(config *projectConfig, name string) (*moduleSpec, error) {
	candidates := []string{name, "event-" + name}
	if match := effectAlias.FindStringSubmatch(name); match != nil {
		candidates = append(candidates, "effect-e"+match[1])
	}
	for _, id := range candidates {
		if m, ok := config.module(id); ok {
			return m, nil
		}
	}
	return nil, fmt.Errorf("unknown module %q; use a module id such as battle, event-attack or effect-e259, or an event short name such as attack", name)
}

// resolveModuleSelection resolves a space- or comma-separated list of module
// names; `event` and `effect` select every event-* or effect-* module. The
// result keeps configuration order without duplicates.
func resolveModuleSelection(config *projectConfig, selection string) ([]*moduleSpec, error) {
	selected := make(map[*moduleSpec]bool)
	for _, name := range strings.FieldsFunc(selection, func(r rune) bool { return r == ',' || r == ' ' }) {
		if name == "event" || name == "effect" {
			for _, m := range config.Modules {
				if strings.HasPrefix(m.ID, name+"-") {
					selected[m] = true
				}
			}
			continue
		}
		m, err := resolveModuleName(config, name)
		if err != nil {
			return nil, err
		}
		selected[m] = true
	}
	if len(selected) == 0 {
		return nil, fmt.Errorf("no modules selected by %q", selection)
	}
	var modules []*moduleSpec
	for _, m := range config.Modules {
		if selected[m] {
			modules = append(modules, m)
		}
	}
	return modules, nil
}

// compileProgress prints a line each time another tenth of a command's
// functions has compiled, so a cold-cache build or check is not silent.
type compileProgress struct {
	mu                sync.Mutex
	label             string
	total, done, next int
}

func newCompileProgress(label string, modules []*moduleSpec) *compileProgress {
	progress := &compileProgress{label: label, next: 1}
	for _, m := range modules {
		progress.total += len(m.Functions)
	}
	return progress
}

func (c *compileProgress) step() {
	if c == nil {
		return
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	c.done++
	if tenths := c.done * 10 / c.total; tenths >= c.next {
		fmt.Printf("%s: %d%% (%d/%d)\n", c.label, tenths*10, c.done, c.total)
		c.next = tenths + 1
	}
}

type compiledFunction struct {
	sections map[string][]byte
	err      error
}

type compiledModule struct {
	functions []compiledFunction
	unitData  [][]compiledDataSpan // indexed by moduleSpec.Units, emitted once per unit
}

// compileModule compiles the selected functions of one module (all when
// only is nil) through the historical toolchain, each linked at its target
// address, and returns their allocatable sections.
func (p project) compileModule(config *projectConfig, resolver symbolResolver, m *moduleSpec, only map[string]bool) (*compiledModule, error) {
	results := &compiledModule{
		functions: make([]compiledFunction, len(m.Functions)),
		unitData:  make([][]compiledDataSpan, len(m.Units)),
	}
	jobs, err := toolJobs()
	if err != nil {
		return nil, err
	}
	// Per-function intermediates go to the container-local work directory:
	// thousands of small files are much faster there than on the host bind
	// mount. They are namespaced by module because many effect overlays share
	// a helper name and buildDisc builds modules concurrently.
	workBase := toolWorkDir()
	if workBase == "" {
		workBase = filepath.Join(p.root, "build", "work")
	}
	workDir := filepath.Join(workBase, "modules", m.ID)
	// A job is one per-function compile (index >= 0) or one unit compile
	// (index < 0 names m.Units[-index-1]); a unit's members take its slices.
	indexOf := map[string]int{}
	for index := range m.Functions {
		indexOf[m.Functions[index].Name] = index
	}
	var queue []int
	unitSelected := map[string]bool{}
	for index := range m.Functions {
		f := &m.Functions[index]
		if only != nil && !only[f.Name] {
			continue
		}
		if f.Unit != "" {
			unitSelected[f.Unit] = true
			continue
		}
		queue = append(queue, index)
	}
	for unitIndex := range m.Units {
		if unitSelected[m.Units[unitIndex].Name] {
			queue = append(queue, -unitIndex-1)
		}
	}
	work := make(chan int)
	var workers sync.WaitGroup
	for range min(jobs, max(len(queue), 1)) {
		workers.Add(1)
		go func() {
			defer workers.Done()
			for index := range work {
				// A disc build runs every module at once; compileSlots caps the
				// total number of concurrent compiles across all of them.
				if compileSlots != nil {
					compileSlots <- struct{}{}
				}
				if index >= 0 {
					results.functions[index].sections, results.functions[index].err = p.compileFunction(resolver, m, &m.Functions[index], workDir)
					p.progress.step()
				} else {
					results.unitData[-index-1] = p.compileUnit(config, resolver, m, &m.Units[-index-1], workDir, results.functions, indexOf)
				}
				if compileSlots != nil {
					<-compileSlots
				}
			}
		}()
	}
	for _, index := range queue {
		work <- index
	}
	close(work)
	workers.Wait()
	return results, nil
}

// compileEntry compiles one entry the way the module build does: a plain
// function by itself, a unit member through its unit, returning that
// member's slices.
func (p project) compileEntry(config *projectConfig, resolver symbolResolver, m *moduleSpec, f *functionSpec, workDir string) (map[string][]byte, error) {
	if f.Unit == "" {
		return p.compileFunction(resolver, m, f, workDir)
	}
	results := make([]compiledFunction, len(m.Functions))
	indexOf := map[string]int{}
	for index := range m.Functions {
		indexOf[m.Functions[index].Name] = index
	}
	p.compileUnit(config, resolver, m, m.unit(f.Unit), workDir, results, indexOf)
	result := results[indexOf[f.Name]]
	return result.sections, result.err
}

// compileUnit compiles one configured unit once and hands each member its
// slice of the merged sections, or the unit's error. The unit-owned data is
// returned separately so the module build can emit it once.
func (p project) compileUnit(config *projectConfig, resolver symbolResolver, m *moduleSpec, unit *unitDef, workDir string, results []compiledFunction, indexOf map[string]int) []compiledDataSpan {
	members := m.unitMembers(unit.Name)
	step := func() {
		if p.progress != nil {
			p.progress.step()
		}
	}
	setError := func(err error) {
		for _, f := range members {
			results[indexOf[f.Name]].err = fmt.Errorf("unit %s: %w", unit.Name, err)
			step()
		}
	}
	instance, err := m.unitInstance(config, unit.Name)
	if err != nil {
		setError(err)
		return nil
	}
	source := filepath.Join(p.root, filepath.FromSlash(unit.Source))
	sections, err := p.compileUnitInstance(resolver, instance, source, filepath.Join(workDir, "units", unit.Name))
	if err != nil {
		setError(err)
		return nil
	}
	memberResults, problems := compareUnitSections(instance, sections)
	if len(problems) > 0 {
		setError(errors.New(strings.Join(problems, "; ")))
		return nil
	}
	text := sections[".text"]
	for _, result := range memberResults {
		f := result.f
		offset := int(f.Addr - instance.addr)
		slices := map[string][]byte{".text": text[offset : offset+f.Size]}
		if len(f.Rodata) > 0 {
			var rodata []byte
			for _, span := range f.Rodata {
				start := int(span.Addr - instance.rodata[0].Addr)
				rodata = append(rodata, sections[".rodata"][start:start+span.Size]...)
			}
			slices[".rodata"] = rodata
		}
		results[indexOf[f.Name]].sections = slices
		step()
	}
	return sliceUnitOwnedData(instance, sections)
}

// functionLinker returns f's source path and the linker script that binds the
// names the source uses and places its sections at their target addresses.
func (p project) functionLinker(resolver symbolResolver, m *moduleSpec, f *functionSpec) (string, string, error) {
	source := filepath.Join(p.root, filepath.FromSlash(m.sourceOf(f)))
	used, err := sourceIdentifiers(source)
	if err != nil {
		return "", "", fmt.Errorf("read source: %w", err)
	}
	bindings := resolver.linkerBindings(m, f, used)
	if profile, ok := compilerProfileNamed(m.profileOf(f)); ok && profile.smallDataLimit != 0 {
		bindings["_gp"] = profile.globalPointer
	}
	script, err := linkerScript(f, bindings)
	return source, script, err
}

// compileFunction compiles one configured function with its profile and the
// module's defines, linked at its target address, under workDir/<name>, and
// returns its allocatable sections.
func (p project) compileFunction(resolver symbolResolver, m *moduleSpec, f *functionSpec, workDir string) (map[string][]byte, error) {
	return p.compileFunctionDefines(resolver, m, f, workDir, m.defineFlags())
}

// compileFunctionDefines is compileFunction with an explicit define list, for
// trials that probe a constant before it enters the module configuration.
func (p project) compileFunctionDefines(resolver symbolResolver, m *moduleSpec, f *functionSpec, workDir string, defines []string) (map[string][]byte, error) {
	source, script, err := p.functionLinker(resolver, m, f)
	if err != nil {
		return nil, err
	}
	linker, err := writeLinkerScript(workDir, f.Name, script)
	if err != nil {
		return nil, err
	}
	profile, ok := compilerProfileNamed(m.profileOf(f))
	if !ok {
		return nil, fmt.Errorf("unknown compiler profile %q", m.profileOf(f))
	}
	sections, err := p.compileHistoricalSections(source, linker, filepath.Join(workDir, f.Name), profile, defines...)
	if err == nil {
		return sections, nil
	}
	// Header macros can introduce names absent from the source scan. Bind only
	// actual undefined object symbols that resolve in this module's context;
	// unknown names still fail the link, and overrides keep their usual priority.
	object := filepath.Join(workDir, f.Name, profile.name+".o")
	if used, objectErr := undefinedObjectIdentifiers(object); objectErr == nil {
		sourceUsed, readErr := sourceIdentifiers(source)
		if readErr == nil {
			for name := range sourceUsed {
				used[name] = struct{}{}
			}
			bindings := resolver.linkerBindings(m, f, used)
			if profile.smallDataLimit != 0 {
				bindings["_gp"] = profile.globalPointer
			}
			if expanded, scriptErr := linkerScript(f, bindings); scriptErr == nil && expanded != script {
				if _, writeErr := writeLinkerScript(workDir, f.Name, expanded); writeErr == nil {
					return p.compileHistoricalSections(source, linker, filepath.Join(workDir, f.Name), profile, defines...)
				}
			}
		}
	}
	// A source read while an editor rewrites it can yield a script without
	// its bindings; when a fresh read differs, relink with that once.
	if _, fresh, freshErr := p.functionLinker(resolver, m, f); freshErr == nil && fresh != script {
		if _, writeErr := writeLinkerScript(workDir, f.Name, fresh); writeErr == nil {
			return p.compileHistoricalSections(source, linker, filepath.Join(workDir, f.Name), profile, defines...)
		}
	}
	return nil, err
}

// undeclaredSection names a compiled allocatable section the configuration
// does not account for.
func undeclaredSection(f *functionSpec, sections map[string][]byte) string {
	for name := range sections {
		if name != ".text" && (name != ".rodata" || len(f.Rodata) == 0) {
			return name
		}
	}
	return ""
}

func (p project) readModuleImage(m *moduleSpec) ([]byte, error) {
	data, err := os.ReadFile(filepath.Join(p.filesRoot, filepath.FromSlash(m.File)))
	if err != nil {
		return nil, errNotExtracted
	}
	if len(data) != m.Size {
		return nil, fmt.Errorf("%s: extracted %s has %d bytes, configured %d", m.ID, m.File, len(data), m.Size)
	}
	if sum := sha256.Sum256(data); hex.EncodeToString(sum[:]) != m.SHA256 {
		return nil, fmt.Errorf("%s: extracted %s does not match its configured sha256", m.ID, m.File)
	}
	return data, nil
}

func moduleBuildOutput(root string, m *moduleSpec) string {
	base := strings.ToLower(filepath.Base(filepath.FromSlash(m.File)))
	switch {
	case m.ID == mainModuleID:
		return filepath.Join(root, "build", "main", base)
	case strings.HasPrefix(m.ID, "event-") && m.ID != "event-small":
		name := strings.TrimSuffix(base, filepath.Ext(base))
		return filepath.Join(root, "build", "event", name, base)
	}
	return filepath.Join(root, "build", strings.ToLower(filepath.Dir(filepath.FromSlash(m.File))), base)
}

// buildModule replaces every configured function and data range in a copy of
// the original image with compiled bytes, failing on the first difference unless
// TOOLS_REPORT_ALL_MISMATCHES is set, and writes the rebuilt image.
func (p project) buildModule(config *projectConfig, resolver symbolResolver, m *moduleSpec) (string, error) {
	want, err := p.readModuleImage(m)
	if err != nil {
		return "", err
	}
	if err := validateUnitOwnedOriginal(m, want); err != nil {
		return "", err
	}
	results, err := p.compileModule(config, resolver, m, nil)
	if err != nil {
		return "", err
	}
	got := append([]byte(nil), want...)
	languageBytes := make(map[string]int)
	languageCounts := make(map[string]int)
	var mismatched []string
	for index := range m.Functions {
		f := &m.Functions[index]
		language := "C"
		if f.Asm != "" {
			language = "asm"
		}
		if results.functions[index].err != nil {
			return "", fmt.Errorf("compile %s: %w", f.Name, results.functions[index].err)
		}
		sections := results.functions[index].sections
		start := m.offset(f.Addr)
		expected := want[start : start+f.Size]
		if shortHash(expected) != f.Hash {
			return "", fmt.Errorf("%s: %s hash does not match the original bytes; run `make checksums`", m.ID, f.Name)
		}
		if offset, differs := mismatch(expected, sections[".text"]); differs {
			line := fmt.Sprintf("%s mismatch at function offset 0x%x (file offset 0x%x)", f.Name, offset, start+offset)
			if os.Getenv("TOOLS_REPORT_ALL_MISMATCHES") == "" {
				return "", errors.New(line)
			}
			mismatched = append(mismatched, line)
			continue
		}
		copy(got[start:], sections[".text"])
		languageCounts[language]++
		languageBytes[language] += f.Size
		fmt.Fprintf(p.stdout(), "matching %s: %s (%d bytes)\n", language, f.Name, f.Size)
		for _, rodata := range f.Rodata {
			rodataStart := m.offset(rodata.Addr)
			expected := want[rodataStart : rodataStart+rodata.Size]
			if shortHash(expected) != rodata.Hash {
				return "", fmt.Errorf("%s: %s rodata 0x%08x hash does not match the original bytes; run `make checksums`", m.ID, f.Name, rodata.Addr)
			}
			compiled, ok := sections[".rodata"]
			if !ok {
				return "", fmt.Errorf("%s did not emit declared section .rodata", f.Name)
			}
			if offset, differs := mismatch(expected, compiled); differs {
				return "", fmt.Errorf("%s .rodata mismatch at section offset 0x%x (file offset 0x%x)", f.Name, offset, rodataStart+offset)
			}
			copy(got[rodataStart:], compiled)
			fmt.Fprintf(p.stdout(), "matching %s data: %s .rodata (%d bytes)\n", language, f.Name, len(compiled))
		}
		if section := undeclaredSection(f, sections); section != "" {
			return "", fmt.Errorf("%s emitted undeclared allocatable section %s", f.Name, section)
		}
	}
	if len(mismatched) > 0 {
		for _, line := range mismatched {
			fmt.Fprintf(os.Stderr, "MISMATCH: %s\n", line)
		}
		return "", fmt.Errorf("%d function(s) did not match", len(mismatched))
	}
	for index := range m.Units {
		unit := &m.Units[index]
		if err := writeUnitOwnedData(m, unit, results.unitData[index], want, got); err != nil {
			return "", err
		}
		for _, span := range results.unitData[index] {
			fmt.Fprintf(p.stdout(), "matching C data: unit %s %s 0x%08x (%d bytes)\n", unit.Name, span.section, span.addr, len(span.data))
		}
	}
	output := moduleBuildOutput(p.root, m)
	if err := os.MkdirAll(filepath.Dir(output), 0o755); err != nil {
		return "", err
	}
	if err := os.WriteFile(output, got, 0o644); err != nil {
		return "", err
	}
	for _, language := range []string{"C", "asm"} {
		if languageCounts[language] != 0 {
			fmt.Fprintf(p.stdout(), "matching %s total: %d bytes across %d functions\n", language, languageBytes[language], languageCounts[language])
		}
	}
	relative, err := filepath.Rel(p.root, output)
	if err != nil {
		return "", err
	}
	fmt.Fprintf(p.stdout(), "byte-exact: %s (%d bytes)\n", filepath.ToSlash(relative), len(got))
	return output, nil
}

// validateUnitOwnedOriginal checks unit data contracts against the verified
// original image, independently of the compiled data's configured hashes.
func validateUnitOwnedOriginal(m *moduleSpec, image []byte) error {
	if err := validateUnitOwnedLayout(m); err != nil {
		return err
	}
	for _, unit := range m.Units {
		for _, section := range []struct {
			name  string
			spans []rodataSpec
		}{{".data", unit.Data}, {".rodata", unit.Rodata}} {
			for _, span := range section.spans {
				if span.Size <= 0 || !m.contains(span.Addr, span.Size) {
					return fmt.Errorf("%s: unit %s owned %s at 0x%08x lies outside the module image", m.ID, unit.Name, section.name, span.Addr)
				}
				start := m.offset(span.Addr)
				if shortHash(image[start:start+span.Size]) != span.Hash {
					return fmt.Errorf("%s: unit %s owned %s at 0x%08x hash does not match the original bytes; run `make checksums`", m.ID, unit.Name, section.name, span.Addr)
				}
			}
		}
	}
	return nil
}

// loadCheckedConfig loads the configuration and runs check-config's checks.
func (p project) loadCheckedConfig() (*projectConfig, error) {
	config, err := p.loadProjectConfig()
	if err != nil {
		return nil, err
	}
	if err := p.validateProjectConfig(config); err != nil {
		return nil, err
	}
	if err := validateDeclarations(p.root); err != nil {
		return nil, err
	}
	if err := validatePlaceholderMembers(p.root); err != nil {
		return nil, err
	}
	return config, nil
}

func (p project) buildCommand(args []string) error {
	if len(args) > 1 {
		return errors.New("usage: tools build [module|disc]")
	}
	if len(args) == 0 || args[0] == "" || args[0] == "disc" || args[0] == "all" {
		return p.buildDisc()
	}
	config, err := p.loadCheckedConfig()
	if err != nil {
		return err
	}
	m, err := resolveModuleName(config, args[0])
	if err != nil {
		return err
	}
	if len(m.Functions) == 0 {
		return fmt.Errorf("module %s has no functions", m.ID)
	}
	mirrored, err := p.withInputMirror()
	if err != nil {
		return err
	}
	mirrored.progress = newCompileProgress("build", []*moduleSpec{m})
	_, err = mirrored.buildModule(config, newSymbolResolver(config), m)
	return err
}

func (p project) buildDisc() error {
	config, err := p.loadCheckedConfig()
	if err != nil {
		return err
	}
	p, err = p.withInputMirror()
	if err != nil {
		return err
	}
	p.progress = newCompileProgress("build", config.Modules)
	resolver := newSymbolResolver(config)
	// Build every module concurrently (most overlays hold only a few
	// functions); compileSlots caps the total concurrent compiles at toolJobs.
	jobs, err := toolJobs()
	if err != nil {
		return err
	}
	compileSlots = make(chan struct{}, jobs)
	defer func() { compileSlots = nil }()
	type moduleResult struct {
		output string
		err    error
		log    bytes.Buffer
	}
	results := make([]*moduleResult, len(config.Modules))
	var moduleWorkers sync.WaitGroup
	for index, m := range config.Modules {
		if len(m.Functions) == 0 {
			continue
		}
		result := &moduleResult{}
		results[index] = result
		moduleWorkers.Add(1)
		go func(m *moduleSpec, result *moduleResult) {
			defer moduleWorkers.Done()
			modulePass := p
			modulePass.out = &result.log
			result.output, result.err = modulePass.buildModule(config, resolver, m)
		}(m, result)
	}
	moduleWorkers.Wait()
	built := make(map[string]string)
	for index, result := range results {
		if result == nil {
			continue
		}
		os.Stdout.Write(result.log.Bytes())
		if result.err != nil {
			return result.err
		}
		built[config.Modules[index].ID] = result.output
	}
	buildDir := filepath.Join(p.root, "build", "disc")
	if err := os.MkdirAll(buildDir, 0o755); err != nil {
		return err
	}
	projectXML, err := os.ReadFile(p.xmlPath)
	if err != nil {
		return errNotExtracted
	}
	filesRoot := xmlAttribute(filepath.ToSlash(p.filesRoot))
	projectText := strings.ReplaceAll(string(projectXML), `source="files/`, `source="`+filesRoot+`/`)
	for _, m := range config.Modules {
		output, ok := built[m.ID]
		if !ok {
			continue
		}
		from := `source="` + filesRoot + `/` + m.File + `"`
		to := `source="` + xmlAttribute(filepath.ToSlash(output)) + `"`
		updated := strings.Replace(projectText, from, to, 1)
		if updated == projectText {
			return fmt.Errorf("could not redirect %s in disc project", m.File)
		}
		projectText = updated
	}
	projectText = strings.Replace(projectText, `file="files/license_data.dat"`, `file="`+filesRoot+`/license_data.dat"`, 1)
	buildXML := filepath.Join(buildDir, "disc.xml")
	if err := os.WriteFile(buildXML, []byte(projectText), 0o644); err != nil {
		return err
	}
	output := filepath.Join(buildDir, targetOutput+".bin")
	cue := filepath.Join(buildDir, targetOutput+".cue")
	if err := runInDir(p.root, "mkpsxiso", "-q", "-y", "-o", output, "-c", cue, buildXML); err != nil {
		return err
	}
	for _, m := range config.Modules {
		path, ok := built[m.ID]
		if !ok {
			continue
		}
		want, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		got, err := readMode2File(output, m.LBA, len(want))
		if err != nil {
			return err
		}
		if offset, differs := mismatch(want, got); differs {
			return fmt.Errorf("rebuilt disc %s mismatch at file offset 0x%x", m.File, offset)
		}
		fmt.Printf("disc module exact: %s at LBA %d (%d bytes)\n", m.File, m.LBA, len(got))
	}
	discSHA256, err := sha256File(output)
	if err != nil {
		return err
	}
	if discSHA256 != targetInputSHA256 {
		return fmt.Errorf("rebuilt disc SHA-256 mismatch: got %s, expected %s", discSHA256, targetInputSHA256)
	}
	fmt.Printf("disc image exact: sha256 %s\n", discSHA256)
	fmt.Printf("built disc: build/disc/%s.cue\n", targetOutput)
	return nil
}

// checkCommand compiles functions and compares their bytes with the
// configured hashes. It never reads the disc or the extracted files.
func (p project) checkCommand(args []string) error {
	flags := flag.NewFlagSet("check", flag.ContinueOnError)
	moduleName := flags.String("module", "", "modules to check (ids, short names, event or effect)")
	if err := flags.Parse(args); err != nil {
		return err
	}
	if flags.NArg() != 0 {
		return errors.New("usage: tools check [--module=MODULES]")
	}
	config, err := p.loadCheckedConfig()
	if err != nil {
		return err
	}
	modules := config.Modules
	if *moduleName != "" {
		if modules, err = resolveModuleSelection(config, *moduleName); err != nil {
			return err
		}
	}
	p, err = p.withInputMirror()
	if err != nil {
		return err
	}
	p.progress = newCompileProgress("check", modules)
	jobs, err := toolJobs()
	if err != nil {
		return err
	}
	compileSlots = make(chan struct{}, jobs)
	defer func() { compileSlots = nil }()
	resolver := newSymbolResolver(config)
	type moduleCheck struct {
		results *compiledModule
		err     error
	}
	checks := make([]moduleCheck, len(modules))
	var workers sync.WaitGroup
	for index, m := range modules {
		workers.Add(1)
		go func(index int, m *moduleSpec) {
			defer workers.Done()
			checks[index].results, checks[index].err = p.compileModule(config, resolver, m, nil)
		}(index, m)
	}
	workers.Wait()
	var problems []string
	functions, bytesChecked := 0, 0
	for index, m := range modules {
		if checks[index].err != nil {
			return checks[index].err
		}
		for functionIndex := range m.Functions {
			f := &m.Functions[functionIndex]
			result := checks[index].results.functions[functionIndex]
			where := fmt.Sprintf("%s %s 0x%08x", m.ID, f.Name, f.Addr)
			if result.err != nil {
				problems = append(problems, fmt.Sprintf("%s: compile failed: %v", where, result.err))
				continue
			}
			text := result.sections[".text"]
			if len(text) != f.Size || shortHash(text) != f.Hash {
				problems = append(problems, fmt.Sprintf("%s: .text is %d bytes with hash %s, expected %d bytes with hash %s", where, len(text), shortHash(text), f.Size, f.Hash))
			}
			for _, rodata := range f.Rodata {
				compiled := result.sections[".rodata"]
				if len(compiled) != rodata.Size || shortHash(compiled) != rodata.Hash {
					problems = append(problems, fmt.Sprintf("%s: .rodata 0x%08x is %d bytes with hash %s, expected %d bytes with hash %s", where, rodata.Addr, len(compiled), shortHash(compiled), rodata.Size, rodata.Hash))
				}
			}
			if section := undeclaredSection(f, result.sections); section != "" {
				problems = append(problems, fmt.Sprintf("%s: emitted undeclared allocatable section %s", where, section))
			}
			functions++
			bytesChecked += f.Size
		}
	}
	if len(problems) > 0 {
		sort.Strings(problems)
		for _, problem := range problems {
			fmt.Fprintf(os.Stderr, "MISMATCH %s\n", problem)
		}
		return fmt.Errorf("check: %d of %d functions differ from their configured hashes", len(problems), functions)
	}
	fmt.Printf("check: %d functions in %d modules match their hashes (%d bytes)\n", functions, len(modules), bytesChecked)
	return nil
}

// checksumsCommand records the hash of every function and .rodata range from
// the original module images, never from compiled output.
func (p project) checksumsCommand(args []string) error {
	if len(args) != 0 {
		return errors.New("usage: tools checksums")
	}
	return p.withTargetLock(func() error {
		config, err := p.loadProjectConfig()
		if err != nil {
			return err
		}
		changed, total := 0, 0
		for _, m := range config.Modules {
			if len(m.Functions) == 0 {
				continue
			}
			image, err := p.readModuleImage(m)
			if err != nil {
				return err
			}
			record := func(addr uint32, size int, hash *string) error {
				if !m.contains(addr, size) || size <= 0 {
					return fmt.Errorf("%s: range 0x%08x+%d lies outside %s", m.ID, addr, size, m.File)
				}
				start := m.offset(addr)
				value := shortHash(image[start : start+size])
				total++
				if *hash != value {
					*hash = value
					changed++
				}
				return nil
			}
			for index := range m.Units {
				unit := &m.Units[index]
				for rangeIndex := range unit.Rodata {
					span := &unit.Rodata[rangeIndex]
					if err := record(span.Addr, span.Size, &span.Hash); err != nil {
						return err
					}
				}
				for rangeIndex := range unit.Data {
					span := &unit.Data[rangeIndex]
					if err := record(span.Addr, span.Size, &span.Hash); err != nil {
						return err
					}
				}
			}
			for index := range m.Functions {
				f := &m.Functions[index]
				if err := record(f.Addr, f.Size, &f.Hash); err != nil {
					return err
				}
				for rodataIndex := range f.Rodata {
					rodata := &f.Rodata[rodataIndex]
					if err := record(rodata.Addr, rodata.Size, &rodata.Hash); err != nil {
						return err
					}
				}
			}
		}
		files, err := p.writeProjectConfig(config)
		if err != nil {
			return err
		}
		fmt.Printf("checksums: %d of %d hashes updated from the original bytes (%d files written)\n", changed, total, files)
		return nil
	})
}
