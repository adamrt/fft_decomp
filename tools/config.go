// config.go holds the compiler profiles and small helpers shared by the
// configuration loader and the build.
package main

import (
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
)

var compilerProfiles = map[string]compilerProfile{
	"gcc-2.6.3_O0_aspsx-2.34": {
		name: "gcc-2.6.3_O0_aspsx-2.34", compilerPath: "/opt/old-gcc/2.6.3-psx/cc1",
		optimization: "-O0", aspsxVersion: "2.34",
	},
	// -O1 does not schedule: the prologue `sw ra` stays at +0x4 and delay
	// slots keep source order, as in parts of WORLD and the CARD screen.
	"gcc-2.6.3_O1_aspsx-2.34": {
		name: "gcc-2.6.3_O1_aspsx-2.34", compilerPath: "/opt/old-gcc/2.6.3-psx/cc1",
		optimization: "-O1", aspsxVersion: "2.34",
	},
	"gcc-2.6.3_O2_aspsx-2.34": {
		name: "gcc-2.6.3_O2_aspsx-2.34", compilerPath: "/opt/old-gcc/2.6.3-psx/cc1",
		optimization: "-O2", aspsxVersion: "2.34",
	},
	// _divcheck: ASPSX's divide-by-zero and INT_MIN/-1 traps around each
	// division (maspsx --expand-div). The target mixes checked and unchecked
	// divisions, so this is per function, not global.
	"gcc-2.6.3_O2_aspsx-2.34_divcheck": {
		name: "gcc-2.6.3_O2_aspsx-2.34_divcheck", compilerPath: "/opt/old-gcc/2.6.3-psx/cc1",
		optimization: "-O2", aspsxVersion: "2.34", expandDiv: true,
	},
	// WLDCORE and OPEN were assembled by an ASPSX before 2.30, which expands an
	// indexed global access to `lui/addiu/addu/load 0($at)` instead of folding %lo.
	"gcc-2.6.3_O2_aspsx-2.21": {
		name: "gcc-2.6.3_O2_aspsx-2.21", compilerPath: "/opt/old-gcc/2.6.3-psx/cc1",
		optimization: "-O2", aspsxVersion: "2.21",
	},
	"gcc-2.6.3_O2_aspsx-2.21_divcheck": {
		name: "gcc-2.6.3_O2_aspsx-2.21_divcheck", compilerPath: "/opt/old-gcc/2.6.3-psx/cc1",
		optimization: "-O2", aspsxVersion: "2.21", expandDiv: true,
	},
	// GCC 2.7 (some WLDCORE and OPEN functions): a separate `mfhi` in constant
	// divisions, and no delay-slot fill from past an if-body join label.
	"gcc-2.7.2_O2_aspsx-2.21": {
		name: "gcc-2.7.2_O2_aspsx-2.21", compilerPath: "/opt/old-gcc/2.7.2-psx/cc1",
		optimization: "-O2", aspsxVersion: "2.21",
	},
	"gcc-2.7.2_O2_aspsx-2.21_divcheck": {
		name: "gcc-2.7.2_O2_aspsx-2.21_divcheck", compilerPath: "/opt/old-gcc/2.7.2-psx/cc1",
		optimization: "-O2", aspsxVersion: "2.21", expandDiv: true,
	},
}

func compilerProfileNamed(name string) (compilerProfile, bool) {
	profile, ok := compilerProfiles[name]
	return profile, ok
}

func parseHex(value, field string) (uint64, error) {
	parsed, err := strconv.ParseUint(value, 0, 64)
	if err != nil {
		return 0, fmt.Errorf("invalid %s %q", field, value)
	}
	return parsed, nil
}

func validSymbol(symbol string) bool {
	if symbol == "" {
		return false
	}
	for index, char := range symbol {
		if char == '_' || char >= 'a' && char <= 'z' || char >= 'A' && char <= 'Z' || index > 0 && char >= '0' && char <= '9' {
			continue
		}
		return false
	}
	return true
}

func validModuleID(value string) bool {
	if value == "" {
		return false
	}
	for _, character := range value {
		if character >= 'a' && character <= 'z' || character >= '0' && character <= '9' || character == '-' {
			continue
		}
		return false
	}
	return true
}

func projectPath(root, value, field string) (string, error) {
	if value == "" {
		return "", fmt.Errorf("missing %s", field)
	}
	clean := filepath.Clean(filepath.FromSlash(value))
	if filepath.IsAbs(clean) || clean == ".." || strings.HasPrefix(clean, ".."+string(filepath.Separator)) {
		return "", fmt.Errorf("%s must stay inside the project: %q", field, value)
	}
	path := filepath.Join(root, clean)
	absoluteRoot, err := filepath.Abs(root)
	if err != nil {
		return "", fmt.Errorf("project root: %w", err)
	}
	resolvedRoot, err := filepath.EvalSymlinks(absoluteRoot)
	if err != nil {
		return "", fmt.Errorf("project root: %w", err)
	}
	resolvedPath, err := filepath.EvalSymlinks(filepath.Join(absoluteRoot, clean))
	if err != nil {
		return "", fmt.Errorf("%s %q: %w", field, value, err)
	}
	relative, err := filepath.Rel(resolvedRoot, resolvedPath)
	if err != nil || relative == ".." || strings.HasPrefix(relative, ".."+string(filepath.Separator)) || filepath.IsAbs(relative) {
		return "", fmt.Errorf("%s must stay inside the project after resolving symlinks: %q", field, value)
	}
	if info, err := os.Stat(resolvedPath); err != nil {
		return "", fmt.Errorf("%s %q: %w", field, value, err)
	} else if !info.Mode().IsRegular() {
		return "", fmt.Errorf("%s is not a regular file: %q", field, value)
	}
	// Keep the logical path: source basenames and relative includes depend on it.
	return path, nil
}
