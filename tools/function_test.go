package main

import (
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
)

// requirePinnedToolchain skips a test that needs the image's pinned compiler.
func requirePinnedToolchain(t *testing.T) {
	t.Helper()
	profile, ok := compilerProfileNamed(defaultProfile)
	if !ok {
		t.Fatalf("canonical profile %q is missing", defaultProfile)
	}
	if _, err := os.Stat(profile.compilerPath); err != nil {
		t.Skipf("pinned compiler %s is unavailable: %v", profile.compilerPath, err)
	}
	if _, err := exec.LookPath("mipsel-linux-gnu-cpp"); err != nil {
		t.Skip("mipsel-linux-gnu-cpp is not in PATH")
	}
}

func writeTestFile(t *testing.T, path string, data []byte) {
	t.Helper()
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(path, data, 0o600); err != nil {
		t.Fatal(err)
	}
}

func TestAsmDifferSettingsUsesPSXMIPSEndian(t *testing.T) {
	settings := asmDifferSettings("compiled.bin")
	for _, expected := range []string{`"arch"] = "mipsel"`, `"myimg"] = "compiled.bin"`, "mipsel-linux-gnu-objdump"} {
		if !strings.Contains(settings, expected) {
			t.Fatalf("settings do not contain %q:\n%s", expected, settings)
		}
	}
}

func TestPermuterSettingsUsesR3000LittleEndianObjdump(t *testing.T) {
	settings := permuterSettings("func_801b47e0")
	for _, expected := range []string{`func_name = "func_801b47e0"`, `compiler_type = "gcc"`, `objdump_command = "mipsel-linux-gnu-objdump -drz -m mips:3000"`} {
		if !strings.Contains(settings, expected) {
			t.Fatalf("settings do not contain %q:\n%s", expected, settings)
		}
	}
}

func TestPermuterTemporaryPathRestriction(t *testing.T) {
	path := filepath.Join(t.TempDir(), "input.c")
	writeTestFile(t, path, []byte("void test(void) {}\n"))
	if err := temporaryPermuterPath(path, true); err != nil {
		t.Fatal(err)
	}
	if err := temporaryPermuterPath(filepath.Join(string(filepath.Separator), "outside.c"), false); err == nil {
		t.Fatal("path outside the temporary root was accepted")
	}
}

func TestResolveFunctionByName(t *testing.T) {
	config, err := validTestProject(t).loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	m, f, err := resolveFunction(config, "battle_helper", "")
	if err != nil || m.ID != "battle" || f.Addr != 0x80067000 {
		t.Fatalf("resolveFunction = %v, %v, %v", m, f, err)
	}
	if m, _, err := resolveFunction(config, "attack_entry", "attack"); err != nil || m.ID != "event-attack" {
		t.Fatalf("module alias: %v, %v", m, err)
	}
	for _, test := range []struct{ name, module, want string }{
		{"missing", "", "no function"},
		{"battle_helper", "main", "has no function"},
		{"battle_helper", "unknown", "unknown module"},
	} {
		if _, _, err := resolveFunction(config, test.name, test.module); err == nil || !strings.Contains(err.Error(), test.want) {
			t.Errorf("resolveFunction(%q, %q) error = %v, want %q", test.name, test.module, err, test.want)
		}
	}
	// A name defined by two modules needs MODULE.
	other, _ := config.module("event-card")
	other.Functions = append(other.Functions, functionSpec{Addr: 0x801bf100, Size: 4, Name: "battle_helper"})
	if _, _, err := resolveFunction(config, "battle_helper", ""); err == nil || !strings.Contains(err.Error(), "battle, event-card") {
		t.Fatalf("ambiguous name error = %v", err)
	}
	if m, _, err := resolveFunction(config, "battle_helper", "card"); err != nil || m.ID != "event-card" {
		t.Fatalf("disambiguated: %v, %v", m, err)
	}
}
