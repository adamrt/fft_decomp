// function.go implements the single-function tools: `diff` compares one
// configured function with its original bytes and `permute` runs
// decomp-permuter on it. Both compile exactly as `build` and `validate` do.
package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"time"
)

// resolveFunction finds a configured function by name. moduleName, an id or
// alias as `build` accepts, is needed only when several modules define it.
func resolveFunction(config *projectConfig, name, moduleName string) (*moduleSpec, *functionSpec, error) {
	if moduleName != "" {
		m, err := resolveModuleName(config, moduleName)
		if err != nil {
			return nil, nil, err
		}
		if f := m.function(name); f != nil {
			return m, f, nil
		}
		return nil, nil, fmt.Errorf("module %s has no function %q", m.ID, name)
	}
	var found []*moduleSpec
	for _, m := range config.Modules {
		if m.function(name) != nil {
			found = append(found, m)
		}
	}
	switch len(found) {
	case 0:
		return nil, nil, fmt.Errorf("no function %q in target/*.yaml", name)
	case 1:
		return found[0], found[0].function(name), nil
	}
	ids := make([]string, len(found))
	for index, m := range found {
		ids[index] = m.ID
	}
	sort.Strings(ids)
	return nil, nil, fmt.Errorf("function %q exists in %d modules (%s); choose one with MODULE=", name, len(ids), strings.Join(ids, ", "))
}

// functionArguments parses `<name> [--module=M]` plus any extra flags the
// caller registers on flags.
func functionArguments(flags *flag.FlagSet, args []string) (string, string, error) {
	moduleName := flags.String("module", "", "module id or alias, when the name is ambiguous")
	if err := flags.Parse(args); err != nil {
		return "", "", err
	}
	if flags.NArg() != 1 || flags.Arg(0) == "" {
		return "", "", fmt.Errorf("usage: tools %s [--module=MODULE] FUNCTION", flags.Name())
	}
	return flags.Arg(0), *moduleName, nil
}

// originalFunction resolves a function and returns its module, its original
// bytes and the module image. Those bytes come from the extracted BIN, which
// this extracts first when only the BIN is present.
func (p project) originalFunction(config *projectConfig, name, moduleName string) (*moduleSpec, *functionSpec, []byte, []byte, error) {
	m, f, err := resolveFunction(config, name, moduleName)
	if err != nil {
		return nil, nil, nil, nil, err
	}
	if !p.validExtraction() {
		if _, err := os.Stat(filepath.Join(p.root, targetInput)); err != nil {
			return nil, nil, nil, nil, fmt.Errorf("the original bytes come from %s, which is missing: place it at the repo root (make validate needs no BIN)", targetInput)
		}
		if err := p.extractDisc(); err != nil {
			return nil, nil, nil, nil, err
		}
	}
	image, err := p.readModuleImage(m)
	if err != nil {
		return nil, nil, nil, nil, err
	}
	start := m.offset(f.Addr)
	expected := image[start : start+f.Size]
	if shortHash(expected) != f.Hash {
		return nil, nil, nil, nil, fmt.Errorf("%s: %s hash does not match the original bytes; run `make checksums`", m.ID, f.Name)
	}
	return m, f, expected, image, nil
}

func asmDifferSettings(candidateBinary string) string {
	return fmt.Sprintf(`def apply(config, args):
    config["arch"] = "mipsel"
    config["baseimg"] = "target.bin"
    config["myimg"] = %q
    config["objdump_executable"] = "mipsel-linux-gnu-objdump"
    config["show_line_numbers_default"] = False
`, candidateBinary)
}

// asmDiffer writes both texts to dir and returns asm-differ's listing.
func asmDiffer(dir string, expected, compiled []byte) ([]byte, error) {
	files := map[string][]byte{
		"target.bin":       expected,
		"compiled.bin":     compiled,
		"diff_settings.py": []byte(asmDifferSettings("compiled.bin")),
	}
	for name, data := range files {
		if err := os.WriteFile(filepath.Join(dir, name), data, 0o644); err != nil {
			return nil, err
		}
	}
	command := exec.Command(
		"asm-differ", "--format", "plain", "--no-pager", "--width", "64", "-U", "3",
		"0", fmt.Sprintf("0x%x", max(len(expected), len(compiled))),
	)
	command.Dir = dir
	output, err := command.CombinedOutput()
	if err != nil {
		return nil, fmt.Errorf("asm-differ: %w: %s", err, strings.TrimSpace(string(output)))
	}
	return output, os.WriteFile(filepath.Join(dir, "diff.txt"), output, 0o644)
}

// diffCommand compiles one function as the build does and compares its .text
// and declared .rodata with the original bytes. On a .text difference it
// prints asm-differ's listing, the mismatch-run summary and advisory
// diagnostics for the first differing instruction.
func (p project) diffCommand(args []string) error {
	name, moduleName, err := functionArguments(flag.NewFlagSet("diff", flag.ContinueOnError), args)
	if err != nil {
		return err
	}
	config, err := p.loadProjectConfig()
	if err != nil {
		return err
	}
	m, f, expected, image, err := p.originalFunction(config, name, moduleName)
	if err != nil {
		return err
	}
	workDir := filepath.Join(p.root, "build", "diff", m.ID)
	unlock, err := fileLock(filepath.Join(workDir, f.Name+".lock"))
	if err != nil {
		return err
	}
	defer unlock()
	sections, err := p.compileFunction(newSymbolResolver(config), m, f, workDir)
	if err != nil {
		return fmt.Errorf("compile %s: %w", f.Name, err)
	}
	var problems []string
	compiled := sections[".text"]
	if offset, differs := mismatch(expected, compiled); differs {
		listing, err := asmDiffer(filepath.Join(workDir, f.Name), expected, compiled)
		if err != nil {
			return err
		}
		os.Stdout.Write(listing)
		fmt.Print(formatDiffSummary(expected, compiled))
		relative, _ := filepath.Rel(p.root, filepath.Join(workDir, f.Name, "diff.txt"))
		fmt.Printf("first difference (listing saved to %s):\n", filepath.ToSlash(relative))
		fmt.Print(mismatchDiagnostics(expected, compiled, offset, uint64(f.Addr)))
		problems = append(problems, fmt.Sprintf(".text differs at +0x%x (%d target bytes, %d compiled)", offset, len(expected), len(compiled)))
	}
	rodataBytes := 0
	for _, rodata := range f.Rodata {
		start := m.offset(rodata.Addr)
		want, got := image[start:start+rodata.Size], sections[".rodata"]
		rodataBytes += len(want)
		if offset, differs := mismatch(want, got); differs {
			problems = append(problems, fmt.Sprintf(".rodata 0x%08x differs at +0x%x (%d target bytes, %d compiled; %s)",
				rodata.Addr, offset, len(want), len(got), instructionDifference(want, got, offset)))
		}
	}
	if section := undeclaredSection(f, sections); section != "" {
		problems = append(problems, "emitted undeclared allocatable section "+section)
	}
	if len(problems) > 0 {
		return fmt.Errorf("%s %s does not match: %s", m.ID, f.Name, strings.Join(problems, "; "))
	}
	detail := fmt.Sprintf("%d bytes", f.Size)
	if rodataBytes > 0 {
		detail += fmt.Sprintf(" + %d .rodata", rodataBytes)
	}
	fmt.Printf("matches: %s %s (%s, profile %s)\n", m.ID, f.Name, detail, m.profileOf(f))
	return nil
}

func permuterSettings(functionName string) string {
	return fmt.Sprintf("func_name = %q\ncompiler_type = \"gcc\"\nobjdump_command = \"mipsel-linux-gnu-objdump -drz -m mips:3000\"\n", functionName)
}

// permuteCommand builds a decomp-permuter workspace for one function under
// build/permute/<name> from the build's source, linker bindings and profile,
// checks that the permuter scores its base, and runs it for a bounded time.
func (p project) permuteCommand(args []string) error {
	flags := flag.NewFlagSet("permute", flag.ContinueOnError)
	seconds := flags.Int("duration", 300, "seconds to run")
	defaultJobs := 4
	if value, err := strconv.Atoi(os.Getenv("TOOLS_JOBS")); err == nil {
		defaultJobs = value
	}
	jobs := flags.Int("jobs", defaultJobs, "permuter threads (default TOOLS_JOBS, else 4)")
	name, moduleName, err := functionArguments(flags, args)
	if err != nil {
		return err
	}
	if *seconds < 1 || *seconds > 86400 {
		return errors.New("permuter duration must be between 1 and 86400 seconds")
	}
	if *jobs < 1 || *jobs > 64 {
		return errors.New("permuter jobs must be between 1 and 64")
	}
	config, err := p.loadProjectConfig()
	if err != nil {
		return err
	}
	m, f, expected, _, err := p.originalFunction(config, name, moduleName)
	if err != nil {
		return err
	}
	source, script, err := p.functionLinker(newSymbolResolver(config), m, f)
	if err != nil {
		return err
	}
	if filepath.Ext(source) != ".c" {
		return errors.New("the permuter requires a C source")
	}
	relative := filepath.ToSlash(filepath.Join("build", "permute", f.Name))
	workspace := filepath.Join(p.root, filepath.FromSlash(relative))
	if err := os.RemoveAll(workspace); err != nil {
		return err
	}
	if err := os.MkdirAll(workspace, 0o755); err != nil {
		return err
	}
	targetAssembly := fmt.Sprintf(".set noreorder\n.section .text\n.globl %[1]s\n.type %[1]s, @function\n%[1]s:\n.incbin \"target.bin\"\n.size %[1]s, . - %[1]s\n", f.Name)
	targetLinker := fmt.Sprintf("OUTPUT_ARCH(mips)\nSECTIONS { .text 0x%08x : SUBALIGN(4) { *(.text) } /DISCARD/ : { *(.reginfo) *(.pdr) *(.comment) *(.gnu.attributes) } }\n", f.Addr)
	compileScript := fmt.Sprintf("#!/bin/sh\nset -eu\n/work/build/bin/tools compile-permuter-input %q \"$1\" \"$3\" %q\n", relative+"/function.ld", m.profileOf(f))
	for _, file := range []struct {
		name string
		data []byte
		mode os.FileMode
	}{
		{"target.bin", expected, 0o644},
		{"target.s", []byte(targetAssembly), 0o644},
		{"target.ld", []byte(targetLinker), 0o644},
		{"function.ld", []byte(script), 0o644},
		{"compile.sh", []byte(compileScript), 0o755},
		{"settings.toml", []byte(permuterSettings(f.Name)), 0o644},
	} {
		if err := os.WriteFile(filepath.Join(workspace, file.name), file.data, file.mode); err != nil {
			return err
		}
	}
	if err := runInDir(p.root, "mipsel-linux-gnu-cpp", "-P", "-undef", "-nostdinc", "-Iinclude", source, "-o", filepath.Join(workspace, "base.c")); err != nil {
		return err
	}
	if err := runInDir(workspace, "mipsel-linux-gnu-as", "-EL", "-march=r3000", "-mtune=r3000", "-no-pad-sections", "-O1", "-G0", "-o", "target-raw.o", "target.s"); err != nil {
		return err
	}
	if err := runInDir(workspace, "mipsel-linux-gnu-ld", "-EL", "-T", "target.ld", "-o", "target.o", "target-raw.o"); err != nil {
		return err
	}
	if err := runInDir(workspace, "decomp-permuter", "--debug", "."); err != nil {
		return fmt.Errorf("decomp-permuter could not score the base: %w", err)
	}
	fmt.Printf("permuter workspace: %s (%s %s, %d s, %d jobs)\n", relative, m.ID, f.Name, *seconds, *jobs)
	ctx, cancel := context.WithTimeout(context.Background(), time.Duration(*seconds)*time.Second)
	defer cancel()
	command := exec.CommandContext(ctx, "decomp-permuter", "--better-only", "--best-only", "--stop-on-zero", "-j", strconv.Itoa(*jobs), ".")
	command.Dir, command.Stdout, command.Stderr = workspace, os.Stdout, os.Stderr
	err = command.Run()
	if ctx.Err() == context.DeadlineExceeded {
		fmt.Printf("\npermuter stopped after %d seconds; outputs remain in %s\n", *seconds, relative)
		return nil
	}
	if err != nil {
		return fmt.Errorf("decomp-permuter: %w", err)
	}
	return nil
}

func temporaryPermuterPath(path string, mustExist bool) error {
	clean := filepath.Clean(path)
	temporaryRoot := filepath.Clean(os.TempDir()) + string(filepath.Separator)
	if !filepath.IsAbs(clean) || !strings.HasPrefix(clean, temporaryRoot) {
		return fmt.Errorf("permuter temporary path must be inside %s", os.TempDir())
	}
	if mustExist {
		info, err := os.Stat(clean)
		if err != nil {
			return err
		}
		if !info.Mode().IsRegular() {
			return fmt.Errorf("permuter input is not a regular file: %s", clean)
		}
	}
	return nil
}

// compilePermuterInput is the compile.sh callback of a permuter workspace: it
// compiles one permuted source with the workspace's linker script and profile
// and copies the linked ELF to the permuter's output path.
func (p project) compilePermuterInput(args []string) error {
	if len(args) != 4 {
		return errors.New("usage: tools compile-permuter-input <linker-script> <temporary-source> <temporary-output> <profile>")
	}
	if filepath.Ext(args[1]) != ".c" {
		return errors.New("permuter compilation requires C source")
	}
	linkerPath, err := projectPath(p.root, args[0], "permuter linker script")
	if err != nil {
		return err
	}
	if err := temporaryPermuterPath(args[1], true); err != nil {
		return err
	}
	if err := temporaryPermuterPath(args[2], false); err != nil {
		return err
	}
	profile, ok := compilerProfileNamed(args[3])
	if !ok {
		return fmt.Errorf("unsupported compiler profile %q", args[3])
	}
	parent := filepath.Join(p.root, "build", "permute", ".compile")
	if err := os.MkdirAll(parent, 0o755); err != nil {
		return err
	}
	buildDir, err := os.MkdirTemp(parent, "run-")
	if err != nil {
		return err
	}
	defer os.RemoveAll(buildDir)
	// The permuter needs the linked ELF, which a cached section-only result
	// does not produce.
	if err := os.Setenv("TOOLS_CACHE", "0"); err != nil {
		return err
	}
	if _, err := p.compileHistoricalSections(args[1], linkerPath, buildDir, profile); err != nil {
		return err
	}
	data, err := os.ReadFile(filepath.Join(buildDir, profile.name+".elf"))
	if err != nil {
		return err
	}
	return os.WriteFile(args[2], data, 0o600)
}
