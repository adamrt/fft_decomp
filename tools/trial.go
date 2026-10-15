// trial.go compiles a candidate source at an existing entry, with identifiers
// and constants substituted, and reports a match or the first differing
// instructions. A trial writes only under build/: the candidate is copied
// there with its substitutions applied, and the entry's own configuration is
// cloned in memory with the copy as its source.
package main

import (
	"bytes"
	"errors"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
	"sync"

	"gopkg.in/yaml.v3"
)

// trialSpec is one trial: compile Source's definition at Module's Entry.
type trialSpec struct {
	Module string `yaml:"module"`
	Entry  string `yaml:"entry"`
	Source string `yaml:"source"`
	// Symbol is the function the source defines; empty means the source's
	// single public definition.
	Symbol string `yaml:"symbol,omitempty"`
	// Subst renames identifiers in the copy (whole words outside includes,
	// comments and literals): the names of the entry's own program.
	Subst map[string]string `yaml:"subst,omitempty"`
	// Defines are VARIANT_ values for this compile, over the module's own.
	Defines map[string]string `yaml:"defines,omitempty"`
	// Label names the trial in reports; empty means "<module> <entry> <- <source>".
	Label string `yaml:"label,omitempty"`
}

type trialBatchYAML struct {
	Trials []trialSpec `yaml:"trials"`
}

// trialResult is the outcome of one trial.
type trialResult struct {
	Spec      trialSpec `yaml:"trial"`
	Status    string    `yaml:"status"` // match, differ or error
	Error     string    `yaml:"error,omitempty"`
	Size      int       `yaml:"size"`
	Compiled  int       `yaml:"compiled,omitempty"`
	Differing int       `yaml:"differing,omitempty"` // instructions that differ after alignment
	Score     int       `yaml:"score,omitempty"`
	Lines     []string  `yaml:"lines,omitempty"` // first differing rows, "+off: target | compiled"
}

func (spec trialSpec) label() string {
	if spec.Label != "" {
		return spec.Label
	}
	return fmt.Sprintf("%s %s <- %s", spec.Module, spec.Entry, spec.Source)
}

func (spec trialSpec) validate() error {
	if spec.Module == "" || spec.Entry == "" || spec.Source == "" {
		return fmt.Errorf("trial %q needs module, entry and source", spec.label())
	}
	for old, name := range spec.Subst {
		if !validSymbol(old) || !validSymbol(name) {
			return fmt.Errorf("trial %q: substitution %s=%s is not between identifiers", spec.label(), old, name)
		}
	}
	for name, value := range spec.Defines {
		if !validSymbol(name) || !strings.HasPrefix(name, definePrefix) || !validDefineValue(value) {
			return fmt.Errorf("trial %q: define %s=%s is not a VARIANT_ name with an integer value", spec.label(), name, value)
		}
	}
	return nil
}

func parseTrialBatch(data []byte) ([]trialSpec, error) {
	decoder := yaml.NewDecoder(bytes.NewReader(data))
	decoder.KnownFields(true)
	var batch trialBatchYAML
	if err := decoder.Decode(&batch); err != nil {
		return nil, err
	}
	if len(batch.Trials) == 0 {
		return nil, errors.New("the batch lists no trials")
	}
	for _, spec := range batch.Trials {
		if err := spec.validate(); err != nil {
			return nil, err
		}
	}
	return batch.Trials, nil
}

// parseAssignments reads repeated NAME=VALUE flags.
func parseAssignments(values []string, what string) (map[string]string, error) {
	result := make(map[string]string)
	for _, value := range values {
		name, assigned, ok := strings.Cut(value, "=")
		if !ok || name == "" || assigned == "" {
			return nil, fmt.Errorf("%s %q is not NAME=VALUE", what, value)
		}
		result[name] = assigned
	}
	return result, nil
}

// sourceDefinitionSymbol names the single public function a source defines.
func sourceDefinitionSymbol(data []byte) (string, error) {
	declarations, _, err := scanFileScopeDeclarations(string(data))
	if err != nil {
		return "", err
	}
	var definitions []string
	for _, declaration := range declarations {
		if declaration.definition && functionDeclarationSignature(declaration.signature) {
			definitions = append(definitions, declaration.symbol)
		}
	}
	if len(definitions) != 1 {
		return "", fmt.Errorf("the source defines %d public functions %v; pass symbol", len(definitions), definitions)
	}
	return definitions[0], nil
}

// substituteIdentifiers renames whole-word identifiers outside includes,
// comments and literals in one pass, so a target name that is also a source
// name is not renamed twice.
func substituteIdentifiers(data []byte, subst map[string]string) []byte {
	if len(subst) == 0 {
		return data
	}
	names := make([]string, 0, len(subst))
	for name := range subst {
		names = append(names, regexp.QuoteMeta(name))
	}
	sort.Strings(names)
	pattern := regexp.MustCompile(`\b(` + strings.Join(names, "|") + `)\b`)
	return rewriteCodeOutsideIncludes(data, func(code []byte) []byte {
		return pattern.ReplaceAllFunc(code, func(match []byte) []byte { return []byte(subst[string(match)]) })
	})
}

// trialWorkDir is where one trial compiles: build/trial/<module>/<entry>/<n>.
func (p project) trialWorkDir(spec trialSpec, index int) string {
	return filepath.Join(p.root, "build", "trial", spec.Module, spec.Entry, fmt.Sprintf("t%d", index))
}

// runTrial compiles one trial and compares it with the entry's original
// bytes (from the image when extracted, by hash otherwise).
func (p project) runTrial(config *projectConfig, resolver symbolResolver, images map[string][]byte, spec trialSpec, index, show int) trialResult {
	result := trialResult{Spec: spec, Status: "error"}
	m, err := resolveModuleName(config, spec.Module)
	if err != nil {
		result.Error = err.Error()
		return result
	}
	f := m.function(spec.Entry)
	if f == nil {
		result.Error = fmt.Sprintf("no function %s in %s", spec.Entry, m.ID)
		return result
	}
	if f.Asm != "" {
		result.Error = fmt.Sprintf("%s is assembly", f.Name)
		return result
	}
	result.Size = f.Size
	sourcePath, err := projectPath(p.root, spec.Source, "trial source")
	if err != nil {
		result.Error = err.Error()
		return result
	}
	data, err := os.ReadFile(sourcePath)
	if err != nil {
		result.Error = err.Error()
		return result
	}
	symbol := spec.Symbol
	if symbol == "" {
		if symbol, err = sourceDefinitionSymbol(data); err != nil {
			result.Error = err.Error()
			return result
		}
	}
	if renamed, ok := spec.Subst[symbol]; ok {
		symbol = renamed
	}
	data = substituteIdentifiers(data, spec.Subst)
	workDir := p.trialWorkDir(spec, index)
	if err := os.MkdirAll(workDir, 0o755); err != nil {
		result.Error = err.Error()
		return result
	}
	copyPath := filepath.Join(workDir, filepath.Base(sourcePath))
	if err := os.WriteFile(copyPath, data, 0o644); err != nil {
		result.Error = err.Error()
		return result
	}
	relative, _ := filepath.Rel(p.root, copyPath)
	trial := *f
	trial.Source = filepath.ToSlash(relative)
	trial.SourceSymbol = ""
	if symbol != f.Name {
		trial.SourceSymbol = symbol
	}
	defines := map[string]string{}
	for _, define := range m.Defines {
		defines[define.Name] = define.Value
	}
	for name, value := range spec.Defines {
		defines[name] = value
	}
	flags := make([]string, 0, len(defines))
	for _, name := range sortedKeys(mapKeysTrue(defines)) {
		flags = append(flags, "-D"+name+"="+defines[name])
	}
	sections, err := p.compileFunctionDefines(resolver, m, &trial, workDir, flags)
	if err != nil {
		result.Error = firstLines(err.Error(), 6)
		return result
	}
	compiled := sections[".text"]
	result.Compiled = len(compiled)
	if section := undeclaredSection(f, sections); section != "" {
		result.Error = "emitted undeclared allocatable section " + section
		return result
	}
	expected := images[m.ID]
	if expected != nil {
		expected = expected[m.offset(f.Addr) : m.offset(f.Addr)+f.Size]
	}
	rodataMatch := true
	for _, rodata := range f.Rodata {
		if image := images[m.ID]; image != nil {
			start := m.offset(rodata.Addr)
			rodataMatch = rodataMatch && bytes.Equal(image[start:start+rodata.Size], sections[".rodata"])
		} else {
			rodataMatch = rodataMatch && len(sections[".rodata"]) == rodata.Size && shortHash(sections[".rodata"]) == rodata.Hash
		}
	}
	textMatch := len(compiled) == f.Size && shortHash(compiled) == f.Hash
	if textMatch && rodataMatch {
		result.Status = "match"
		return result
	}
	result.Status = "differ"
	if !rodataMatch {
		result.Lines = append(result.Lines, "rodata differs")
	}
	if expected == nil {
		if !textMatch {
			result.Lines = append(result.Lines, fmt.Sprintf("hash %s, expected %s (extract the disc for instruction detail)", shortHash(compiled), f.Hash))
		}
		return result
	}
	result.Differing, result.Score, result.Lines = trialDifferences(expected, compiled, show, result.Lines)
	return result
}

// trialDifferences aligns the two listings and returns the count of rows
// that differ, the asm-differ style score and the first show rows.
func trialDifferences(expected, compiled []byte, show int, lines []string) (int, int, []string) {
	base := trimTrailingNops(disassembleMIPS(expected))
	mine := trimTrailingNops(disassembleMIPS(compiled))
	differing := 0
	for _, row := range alignMIPSLines(base, mine) {
		var left, right string
		offset := -1
		if row.base >= 0 {
			left, offset = base[row.base].row(), row.base*4
		}
		if row.mine >= 0 {
			right = mine[row.mine].row()
			if row.mine < len(mine) && row.retargeted.mnemonic != "" {
				right = row.retargeted.row()
			}
			if offset < 0 {
				offset = row.mine * 4
			}
		}
		if left == right {
			continue
		}
		differing++
		if len(lines) < show {
			lines = append(lines, fmt.Sprintf("+0x%x: %s | %s", offset, strings.ReplaceAll(left, "\t", " "), strings.ReplaceAll(right, "\t", " ")))
		}
	}
	return differing, scoreMIPSLines(base, mine), lines
}

func firstLines(text string, count int) string {
	lines := strings.Split(strings.TrimSpace(text), "\n")
	if len(lines) > count {
		lines = append(lines[:count], "...")
	}
	return strings.Join(lines, "\n")
}

func (result trialResult) report() string {
	switch result.Status {
	case "match":
		return fmt.Sprintf("MATCH %s (%d bytes)\n", result.Spec.label(), result.Size)
	case "differ":
		var out strings.Builder
		fmt.Fprintf(&out, "DIFF  %s: %d instructions differ, score %d, sizes %d/%d\n", result.Spec.label(), result.Differing, result.Score, result.Size, result.Compiled)
		for _, line := range result.Lines {
			fmt.Fprintf(&out, "      %s\n", line)
		}
		return out.String()
	default:
		return fmt.Sprintf("ERROR %s: %s\n", result.Spec.label(), strings.ReplaceAll(result.Error, "\n", "\n      "))
	}
}

// runTrials runs a batch concurrently and returns results in batch order.
func (p project) runTrials(config *projectConfig, specs []trialSpec, show int) ([]trialResult, error) {
	resolver := newSymbolResolver(config)
	images := make(map[string][]byte)
	for _, spec := range specs {
		m, err := resolveModuleName(config, spec.Module)
		if err != nil {
			return nil, err
		}
		if _, seen := images[m.ID]; !seen {
			image, err := p.readModuleImage(m)
			if err != nil {
				image = nil // compare by hash only
			}
			images[m.ID] = image
		}
	}
	jobs, err := toolJobs()
	if err != nil {
		return nil, err
	}
	results := make([]trialResult, len(specs))
	work := make(chan int)
	var workers sync.WaitGroup
	for range min(jobs, len(specs)) {
		workers.Add(1)
		go func() {
			defer workers.Done()
			for index := range work {
				results[index] = p.runTrial(config, resolver, images, specs[index], index, show)
			}
		}()
	}
	for index := range specs {
		work <- index
	}
	close(work)
	workers.Wait()
	return results, nil
}

func (p project) trialCommand(args []string) error {
	flags := flag.NewFlagSet("trial", flag.ContinueOnError)
	module := flags.String("module", "", "module of the entry")
	entry := flags.String("entry", "", "entry whose bytes the candidate must reproduce")
	source := flags.String("source", "", "candidate C source inside the project")
	symbol := flags.String("symbol", "", "function the candidate defines (default: its single public definition)")
	batch := flags.String("batch", "", "YAML batch of trials (trials: [{module, entry, source, symbol, subst, defines}])")
	twins := flags.String("twins", "", "twin group ids from the twins report, comma separated; trials every member at every other")
	report := flags.String("report", "build/analysis/twins/twins.yaml", "twins report for --twins")
	out := flags.String("out", "", "write the results as YAML under build/")
	show := flags.Int("show", 4, "differing instructions to print per trial")
	var substFlags, defineFlags repeatedFlag
	flags.Var(&substFlags, "subst", "OLD=NEW identifier substitution (repeatable)")
	flags.Var(&defineFlags, "define", "VARIANT_NAME=VALUE for this compile (repeatable)")
	if err := flags.Parse(args); err != nil {
		return err
	}
	if flags.NArg() != 0 {
		return errors.New("trial accepts named flags only")
	}
	config, err := p.loadProjectConfig()
	if err != nil {
		return err
	}
	var specs []trialSpec
	switch {
	case *batch != "" && *module == "" && *twins == "":
		path, err := projectPath(p.root, *batch, "trial batch")
		if err != nil {
			return err
		}
		data, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		if specs, err = parseTrialBatch(data); err != nil {
			return fmt.Errorf("%s: %w", *batch, err)
		}
	case *twins != "" && *batch == "" && *module == "":
		if specs, err = p.twinTrials(config, *report, strings.Split(*twins, ",")); err != nil {
			return err
		}
	case *module != "" && *entry != "" && *source != "" && *batch == "" && *twins == "":
		subst, err := parseAssignments(substFlags, "substitution")
		if err != nil {
			return err
		}
		defines, err := parseAssignments(defineFlags, "define")
		if err != nil {
			return err
		}
		spec := trialSpec{Module: *module, Entry: *entry, Source: *source, Symbol: *symbol, Subst: subst, Defines: defines}
		if err := spec.validate(); err != nil {
			return err
		}
		specs = []trialSpec{spec}
	default:
		return errors.New("usage: tools trial --module=M --entry=E --source=S [--symbol=F] [--subst OLD=NEW]... [--define VARIANT_X=N]... | --batch=FILE | --twins=ID[,ID]... [--report=FILE] [--out=FILE] [--show=N]")
	}
	results, err := p.runTrials(config, specs, *show)
	if err != nil {
		return err
	}
	counts := map[string]int{}
	for _, result := range results {
		fmt.Print(result.report())
		counts[result.Status]++
	}
	if *out != "" {
		path, err := projectPath(p.root, *out, "trial output")
		if err != nil {
			return err
		}
		if !strings.HasPrefix(filepath.ToSlash(*out), "build/") {
			return fmt.Errorf("trial output must lie under build/: %s", *out)
		}
		if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
			return err
		}
		var buffer bytes.Buffer
		encoder := yaml.NewEncoder(&buffer)
		encoder.SetIndent(2)
		if err := encoder.Encode(map[string][]trialResult{"results": results}); err != nil {
			return err
		}
		if err := atomicWrite(path, buffer.Bytes()); err != nil {
			return err
		}
	}
	if len(results) > 1 {
		fmt.Printf("trial: %d match, %d differ, %d error\n", counts["match"], counts["differ"], counts["error"])
	}
	if counts["match"] != len(results) {
		return fmt.Errorf("%d of %d trials did not match", len(results)-counts["match"], len(results))
	}
	return nil
}

// repeatedFlag collects a flag given several times.
type repeatedFlag []string

func (values *repeatedFlag) String() string     { return strings.Join(*values, ",") }
func (values *repeatedFlag) Set(v string) error { *values = append(*values, v); return nil }
