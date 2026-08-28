// linker.go resolves the names each function's source uses and writes the
// per-function linker script that places its sections at target addresses.
package main

import (
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"
)

// sourceIdentifiers returns the C identifiers that appear as whole tokens in a
// source file: an over-approximation of the names its object can reference. A
// missing binding fails the link rather than corrupting the output.
func sourceIdentifiers(path string) (map[string]struct{}, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	idents := make(map[string]struct{})
	start := -1
	isWord := func(c byte) bool {
		return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
	}
	for i := 0; i <= len(data); i++ {
		if i < len(data) && isWord(data[i]) {
			if start < 0 {
				start = i
			}
			continue
		}
		if start >= 0 {
			idents[string(data[start:i])] = struct{}{}
			start = -1
		}
	}
	return idents, nil
}

// compilerEmitted reports whether GCC can reference a symbol the source never
// names: `__main` from main()'s prologue, libgcc helpers such as `__divsi3`,
// and `memcpy`/`memset` from block moves and clears.
func compilerEmitted(name string) bool {
	if strings.HasPrefix(name, "__") {
		return true
	}
	switch name {
	case "memcpy", "memset", "memmove":
		return true
	}
	return false
}

// symbolResolver answers which address a name has for a module: the module's
// own functions, data and imports, then each linked module's functions and
// data in order, then main's.
type symbolResolver struct {
	config   *projectConfig
	own      map[string]map[string]uint32
	exported map[string]map[string]uint32
}

func newSymbolResolver(config *projectConfig) symbolResolver {
	resolver := symbolResolver{config: config, own: make(map[string]map[string]uint32), exported: make(map[string]map[string]uint32)}
	for _, m := range config.Modules {
		own := make(map[string]uint32)
		exported := make(map[string]uint32)
		for _, f := range m.Functions {
			own[f.Name], exported[f.Name] = f.Addr, f.Addr
		}
		for _, symbol := range m.Data {
			own[symbol.Name], exported[symbol.Name] = symbol.Addr, symbol.Addr
		}
		for _, symbol := range m.Imports {
			own[symbol.Name] = symbol.Addr
		}
		resolver.own[m.ID], resolver.exported[m.ID] = own, exported
	}
	return resolver
}

func (resolver symbolResolver) resolve(m *moduleSpec, name string) (uint32, bool) {
	if addr, ok := resolver.own[m.ID][name]; ok {
		return addr, true
	}
	for _, link := range m.Links {
		if addr, ok := resolver.exported[link][name]; ok {
			return addr, true
		}
	}
	addr, ok := resolver.exported[mainModuleID][name]
	return addr, ok
}

// linkerBindings lists the PROVIDE bindings one function's link needs: every
// name its source mentions that resolves, compiler-emitted helpers the module
// itself defines, and the function's overrides.
func (resolver symbolResolver) linkerBindings(m *moduleSpec, f *functionSpec, used map[string]struct{}) map[string]uint32 {
	bindings := make(map[string]uint32)
	for name, addr := range resolver.own[m.ID] {
		if compilerEmitted(name) {
			bindings[name] = addr
		}
	}
	for name := range used {
		if addr, ok := resolver.resolve(m, name); ok {
			bindings[name] = addr
		}
	}
	for _, override := range m.Overrides {
		if override.Function == f.Name {
			bindings[override.Name] = override.Addr
		}
	}
	return bindings
}

// linkerScript places .text (and .rodata when declared) at the target
// addresses so the compiled sections are final bytes.
func linkerScript(f *functionSpec, bindings map[string]uint32) (string, error) {
	names := make([]string, 0, len(bindings))
	for name := range bindings {
		names = append(names, name)
	}
	sort.Strings(names)
	var script strings.Builder
	script.WriteString("OUTPUT_ARCH(mips)\n")
	for _, name := range names {
		fmt.Fprintf(&script, "PROVIDE(%s = 0x%08x);\n", name, bindings[name])
	}
	script.WriteString("SECTIONS {\n")
	switch len(f.Rodata) {
	case 0:
	case 1:
		fmt.Fprintf(&script, "  .rodata 0x%x : SUBALIGN(4) { *(.rodata) *(.rdata) }\n", f.Rodata[0].Addr)
	default:
		return "", fmt.Errorf("%s declares more than one .rodata range", f.Name)
	}
	fmt.Fprintf(&script, "  .text 0x%x : SUBALIGN(4) { *(.text) }\n", f.Addr)
	script.WriteString("  /DISCARD/ : { *(.reginfo) *(.pdr) *(.comment) *(.gnu.attributes) }\n}\n")
	return script.String(), nil
}

// writeLinkerScript stores a function's script under buildRoot/<name>.
func writeLinkerScript(buildRoot, name, script string) (string, error) {
	dir := filepath.Join(buildRoot, name)
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return "", err
	}
	path := filepath.Join(dir, "generated.ld")
	if existing, err := os.ReadFile(path); err == nil && string(existing) == script {
		return path, nil
	}
	return path, os.WriteFile(path, []byte(script), 0o644)
}
