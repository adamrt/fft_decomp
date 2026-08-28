package main

import (
	"bytes"
	"errors"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"testing"
)

func writeSymbolRenameFixture(t *testing.T, root, relative, contents string) string {
	t.Helper()
	path := filepath.Join(root, filepath.FromSlash(relative))
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(path, []byte(contents), 0o640); err != nil {
		t.Fatal(err)
	}
	return path
}

func TestSymbolFileTransactionRollsBackPublishedFiles(t *testing.T) {
	root := t.TempDir()
	first, second := filepath.Join(root, "first"), filepath.Join(root, "second")
	if err := os.WriteFile(first, []byte("first-before"), 0o640); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(second, []byte("second-before"), 0o600); err != nil {
		t.Fatal(err)
	}
	transaction := newSymbolFileTransaction()
	if err := addExistingFile(transaction, first, []byte("first-after")); err != nil {
		t.Fatal(err)
	}
	if err := addExistingFile(transaction, second, []byte("second-after")); err != nil {
		t.Fatal(err)
	}
	calls := 0
	err := transaction.commitWithRename(func(oldPath, newPath string) error {
		calls++
		if calls == 3 {
			return errors.New("injected publication failure")
		}
		return os.Rename(oldPath, newPath)
	})
	if err == nil || !strings.Contains(err.Error(), "injected publication failure") {
		t.Fatalf("unexpected error: %v", err)
	}
	for path, want := range map[string]string{first: "first-before", second: "second-before"} {
		got, readErr := os.ReadFile(path)
		if readErr != nil {
			t.Fatal(readErr)
		}
		if string(got) != want {
			t.Fatalf("%s contains %q, want %q", path, got, want)
		}
	}
	entries, err := os.ReadDir(root)
	if err != nil {
		t.Fatal(err)
	}
	if len(entries) != 2 {
		t.Fatalf("transaction left temporary files: %#v", entries)
	}
}

func TestSymbolFileTransactionRejectsStaleInputWithoutWrites(t *testing.T) {
	root := t.TempDir()
	first, second := filepath.Join(root, "first"), filepath.Join(root, "second")
	os.WriteFile(first, []byte("first-before"), 0o600)
	os.WriteFile(second, []byte("second-before"), 0o600)
	transaction := newSymbolFileTransaction()
	if err := addExistingFile(transaction, first, []byte("first-after")); err != nil {
		t.Fatal(err)
	}
	if err := addExistingFile(transaction, second, []byte("second-after")); err != nil {
		t.Fatal(err)
	}
	os.WriteFile(second, []byte("concurrent"), 0o600)
	if err := transaction.commit(); err == nil || !strings.Contains(err.Error(), "changed on disk") {
		t.Fatalf("unexpected error: %v", err)
	}
	firstBody, _ := os.ReadFile(first)
	secondBody, _ := os.ReadFile(second)
	if string(firstBody) != "first-before" || string(secondBody) != "concurrent" {
		t.Fatalf("preflight made partial writes: %q, %q", firstBody, secondBody)
	}
}

func TestSymbolFileTransactionRollsBackMoveAndCreation(t *testing.T) {
	root := t.TempDir()
	config := writeSymbolRenameFixture(t, root, "a-config", "config-before")
	destination := filepath.Join(root, "b-destination")
	source := writeSymbolRenameFixture(t, root, "c-source", "source-before")
	transaction := newSymbolFileTransaction()
	if err := addExistingFile(transaction, config, []byte("config-after")); err != nil {
		t.Fatal(err)
	}
	if err := transaction.addCreation(destination, []byte("moved-source"), 0o640); err != nil {
		t.Fatal(err)
	}
	if err := addRemovedFile(transaction, source); err != nil {
		t.Fatal(err)
	}
	calls := 0
	err := transaction.commitWithRename(func(oldPath, newPath string) error {
		calls++
		if calls == 4 {
			return errors.New("injected move failure")
		}
		return os.Rename(oldPath, newPath)
	})
	if err == nil || !strings.Contains(err.Error(), "injected move failure") {
		t.Fatalf("unexpected error: %v", err)
	}
	for path, want := range map[string]string{config: "config-before", source: "source-before"} {
		body, readErr := os.ReadFile(path)
		if readErr != nil || string(body) != want {
			t.Fatalf("rollback left %s as %q (%v), want %q", path, body, readErr, want)
		}
	}
	if _, err := os.Stat(destination); !errors.Is(err, os.ErrNotExist) {
		t.Fatalf("rollback left destination behind: %v", err)
	}
}

func TestSourceRenameRejectsLinkerCollision(t *testing.T) {
	_, err := renameLinkerIdentifier([]byte("old = 0x10;\nnew = 0x20;\n"), "old", "new")
	if err == nil || !strings.Contains(err.Error(), "collision") {
		t.Fatalf("unexpected error: %v", err)
	}
	output, err := renameLinkerIdentifier([]byte("old = 0x10;\nnew = 0x10;\nuse = old;\n"), "old", "new")
	if err != nil {
		t.Fatal(err)
	}
	if bytes.Count(output, []byte("new = 0x10;")) != 1 || bytes.Contains(output, []byte("old")) {
		t.Fatalf("equal alias was not folded cleanly:\n%s", output)
	}
}

func TestPlanSourceIdentifierRenamesSkipsIncludesAndCoversAssembly(t *testing.T) {
	root := t.TempDir()
	for path, body := range map[string]string{
		"src/main/caller.c":   "#include \"fft/main_startup.h\"\nvoid caller(void) { main_startup(); }\n",
		"src/main/restore.s":  "\tj\tmain_startup\n",
		"include/fft/extra.h": "#include \"fft/main_startup.h\"\nvoid main_startup(void);\n",
	} {
		full := filepath.Join(root, filepath.FromSlash(path))
		if err := os.MkdirAll(filepath.Dir(full), 0o755); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(full, []byte(body), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	plan, err := planSourceIdentifierRenames(root, []sourceIdentifierRename{{"main_startup", "main_boot_run_startup"}})
	if err != nil {
		t.Fatal(err)
	}
	for path, want := range map[string]string{
		"src/main/caller.c":   "#include \"fft/main_startup.h\"\nvoid caller(void) { main_boot_run_startup(); }\n",
		"src/main/restore.s":  "\tj\tmain_boot_run_startup\n",
		"include/fft/extra.h": "#include \"fft/main_startup.h\"\nvoid main_boot_run_startup(void);\n",
	} {
		change, ok := plan.changes[filepath.Join(root, filepath.FromSlash(path))]
		if !ok || string(change.replacement) != want {
			t.Fatalf("%s = %q, want %q", path, change.replacement, want)
		}
	}
}

func TestReplaceIdentifierOutsideIncludesSkipsCommentsAndLiterals(t *testing.T) {
	source := []byte("#include \"fft/load.h\"\n/* the load-game overlay */\n// load here\nchar *s = \"load\";\nint load(void);\nload();\n")
	want := []byte("#include \"fft/load.h\"\n/* the load-game overlay */\n// load here\nchar *s = \"load\";\nint main_file_build_header_nnl(void);\nmain_file_build_header_nnl();\n")
	got := replaceIdentifierOutsideIncludes(regexp.MustCompile(`\bload\b`), source, []byte("main_file_build_header_nnl"))
	if !bytes.Equal(got, want) {
		t.Fatalf("got:\n%s\nwant:\n%s", got, want)
	}
}

// addExistingFile and addRemovedFile snapshot a file as renameSymbols does.
func addExistingFile(transaction *symbolFileTransaction, path string, replacement []byte) error {
	info, original, err := readRegularSymbolFile(path)
	if err != nil {
		return err
	}
	return transaction.addExistingSnapshot(path, original, replacement, info.Mode())
}

func addRemovedFile(transaction *symbolFileTransaction, path string) error {
	info, original, err := readRegularSymbolFile(path)
	if err != nil {
		return err
	}
	return transaction.addRemovalSnapshot(path, original, info.Mode())
}
