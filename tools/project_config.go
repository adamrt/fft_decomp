// project_config.go loads, validates and canonically writes target/*.yaml, one
// YAML document per disc module; tools read and write only through it, so hand
// edits and tool output share one spelling (`make config-fmt`).
package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"

	"gopkg.in/yaml.v3"
)

const (
	modulesConfigDir  = "target"
	hashDigits        = 16
	mainModuleID      = "main"
	mainAssemblyLimit = 0x800120f4
)

type projectConfig struct {
	Modules []*moduleSpec
	byID    map[string]*moduleSpec
	files   map[string][]byte // module files as loaded, for the canonical-form check
}

// moduleSpec is one disc file. Runtime address addr maps to file offset
// addr - Load; for the PS-X EXE, Load is the address of its 0x800-byte header.
type moduleSpec struct {
	Comment   string // leading comment block, kept verbatim
	ID        string
	File      string // path on the disc
	LBA       int
	Size      int
	SHA256    string
	Load      uint32
	SourceDir string
	Profile   string       // default compiler profile of the module's functions
	Links     []string     // modules resident alongside this one, searched after it
	Defines   []defineSpec // VARIANT_ macros passed as -D flags to every compile of this module
	Units     []unitDef    // merged sources whose member functions carry unit: instead of source:
	Libraries []librarySpec
	Functions []functionSpec
	Data      []symbolSpec
	Imports   []symbolSpec
	Overrides []overrideSpec
	Regions   []regionSpec

	path string // file path relative to the project root
}

type functionSpec struct {
	Addr         uint32
	Size         int
	Name         string
	Hash         string
	Profile      string // empty: the module profile
	Source       string // empty: <source_dir>/<name>.c, or .s when Asm is set
	SourceSymbol string // empty: Name; C definition compiled separately for this entry
	Asm          string // why this function is standalone assembly
	Unit         string // the module unit this function is compiled within (then Source is the unit's)
	Rodata       []rodataSpec
}

// unitDef is one original translation unit: a merged source, compiled once
// per module, whose public functions are the member rows naming it, in
// address order.
type unitDef struct {
	Name    string
	Source  string
	Profile string       // empty: each member's own profile; else the profile every member compiles with
	Rodata  []rodataSpec // read-only tables the unit defines between its functions' literals
}

type rodataSpec struct {
	Addr uint32
	Size int
	Hash string
}

// defineSpec is one per-module build constant: a VARIANT_ macro whose value
// a trial proved differs between the programs that compile one source.
type defineSpec struct {
	Name  string
	Value string
}

// definePrefix starts every per-module build constant.
const definePrefix = "VARIANT_"

// moduleMacroPrefix starts the macro the build defines for each program, so
// a shared source can keep a program-specific block: FFT_BATTLE, FFT_WORLD,
// FFT_EVENT_BUNIT, FFT_EFFECT_E000.
const moduleMacroPrefix = "FFT_"

// moduleMacro names the program macro of a module id or family.
func moduleMacro(id string) string {
	return moduleMacroPrefix + strings.ToUpper(strings.ReplaceAll(id, "-", "_"))
}

// macroNames lists the program macros a compile of this module defines: its
// own and, for an overlay family such as event-bunit, the family's (FFT_EVENT).
func (m *moduleSpec) macroNames() []string {
	names := []string{moduleMacro(m.ID)}
	if family, _, ok := strings.Cut(m.ID, "-"); ok {
		names = append(names, moduleMacro(family))
	}
	return names
}

// macros maps every macro a compile of this module defines to its value:
// the program macros (1) and the VARIANT_ constants.
func (m *moduleSpec) macros() map[string]string {
	macros := make(map[string]string, len(m.Defines)+2)
	for _, name := range m.macroNames() {
		macros[name] = "1"
	}
	for _, define := range m.Defines {
		macros[define.Name] = define.Value
	}
	return macros
}

// defineFlags renders the module's program macros and defines as
// preprocessor flags.
func (m *moduleSpec) defineFlags() []string {
	flags := make([]string, 0, len(m.Defines)+2)
	for _, name := range m.macroNames() {
		flags = append(flags, "-D"+name+"=1")
	}
	for _, define := range m.Defines {
		flags = append(flags, "-D"+define.Name+"="+define.Value)
	}
	return flags
}

// defineValue reports the module's value for a VARIANT_ macro.
func (m *moduleSpec) defineValue(name string) (string, bool) {
	for _, define := range m.Defines {
		if define.Name == name {
			return define.Value, true
		}
	}
	return "", false
}

type symbolSpec struct {
	Addr uint32
	Name string
}

// overrideSpec binds one name to an address for one function only, ahead of
// the module-wide resolution.
type overrideSpec struct {
	Function string
	Name     string
	Addr     uint32
}

type librarySpec struct {
	ID      string
	Addr    uint32
	End     uint32
	Kind    string
	Library string
}

type regionSpec struct {
	Addr uint32
	End  uint32
	Kind string
	Why  string
}

var libraryKinds = map[string]bool{"psyq": true, "c-runtime": true, "middleware": true}

// regionKinds are the reasons a code range is not reconstructed as C.
var regionKinds = map[string]bool{
	"blocked":     true, // ordinary C the pinned toolchain cannot reproduce
	"data":        true,
	"handwritten": true, // original assembly
	"interior":    true, // a label inside another routine, not an entry
	"library":     true,
	"padding":     true,
}

func (m *moduleSpec) end() uint64 { return uint64(m.Load) + uint64(m.Size) }

// offset converts a runtime address to a file offset in the module image.
func (m *moduleSpec) offset(addr uint32) int { return int(addr - m.Load) }

func (m *moduleSpec) contains(addr uint32, size int) bool {
	return addr >= m.Load && uint64(addr)+uint64(size) <= m.end()
}

func (m *moduleSpec) profileOf(f *functionSpec) string {
	if f.Profile != "" {
		return f.Profile
	}
	if f.Unit != "" {
		if unit := m.unit(f.Unit); unit != nil && unit.Profile != "" {
			return unit.Profile
		}
	}
	return m.Profile
}

func (m *moduleSpec) defaultSource(f *functionSpec) string {
	extension := ".c"
	if f.Asm != "" {
		extension = ".s"
	}
	return m.SourceDir + "/" + f.Name + extension
}

// sourceOf returns the function's source path relative to the project root.
func (m *moduleSpec) sourceOf(f *functionSpec) string {
	if f.Unit != "" {
		if unit := m.unit(f.Unit); unit != nil {
			return unit.Source
		}
		return ""
	}
	if f.Source != "" {
		return f.Source
	}
	return m.defaultSource(f)
}

// unit returns the module's unit definition of that name.
// renderRodataRanges writes read-only ranges as the inline YAML list items.
func renderRodataRanges(ranges []rodataSpec) string {
	items := make([]string, len(ranges))
	for index, rodata := range ranges {
		text := fmt.Sprintf("addr: 0x%08x, size: %d", rodata.Addr, rodata.Size)
		if rodata.Hash != "" {
			text += ", hash: " + yamlText(rodata.Hash)
		}
		items[index] = "{" + text + "}"
	}
	return strings.Join(items, ", ")
}

// dataAt returns the module's data row at an address, or nil.
func (m *moduleSpec) dataAt(addr uint32) *symbolSpec {
	for index := range m.Data {
		if m.Data[index].Addr == addr {
			return &m.Data[index]
		}
	}
	return nil
}

func (m *moduleSpec) unit(name string) *unitDef {
	for index := range m.Units {
		if m.Units[index].Name == name {
			return &m.Units[index]
		}
	}
	return nil
}

// unitMembers returns the functions compiled within the unit, in address order.
func (m *moduleSpec) unitMembers(name string) []*functionSpec {
	var members []*functionSpec
	for index := range m.Functions {
		if m.Functions[index].Unit == name {
			members = append(members, &m.Functions[index])
		}
	}
	return members
}

// regionOverlapping returns the first region overlapping [start, end).
func (m *moduleSpec) regionOverlapping(start, end uint32) (regionSpec, bool) {
	for _, region := range m.Regions {
		if start < region.End && region.Addr < end {
			return region, true
		}
	}
	return regionSpec{}, false
}

func (m *moduleSpec) function(name string) *functionSpec {
	for index := range m.Functions {
		if m.Functions[index].Name == name {
			return &m.Functions[index]
		}
	}
	return nil
}

func (c *projectConfig) module(id string) (*moduleSpec, bool) {
	m, ok := c.byID[id]
	return m, ok
}

// moduleConfigPath is the file that holds a module id. Event overlays share
// target/event.yaml and effect overlays target/effect.yaml, one YAML document
// per module.
func moduleConfigPath(id string) string {
	switch {
	case strings.HasPrefix(id, "event-"):
		return modulesConfigDir + "/event.yaml"
	case strings.HasPrefix(id, "effect-"):
		return modulesConfigDir + "/effect.yaml"
	}
	return modulesConfigDir + "/" + id + ".yaml"
}

// loadProjectConfig reads every module file. It checks syntax and the facts
// every consumer relies on; validateProjectConfig checks the rest.
func (p project) loadProjectConfig() (*projectConfig, error) {
	config := &projectConfig{byID: make(map[string]*moduleSpec), files: make(map[string][]byte)}
	paths, err := filepath.Glob(filepath.Join(p.root, filepath.FromSlash(modulesConfigDir), "*.yaml"))
	if err != nil {
		return nil, err
	}
	sort.Strings(paths)
	for _, path := range paths {
		data, err := os.ReadFile(path)
		if err != nil {
			return nil, err
		}
		relative, err := filepath.Rel(p.root, path)
		if err != nil {
			return nil, err
		}
		relative = filepath.ToSlash(relative)
		config.files[relative] = data
		for index, document := range splitYAMLDocuments(data) {
			module, err := parseModuleConfig(document)
			if err != nil {
				return nil, fmt.Errorf("%s document %d: %w", relative, index+1, err)
			}
			module.path = relative
			if module.ID == "" || !validModuleID(module.ID) || moduleConfigPath(module.ID) != relative {
				return nil, fmt.Errorf("%s: module id %q belongs in %s", relative, module.ID, moduleConfigPath(module.ID))
			}
			if _, exists := config.byID[module.ID]; exists {
				return nil, fmt.Errorf("duplicate module %s", module.ID)
			}
			config.byID[module.ID] = module
			config.Modules = append(config.Modules, module)
		}
	}
	if _, ok := config.byID[mainModuleID]; !ok {
		return nil, fmt.Errorf("%s has no main module", modulesConfigDir)
	}
	return config, nil
}

// splitYAMLDocuments splits a stream at `---` lines, keeping each document's
// leading comment with it.
func splitYAMLDocuments(data []byte) [][]byte {
	var documents [][]byte
	var current []string
	for _, line := range strings.SplitAfter(string(data), "\n") {
		if strings.TrimRight(line, "\r\n") == "---" {
			documents = append(documents, []byte(strings.Join(current, "")))
			current = nil
			continue
		}
		current = append(current, line)
	}
	if len(current) > 0 || len(documents) == 0 {
		documents = append(documents, []byte(strings.Join(current, "")))
	}
	return documents
}

type moduleFileYAML struct {
	Module    string            `yaml:"module"`
	File      string            `yaml:"file"`
	LBA       int               `yaml:"lba"`
	Size      int               `yaml:"size"`
	SHA256    string            `yaml:"sha256"`
	Load      uint32            `yaml:"load"`
	SourceDir string            `yaml:"source_dir"`
	Profile   string            `yaml:"profile"`
	Links     []string          `yaml:"links"`
	Defines   map[string]string `yaml:"defines"`
	Units     []unitYAML        `yaml:"units"`
	Libraries []libraryYAML     `yaml:"libraries"`
	Functions []functionYAML    `yaml:"functions"`
	Data      []symbolYAML      `yaml:"data"`
	Imports   []symbolYAML      `yaml:"imports"`
	Overrides []overrideYAML    `yaml:"overrides"`
	Regions   []regionYAML      `yaml:"regions"`
}

type functionYAML struct {
	Addr         uint32       `yaml:"addr"`
	Size         int          `yaml:"size"`
	Name         string       `yaml:"name"`
	Hash         string       `yaml:"hash"`
	Profile      string       `yaml:"profile"`
	Source       string       `yaml:"source"`
	SourceSymbol string       `yaml:"source_symbol"`
	Asm          string       `yaml:"asm"`
	Unit         string       `yaml:"unit"`
	Rodata       []rodataYAML `yaml:"rodata"`
}

type unitYAML struct {
	Name    string       `yaml:"name"`
	Source  string       `yaml:"source"`
	Profile string       `yaml:"profile"`
	Rodata  []rodataYAML `yaml:"rodata"`
}

type rodataYAML struct {
	Addr uint32 `yaml:"addr"`
	Size int    `yaml:"size"`
	Hash string `yaml:"hash"`
}

type symbolYAML struct {
	Addr uint32 `yaml:"addr"`
	Name string `yaml:"name"`
}

type overrideYAML struct {
	Function string `yaml:"function"`
	Name     string `yaml:"name"`
	Addr     uint32 `yaml:"addr"`
}

type libraryYAML struct {
	ID      string `yaml:"id"`
	Addr    uint32 `yaml:"addr"`
	End     uint32 `yaml:"end"`
	Kind    string `yaml:"kind"`
	Library string `yaml:"library"`
}

type regionYAML struct {
	Addr uint32 `yaml:"addr"`
	End  uint32 `yaml:"end"`
	Kind string `yaml:"kind"`
	Why  string `yaml:"why"`
}

func parseModuleConfig(data []byte) (*moduleSpec, error) {
	decoder := yaml.NewDecoder(bytes.NewReader(data))
	decoder.KnownFields(true)
	var file moduleFileYAML
	if err := decoder.Decode(&file); err != nil {
		return nil, err
	}
	module := &moduleSpec{
		Comment: leadingComment(data), ID: file.Module, File: file.File, LBA: file.LBA,
		Size: file.Size, SHA256: file.SHA256, Load: file.Load, SourceDir: file.SourceDir,
		Profile: file.Profile, Links: file.Links,
	}
	for name, value := range file.Defines {
		module.Defines = append(module.Defines, defineSpec{Name: name, Value: value})
	}
	for _, item := range file.Units {
		unit := unitDef{Name: item.Name, Source: item.Source, Profile: item.Profile}
		for _, rodata := range item.Rodata {
			unit.Rodata = append(unit.Rodata, rodataSpec(rodata))
		}
		sort.SliceStable(unit.Rodata, func(i, j int) bool { return unit.Rodata[i].Addr < unit.Rodata[j].Addr })
		module.Units = append(module.Units, unit)
	}
	for _, item := range file.Libraries {
		module.Libraries = append(module.Libraries, librarySpec(item))
	}
	for _, item := range file.Functions {
		function := functionSpec{
			Addr: item.Addr, Size: item.Size, Name: item.Name, Hash: item.Hash,
			Profile: item.Profile, Source: item.Source, SourceSymbol: item.SourceSymbol, Asm: item.Asm, Unit: item.Unit,
		}
		for _, rodata := range item.Rodata {
			function.Rodata = append(function.Rodata, rodataSpec(rodata))
		}
		module.Functions = append(module.Functions, function)
	}
	for _, item := range file.Data {
		module.Data = append(module.Data, symbolSpec(item))
	}
	for _, item := range file.Imports {
		module.Imports = append(module.Imports, symbolSpec(item))
	}
	for _, item := range file.Overrides {
		module.Overrides = append(module.Overrides, overrideSpec(item))
	}
	for _, item := range file.Regions {
		module.Regions = append(module.Regions, regionSpec(item))
	}
	return module, nil
}

func leadingComment(data []byte) string {
	var lines []string
	for _, line := range strings.Split(string(data), "\n") {
		if !strings.HasPrefix(line, "#") {
			break
		}
		lines = append(lines, strings.TrimRight(line, " \t\r"))
	}
	return strings.Join(lines, "\n")
}

// normalize sorts every list and drops values that restate a default, which is
// exactly the canonical form renderModuleConfig writes.
func (m *moduleSpec) normalize() {
	m.hoistUnitProfiles()
	for index := range m.Functions {
		function := &m.Functions[index]
		if function.Profile == m.Profile && !m.unitOverridesProfile(function) {
			function.Profile = ""
		}
		if function.Unit != "" {
			if unit := m.unit(function.Unit); unit != nil && unit.Profile != "" && function.Profile == unit.Profile {
				function.Profile = ""
			}
		}
		if function.SourceSymbol == function.Name {
			function.SourceSymbol = ""
		}
		if function.Unit != "" || function.Source == m.defaultSource(function) {
			function.Source = ""
		}
		sort.SliceStable(function.Rodata, func(i, j int) bool { return function.Rodata[i].Addr < function.Rodata[j].Addr })
	}
	sort.SliceStable(m.Functions, func(i, j int) bool { return m.Functions[i].Addr < m.Functions[j].Addr })
	sort.SliceStable(m.Units, func(i, j int) bool { return m.Units[i].Name < m.Units[j].Name })
	sortSymbols(m.Data)
	sortSymbols(m.Imports)
	sort.SliceStable(m.Overrides, func(i, j int) bool {
		if m.Overrides[i].Function != m.Overrides[j].Function {
			return m.Overrides[i].Function < m.Overrides[j].Function
		}
		return m.Overrides[i].Name < m.Overrides[j].Name
	})
	sort.SliceStable(m.Libraries, func(i, j int) bool { return m.Libraries[i].Addr < m.Libraries[j].Addr })
	sort.SliceStable(m.Defines, func(i, j int) bool { return m.Defines[i].Name < m.Defines[j].Name })
	sort.SliceStable(m.Regions, func(i, j int) bool {
		if m.Regions[i].Addr != m.Regions[j].Addr {
			return m.Regions[i].Addr < m.Regions[j].Addr
		}
		return m.Regions[i].End > m.Regions[j].End
	})
}

func sortSymbols(symbols []symbolSpec) {
	sort.SliceStable(symbols, func(i, j int) bool {
		if symbols[i].Addr != symbols[j].Addr {
			return symbols[i].Addr < symbols[j].Addr
		}
		return symbols[i].Name < symbols[j].Name
	})
}

// renderModuleConfig writes the canonical form: fixed key order, one flow
// mapping per list row, unquoted hex addresses and decimal sizes.
func renderModuleConfig(m *moduleSpec) []byte {
	m.normalize()
	var out bytes.Buffer
	if m.Comment != "" {
		out.WriteString(m.Comment + "\n")
	}
	fmt.Fprintf(&out, "module: %s\nfile: %s\nlba: %d\nsize: %d\nsha256: %s\nload: 0x%08x\nsource_dir: %s\nprofile: %s\n",
		yamlText(m.ID), yamlText(m.File), m.LBA, m.Size, yamlText(m.SHA256), m.Load, yamlText(m.SourceDir), yamlText(m.Profile))
	if len(m.Links) > 0 {
		links := make([]string, len(m.Links))
		for index, link := range m.Links {
			links[index] = yamlText(link)
		}
		fmt.Fprintf(&out, "links: [%s]\n", strings.Join(links, ", "))
	}
	if len(m.Defines) > 0 {
		defines := make([]string, len(m.Defines))
		for index, define := range m.Defines {
			// Values are integer literals, written bare.
			defines[index] = yamlText(define.Name) + ": " + define.Value
		}
		fmt.Fprintf(&out, "defines: {%s}\n", strings.Join(defines, ", "))
	}
	section := func(name string, count int, row func(int) string) {
		if count == 0 {
			return
		}
		fmt.Fprintf(&out, "\n%s:\n", name)
		for index := range count {
			fmt.Fprintf(&out, "  - {%s}\n", row(index))
		}
	}
	section("units", len(m.Units), func(index int) string {
		item := m.Units[index]
		text := fmt.Sprintf("name: %s, source: %s", yamlText(item.Name), yamlText(item.Source))
		if item.Profile != "" {
			text += ", profile: " + yamlText(item.Profile)
		}
		if len(item.Rodata) > 0 {
			text += ", rodata: [" + renderRodataRanges(item.Rodata) + "]"
		}
		return text
	})
	section("libraries", len(m.Libraries), func(index int) string {
		item := m.Libraries[index]
		return fmt.Sprintf("id: %s, addr: 0x%08x, end: 0x%08x, kind: %s, library: %s", yamlText(item.ID), item.Addr, item.End, yamlText(item.Kind), yamlText(item.Library))
	})
	section("functions", len(m.Functions), func(index int) string {
		item := m.Functions[index]
		fields := []string{fmt.Sprintf("addr: 0x%08x", item.Addr), fmt.Sprintf("size: %d", item.Size), "name: " + yamlText(item.Name)}
		if item.Hash != "" {
			fields = append(fields, "hash: "+yamlText(item.Hash))
		}
		if item.Profile != "" {
			fields = append(fields, "profile: "+yamlText(item.Profile))
		}
		if item.Unit != "" {
			fields = append(fields, "unit: "+yamlText(item.Unit))
		}
		if item.Source != "" {
			fields = append(fields, "source: "+yamlText(item.Source))
		}
		if item.SourceSymbol != "" {
			fields = append(fields, "source_symbol: "+yamlText(item.SourceSymbol))
		}
		if item.Asm != "" {
			fields = append(fields, "asm: "+yamlText(item.Asm))
		}
		if len(item.Rodata) > 0 {
			fields = append(fields, "rodata: ["+renderRodataRanges(item.Rodata)+"]")
		}
		return strings.Join(fields, ", ")
	})
	symbolRow := func(symbols []symbolSpec) func(int) string {
		return func(index int) string {
			return fmt.Sprintf("addr: 0x%08x, name: %s", symbols[index].Addr, yamlText(symbols[index].Name))
		}
	}
	section("data", len(m.Data), symbolRow(m.Data))
	section("imports", len(m.Imports), symbolRow(m.Imports))
	section("overrides", len(m.Overrides), func(index int) string {
		item := m.Overrides[index]
		return fmt.Sprintf("function: %s, name: %s, addr: 0x%08x", yamlText(item.Function), yamlText(item.Name), item.Addr)
	})
	section("regions", len(m.Regions), func(index int) string {
		item := m.Regions[index]
		return fmt.Sprintf("addr: 0x%08x, end: 0x%08x, kind: %s, why: %s", item.Addr, item.End, yamlText(item.Kind), yamlText(item.Why))
	})
	return out.Bytes()
}

var plainYAMLText = regexp.MustCompile(`^[A-Za-z0-9_][A-Za-z0-9_./-]*$`)

// yamlText spells a string as a plain scalar when that reads back as the same
// string in flow context, and as a double-quoted scalar otherwise.
func yamlText(value string) string {
	if plainYAMLText.MatchString(value) {
		var decoded any
		if yaml.Unmarshal([]byte(value), &decoded) == nil {
			if text, ok := decoded.(string); ok && text == value {
				return value
			}
		}
	}
	var quoted bytes.Buffer
	encoder := json.NewEncoder(&quoted)
	encoder.SetEscapeHTML(false)
	_ = encoder.Encode(value)
	return strings.TrimSuffix(quoted.String(), "\n")
}

// renderConfigFiles renders every module file canonically: modules ordered by
// id, one YAML document each.
func renderConfigFiles(config *projectConfig) map[string][]byte {
	grouped := make(map[string][]*moduleSpec)
	for _, m := range config.Modules {
		grouped[moduleConfigPath(m.ID)] = append(grouped[moduleConfigPath(m.ID)], m)
	}
	files := make(map[string][]byte, len(grouped))
	for path, modules := range grouped {
		sort.Slice(modules, func(i, j int) bool { return modules[i].ID < modules[j].ID })
		var out bytes.Buffer
		for index, m := range modules {
			if index > 0 {
				out.WriteString("---\n")
			}
			out.Write(renderModuleConfig(m))
		}
		files[path] = out.Bytes()
	}
	return files
}

// writeProjectConfig writes every module file whose canonical form differs
// from the disk and removes module files that no longer hold a module.
func (p project) writeProjectConfig(config *projectConfig) (int, error) {
	changed := 0
	rendered := renderConfigFiles(config)
	for path, data := range rendered {
		full := filepath.Join(p.root, filepath.FromSlash(path))
		if existing, err := os.ReadFile(full); err == nil && bytes.Equal(existing, data) {
			continue
		}
		if err := os.MkdirAll(filepath.Dir(full), 0o755); err != nil {
			return changed, err
		}
		if err := atomicWrite(full, data); err != nil {
			return changed, err
		}
		changed++
	}
	for path := range config.files {
		if _, ok := rendered[path]; !ok {
			if err := os.Remove(filepath.Join(p.root, filepath.FromSlash(path))); err != nil {
				return changed, err
			}
			changed++
		}
	}
	return changed, nil
}

// configFormat rewrites every module file in canonical form.
func (p project) configFormat() error {
	return p.withTargetLock(func() error {
		config, err := p.loadProjectConfig()
		if err != nil {
			return err
		}
		changed, err := p.writeProjectConfig(config)
		if err != nil {
			return err
		}
		fmt.Printf("config-fmt: %d of %d files rewritten\n", changed, len(config.files))
		return nil
	})
}

func validHash(value string) bool {
	if len(value) != hashDigits {
		return false
	}
	_, err := strconv.ParseUint(value, 16, 64)
	return err == nil && strings.ToLower(value) == value
}

// validateProjectConfig checks everything that can be checked without the
// disc image: canonical form, ordering, ranges, sources, hashes, links.
func (p project) validateProjectConfig(config *projectConfig) error {
	var problems []string
	fail := func(format string, args ...any) { problems = append(problems, fmt.Sprintf(format, args...)) }
	seenFiles := make(map[string]string)
	seenLBAs := make(map[int]string)
	sources := make(map[string]bool)
	sourceSymbols := make(map[string]string)
	checkedDefinitions := make(map[string]bool)
	rendered := renderConfigFiles(config)
	paths := make([]string, 0, len(rendered))
	for path := range rendered {
		paths = append(paths, path)
	}
	sort.Strings(paths)
	for _, path := range paths {
		if !bytes.Equal(rendered[path], config.files[path]) {
			fail("%s is not in canonical form; run `make config-fmt`", path)
		}
	}
	for _, m := range config.Modules {
		label := m.path + " " + m.ID
		if m.File == "" || seenFiles[m.File] != "" {
			fail("%s: missing or duplicate file %q", label, m.File)
		}
		seenFiles[m.File] = m.ID
		if m.LBA <= 0 || seenLBAs[m.LBA] != "" {
			fail("%s: missing or duplicate lba %d", label, m.LBA)
		}
		seenLBAs[m.LBA] = m.ID
		if m.Size <= 0 || !validSHA256(m.SHA256) || m.Load%4 != 0 || m.end() > 0x100000000 {
			fail("%s: invalid size, sha256 or load", label)
		}
		if m.SourceDir == "" || filepath.ToSlash(filepath.Clean(m.SourceDir)) != m.SourceDir || !strings.HasPrefix(m.SourceDir, "src/") {
			fail("%s: source_dir must be a clean path under src/", label)
		}
		if _, ok := compilerProfileNamed(m.Profile); !ok {
			fail("%s: unknown profile %q", label, m.Profile)
		}
		seenLinks := make(map[string]bool)
		for _, link := range m.Links {
			if _, ok := config.byID[link]; !ok || link == m.ID || link == mainModuleID || seenLinks[link] {
				fail("%s: invalid link %q (main is always searched last)", label, link)
			}
			seenLinks[link] = true
		}
		names := make(map[string]string)
		claim := func(name, what string) {
			if !validSymbol(name) {
				fail("%s: invalid %s name %q", label, what, name)
			} else if previous := names[name]; previous != "" {
				fail("%s: %s %s duplicates a %s name", label, what, name, previous)
			}
			names[name] = what
		}
		moduleSources := make(map[string]string)
		unitSources := make(map[string]bool)
		var previousEnd uint64
		for index := range m.Functions {
			f := &m.Functions[index]
			claim(f.Name, "function")
			where := fmt.Sprintf("%s: function %s", label, f.Name)
			if f.Addr%4 != 0 || f.Size <= 0 || f.Size%4 != 0 || !m.contains(f.Addr, f.Size) {
				fail("%s has an invalid or out-of-image range", where)
			}
			if uint64(f.Addr) < previousEnd {
				fail("%s overlaps the previous function", where)
			}
			previousEnd = uint64(f.Addr) + uint64(f.Size)
			if !validHash(f.Hash) {
				fail("%s has no valid hash; run `make checksums`", where)
			}
			if _, ok := compilerProfileNamed(m.profileOf(f)); !ok {
				fail("%s uses unknown profile %q", where, m.profileOf(f))
			}
			source := m.sourceOf(f)
			if f.Unit != "" {
				if m.unit(f.Unit) == nil {
					fail("%s: unit %q is not defined in units", where, f.Unit)
				}
				if f.Source != "" || f.SourceSymbol != "" || f.Asm != "" {
					fail("%s: a unit member carries no source, source_symbol or asm", where)
				}
				unitSources[source] = true
			}
			if f.Asm != "" {
				if m.ID != mainModuleID || filepath.Ext(source) != ".s" || uint64(f.Addr)+uint64(f.Size) > mainAssemblyLimit {
					fail("%s: assembly requires a .s source in a main startup/ABI range below 0x%08x", where, mainAssemblyLimit)
				}
			} else if filepath.Ext(source) != ".c" {
				fail("%s: C function requires a .c source (set asm for assembly)", where)
			}
			_, sourceErr := projectPath(p.root, source, "source")
			if sourceErr != nil {
				fail("%s: %v", where, sourceErr)
			}
			sourceSymbol := f.sourceSymbol()
			if !validSymbol(sourceSymbol) {
				fail("%s: invalid source_symbol %q", where, sourceSymbol)
			}
			if previous := sourceSymbols[source]; f.Unit == "" && previous != "" && previous != sourceSymbol {
				fail("%s: source %s has conflicting definition symbols %s and %s", where, source, previous, sourceSymbol)
			}
			if f.Unit == "" {
				sourceSymbols[source] = sourceSymbol
			}
			if f.SourceSymbol != "" && f.Asm != "" {
				fail("%s: source_symbol requires a C source", where)
			}
			if f.Unit == "" && (f.SourceSymbol != "" || moduleSources[source] != "") && f.Asm == "" && sourceErr == nil && !checkedDefinitions[source] {
				checkedDefinitions[source] = true
				if err := validateSourceDefinition(p.root, source, sourceSymbol); err != nil {
					fail("%s: %v", where, err)
				}
			}
			moduleSources[source] = f.Name
			sources[source] = true
			for _, rodata := range f.Rodata {
				if rodata.Size <= 0 || !m.contains(rodata.Addr, rodata.Size) {
					fail("%s: rodata 0x%08x lies outside the module image", where, rodata.Addr)
				}
				if !validHash(rodata.Hash) {
					fail("%s: rodata 0x%08x has no valid hash; run `make checksums`", where, rodata.Addr)
				}
			}
		}
		seenUnits := make(map[string]bool)
		for _, unit := range m.Units {
			where := fmt.Sprintf("%s: unit %s", label, unit.Name)
			if !validSymbol(unit.Name) || seenUnits[unit.Name] {
				fail("%s has an invalid or duplicate name", where)
			}
			seenUnits[unit.Name] = true
			if filepath.Ext(unit.Source) != ".c" || !strings.HasPrefix(unit.Source, "src/") {
				fail("%s: source must be a .c file under src/", where)
			}
			members := m.unitMembers(unit.Name)
			if len(members) == 0 {
				fail("%s has no member functions", where)
				continue
			}
			if unit.Profile != "" {
				if _, ok := compilerProfileNamed(unit.Profile); !ok {
					fail("%s: unknown profile %q", where, unit.Profile)
				}
				if unit.Profile == m.Profile {
					fail("%s: profile restates the module profile", where)
				}
			}
			for _, rodata := range unit.Rodata {
				if rodata.Size <= 0 || !m.contains(rodata.Addr, rodata.Size) {
					fail("%s: owned rodata 0x%08x lies outside the module image", where, rodata.Addr)
				}
				if !validHash(rodata.Hash) {
					fail("%s: owned rodata 0x%08x has no valid hash", where, rodata.Addr)
				}
				if m.dataAt(rodata.Addr) == nil {
					fail("%s: owned rodata 0x%08x names no data row of the module", where, rodata.Addr)
				}
			}
			if _, err := projectPath(p.root, unit.Source, "unit source"); err != nil {
				fail("%s: %v", where, err)
				continue
			}
			if _, err := m.unitInstance(config, unit.Name); err != nil {
				fail("%s: %v", where, err)
				continue
			}
			symbols := make([]string, len(members))
			for index, f := range members {
				symbols[index] = f.Name
			}
			if err := validateUnitSourceFor(p.root, m, unit.Source, symbols); err != nil {
				fail("%s: %v", where, err)
			}
		}
		for source := range unitSources {
			sources[source] = true
		}
		for _, symbol := range m.Data {
			claim(symbol.Name, "data")
		}
		for _, symbol := range m.Imports {
			claim(symbol.Name, "import")
		}
		for _, override := range m.Overrides {
			f := m.function(override.Function)
			if f != nil && f.SourceSymbol != "" && override.Name == f.sourceSymbol() && override.Addr != f.Addr {
				fail("%s: override %s/%s conflicts with its source definition", label, override.Function, override.Name)
			}
			if f == nil || !validSymbol(override.Name) {
				fail("%s: override %s/%s names no function of this module", label, override.Function, override.Name)
			}
		}
		var libraryEnd uint32
		seenLibraries := make(map[string]bool)
		for _, library := range m.Libraries {
			if !validSymbol(library.ID) || seenLibraries[library.ID] || !libraryKinds[library.Kind] || library.Library == "" {
				fail("%s: library %q has an invalid id, kind or name", label, library.ID)
			}
			seenLibraries[library.ID] = true
			if library.Addr%4 != 0 || library.End%4 != 0 || library.End <= library.Addr || library.Addr < libraryEnd || !m.contains(library.Addr, int(library.End-library.Addr)) {
				fail("%s: library %s range is invalid, overlapping or outside the image", label, library.ID)
			}
			libraryEnd = library.End
		}
		for _, region := range m.Regions {
			where := fmt.Sprintf("%s: region 0x%08x-0x%08x", label, region.Addr, region.End)
			if !regionKinds[region.Kind] || strings.TrimSpace(region.Why) == "" {
				fail("%s needs a known kind and a why", where)
			}
			if region.Addr%4 != 0 || region.End%4 != 0 || region.End <= region.Addr || !m.contains(region.Addr, int(region.End-region.Addr)) {
				fail("%s is invalid or outside the image", where)
			}
			if region.Kind == "blocked" {
				for _, f := range m.Functions {
					if region.Addr < f.Addr+uint32(f.Size) && f.Addr < region.End {
						fail("%s is blocked but overlaps matching function %s; remove the region", where, f.Name)
					}
				}
			}
		}
	}
	if err := validateSourceInventory(p.root, "src", sources, "module configuration"); err != nil {
		fail("%v", err)
	}
	problems = append(problems, validateDefines(p.root, config)...)
	problems = append(problems, validateVariantNames(p.root, config)...)
	if len(problems) > 0 {
		if len(problems) > 40 {
			problems = append(problems[:40], fmt.Sprintf("... and %d more", len(problems)-40))
		}
		return errors.New("config check failed:\n  " + strings.Join(problems, "\n  "))
	}
	return nil
}

// unitOverridesProfile reports whether f belongs to a unit whose own profile
// differs from the module's, so an explicit module profile on f is meaningful.
func (m *moduleSpec) unitOverridesProfile(f *functionSpec) bool {
	if f.Unit == "" {
		return false
	}
	unit := m.unit(f.Unit)
	return unit != nil && unit.Profile != "" && unit.Profile != m.Profile
}

// hoistUnitProfiles moves the profile a unit's members compile with onto the
// unit row, so the members stop repeating it. Members may differ only in
// division checks: no unit mixes checked and unchecked divisions (the trap was
// an assembler option per object), and a function without a division compiles
// identically either way, so the checked variant serves the whole unit, as
// unitProfile already decides when the unit compiles.
func (m *moduleSpec) hoistUnitProfiles() {
	for index := range m.Units {
		unit := &m.Units[index]
		if unit.Profile != "" {
			continue
		}
		members := m.unitMembers(unit.Name)
		if len(members) == 0 {
			continue
		}
		chosen := m.profileOf(members[0])
		chosenProfile, ok := compilerProfileNamed(chosen)
		if !ok {
			continue
		}
		agree := true
		for _, f := range members[1:] {
			name := m.profileOf(f)
			if name == chosen {
				continue
			}
			profile, ok := compilerProfileNamed(name)
			if !ok || profile.compilerPath != chosenProfile.compilerPath || profile.optimization != chosenProfile.optimization ||
				profile.aspsxVersion != chosenProfile.aspsxVersion || profile.smallDataLimit != chosenProfile.smallDataLimit {
				agree = false
				break
			}
			if profile.expandDiv && !chosenProfile.expandDiv {
				chosen, chosenProfile = name, profile
			}
		}
		if !agree || chosen == m.Profile {
			continue
		}
		unit.Profile = chosen
	}
}
