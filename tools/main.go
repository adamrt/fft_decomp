// tools owns reproducible target-disc and binary-analysis operations.
package main

import (
	"bytes"
	"crypto/sha256"
	"debug/elf"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"encoding/xml"
	"errors"
	"fmt"
	"hash"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"sort"
	"strconv"
	"strings"
	"sync"
	"time"
)

const expectedISOFileCount = 2464

// A bind-mounted source can briefly appear half-written (Docker Desktop), so
// a failed compile is retried a few times before it is reported.
const historicalCompileRetries = 3
const historicalCompileRetryDelay = 25 * time.Millisecond

type project struct {
	root          string
	extractRoot   string
	filesRoot     string
	xmlPath       string
	inventoryPath string
	// out receives module build progress; nil means os.Stdout. buildDisc
	// gives each concurrently built module its own buffer so the log keeps
	// module order.
	out io.Writer
	// inputRoot, when set, is a container-local mirror of src/ and include/
	// that module builds preprocess from (see withInputMirror).
	inputRoot string
	// progress, when set, counts compiled functions across modules.
	progress *compileProgress
}

// preprocessInput maps a source under the project root to its mirrored copy
// and returns the directory cpp must run in so -Iinclude resolves there too.
func (p project) preprocessInput(sourcePath string) (string, string) {
	if p.inputRoot == "" {
		return p.root, sourcePath
	}
	relative, err := filepath.Rel(p.root, sourcePath)
	if err != nil || strings.HasPrefix(relative, "..") {
		return p.root, sourcePath
	}
	return p.inputRoot, filepath.Join(p.inputRoot, relative)
}

// toolWorkDir is the directory for per-function intermediates and the input
// mirror: TOOLS_WORK_DIR, or /tmp/fft-build inside the container when it is
// unset. An explicitly empty TOOLS_WORK_DIR keeps everything under build/.
func toolWorkDir() string {
	if value, ok := os.LookupEnv("TOOLS_WORK_DIR"); ok {
		return value
	}
	return "/tmp/fft-build"
}

// withInputMirror syncs src/ and include/ into the work directory's _input and
// returns a project that preprocesses from there. Reading the ~20 headers
// each C file includes through the host bind mount costs about ten times the
// preprocessing itself; one incremental sync per build avoids that.
func (p project) withInputMirror() (project, error) {
	base := toolWorkDir()
	if base == "" {
		return p, nil
	}
	mirror := filepath.Join(base, "_input")
	for _, dir := range []string{"src", "include"} {
		if err := syncTree(filepath.Join(p.root, dir), filepath.Join(mirror, dir)); err != nil {
			return p, fmt.Errorf("mirror %s: %w", dir, err)
		}
	}
	p.inputRoot = mirror
	return p, nil
}

// syncTree makes destination an exact copy of source: files whose size or
// modification time differ are copied (keeping the source time), and files
// or directories absent from source are removed.
func syncTree(source, destination string) error {
	seen := make(map[string]bool)
	err := filepath.WalkDir(source, func(path string, entry os.DirEntry, err error) error {
		if err != nil {
			return err
		}
		relative, err := filepath.Rel(source, path)
		if err != nil {
			return err
		}
		seen[relative] = true
		target := filepath.Join(destination, relative)
		if entry.IsDir() {
			return os.MkdirAll(target, 0o755)
		}
		info, err := entry.Info()
		if err != nil {
			return err
		}
		if existing, err := os.Stat(target); err == nil && existing.Size() == info.Size() && existing.ModTime().Equal(info.ModTime()) {
			return nil
		}
		data, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		if err := os.WriteFile(target, data, 0o644); err != nil {
			return err
		}
		return os.Chtimes(target, info.ModTime(), info.ModTime())
	})
	if err != nil {
		return err
	}
	var stale []string
	err = filepath.WalkDir(destination, func(path string, entry os.DirEntry, err error) error {
		if err != nil {
			return err
		}
		relative, err := filepath.Rel(destination, path)
		if err != nil {
			return err
		}
		if !seen[relative] {
			stale = append(stale, path)
			if entry.IsDir() {
				return filepath.SkipDir
			}
		}
		return nil
	})
	if err != nil {
		return err
	}
	for _, path := range stale {
		if err := os.RemoveAll(path); err != nil {
			return err
		}
	}
	return nil
}

func (p project) stdout() io.Writer {
	if p.out != nil {
		return p.out
	}
	return os.Stdout
}

type isoProject struct {
	Tracks []isoTrack `xml:"track"`
}

type isoTrack struct {
	Directory isoDirectory `xml:"directory_tree"`
}

type isoDirectory struct {
	Files       []isoFile      `xml:"file"`
	Directories []isoDirectory `xml:"dir"`
}

type isoFile struct {
	Source string `xml:"source,attr"`
	LBA    string `xml:"offs,attr"`
	Type   string `xml:"type,attr"`
	Date   string `xml:"date,attr"`
}

type inventory struct {
	SchemaVersion int               `json:"schema_version"`
	TargetID      string            `json:"target_id"`
	Source        string            `json:"source"`
	FileCount     int               `json:"file_count"`
	Files         []inventoryRecord `json:"files"`
}

type inventoryRecord struct {
	Path         string `json:"path"`
	Bytes        int64  `json:"bytes"`
	SHA256       string `json:"sha256"`
	LBA          int    `json:"lba,omitempty"`
	Type         string `json:"type,omitempty"`
	Date         string `json:"date,omitempty"`
	Format       string `json:"format,omitempty"`
	EntryPoint   string `json:"entry_point,omitempty"`
	InitialGP    string `json:"initial_gp,omitempty"`
	TextAddress  string `json:"text_address,omitempty"`
	TextSize     int    `json:"text_size,omitempty"`
	DataAddress  string `json:"data_address,omitempty"`
	DataSize     int    `json:"data_size,omitempty"`
	BSSAddress   string `json:"bss_address,omitempty"`
	BSSSize      int    `json:"bss_size,omitempty"`
	StackAddress string `json:"stack_address,omitempty"`
	StackSize    int    `json:"stack_size,omitempty"`
}

const usage = "usage: tools build [module|disc]|validate [--module=M]|diff [--module=M] FUNC|permute [--module=M] [--duration=S] [--jobs=N] FUNC|checksums|check-config|config-fmt|declarations ...|symbols <action> ...|extract"

func main() {
	if len(os.Args) < 2 {
		fatalf(usage)
	}
	commandsWithArguments := map[string]bool{
		"build":                  true,
		"validate":               true,
		"diff":                   true,
		"permute":                true,
		"symbols":                true,
		"declarations":           true,
		"compile-permuter-input": true,
	}
	if !commandsWithArguments[os.Args[1]] && len(os.Args) != 2 {
		fatalf("usage: tools %s", os.Args[1])
	}
	p, err := findProject()
	if err != nil {
		fatalf("%v", err)
	}

	switch os.Args[1] {
	case "build":
		err = p.buildCommand(os.Args[2:])
	case "validate":
		err = p.validateCommand(os.Args[2:])
	case "checksums":
		err = p.checksumsCommand(nil)
	case "check-config":
		err = p.checkConfig()
	case "config-fmt":
		err = p.configFormat()
	case "diff":
		err = p.diffCommand(os.Args[2:])
	case "permute":
		err = p.permuteCommand(os.Args[2:])
	case "symbols":
		err = p.symbolsCommand(os.Args[2:])
	case "declarations":
		err = p.declarationsCommand(os.Args[2:])
	case "extract":
		err = p.extractDisc()
	case "analyze":
		err = p.buildAnalysis()
	case "compile-permuter-input":
		err = p.compilePermuterInput(os.Args[2:])
	default:
		fatalf("unknown command %q; %s", os.Args[1], usage)
	}
	if err != nil {
		fatalf("%v", err)
	}
}

func fatalf(format string, args ...any) {
	fmt.Fprintf(os.Stderr, "tools: "+format+"\n", args...)
	os.Exit(1)
}

// findProject walks up to the directory holding target/main.yaml.
func findProject() (project, error) {
	dir, err := os.Getwd()
	if err != nil {
		return project{}, err
	}
	for {
		if _, err := os.Stat(filepath.Join(dir, modulesConfigDir, mainModuleID+".yaml")); err == nil {
			extractRoot := filepath.Join(dir, "build", "extracted")
			return project{
				root:          dir,
				extractRoot:   extractRoot,
				filesRoot:     filepath.Join(extractRoot, "files"),
				xmlPath:       filepath.Join(extractRoot, "disc.xml"),
				inventoryPath: filepath.Join(extractRoot, "inventory.json"),
			}, nil
		}
		parent := filepath.Dir(dir)
		if parent == dir {
			return project{}, errors.New("could not find target/main.yaml")
		}
		dir = parent
	}
}

func digest(h hash.Hash) string {
	return hex.EncodeToString(h.Sum(nil))
}

func sha256File(path string) (string, error) {
	f, err := os.Open(path)
	if err != nil {
		return "", err
	}
	defer f.Close()
	h := sha256.New()
	if _, err := io.Copy(h, f); err != nil {
		return "", err
	}
	return digest(h), nil
}

var errNotExtracted = errors.New("the original disc is not extracted: place scus-94221.bin at the repo root and run `make extract`")

// verifyDisc checks the user-supplied BIN against the supported target.
func (p project) verifyDisc() (string, error) {
	image := filepath.Join(p.root, targetInput)
	info, err := os.Stat(image)
	if err != nil {
		return "", fmt.Errorf("missing %s at the repo root (make validate works without it)", targetInput)
	}
	fmt.Printf("disc: %s\nbytes: %d\n", targetInput, info.Size())
	if info.Size() != targetInputBytes {
		return "", fmt.Errorf("size mismatch: expected %d, got %d", targetInputBytes, info.Size())
	}
	sum, err := sha256File(image)
	if err != nil {
		return "", err
	}
	if sum != targetInputSHA256 {
		return "", fmt.Errorf("sha256 mismatch: expected %s, got %s", targetInputSHA256, sum)
	}
	fmt.Printf("sha256: %s [ok]\n", sum)
	return image, nil
}

func readISOProject(path string) (isoProject, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return isoProject{}, err
	}
	var project isoProject
	return project, xml.Unmarshal(data, &project)
}

func flattenFiles(dir isoDirectory, output *[]isoFile) {
	*output = append(*output, dir.Files...)
	for _, child := range dir.Directories {
		flattenFiles(child, output)
	}
}

func (p project) validExtraction() bool {
	if _, err := os.Stat(filepath.Join(p.filesRoot, "SCUS_942.21")); err != nil {
		return false
	}
	project, err := readISOProject(p.xmlPath)
	if err != nil {
		return false
	}
	var files []isoFile
	for _, track := range project.Tracks {
		flattenFiles(track.Directory, &files)
	}
	return len(files) == expectedISOFileCount
}

// extractionStamp records the BIN's size and modification time after a
// verified extraction, so later calls skip re-hashing 541 MB.
func (p project) extractionStamp() (string, error) {
	info, err := os.Stat(filepath.Join(p.root, targetInput))
	if err != nil {
		return "", err
	}
	return fmt.Sprintf("%s %d %d\n", targetInputSHA256, info.Size(), info.ModTime().UnixNano()), nil
}

func (p project) extractDisc() error {
	stampPath := filepath.Join(p.extractRoot, "source.stamp")
	stamp, err := p.extractionStamp()
	if err == nil && p.validExtraction() {
		if recorded, err := os.ReadFile(stampPath); err == nil && string(recorded) == stamp {
			return nil
		}
	}
	image, err := p.verifyDisc()
	if err != nil {
		return err
	}
	if p.validExtraction() {
		fmt.Printf("extraction already complete: %s\n", filepath.Join("build", "extracted"))
		if err := p.buildInventory(); err != nil {
			return err
		}
		return atomicWrite(stampPath, []byte(stamp))
	}
	if _, err := os.Stat(p.extractRoot); err == nil {
		return fmt.Errorf("incomplete extraction exists at build/extracted; move or remove it before retrying")
	} else if !os.IsNotExist(err) {
		return err
	}
	if err := os.MkdirAll(p.filesRoot, 0o755); err != nil {
		return err
	}
	cmd := exec.Command("dumpsxiso", dumpsxisoArgs(p.filesRoot, p.xmlPath, image)...)
	cmd.Dir, cmd.Stdout, cmd.Stderr = p.root, os.Stdout, os.Stderr
	if err := cmd.Run(); err != nil {
		if cleanupErr := os.RemoveAll(p.extractRoot); cleanupErr != nil {
			return fmt.Errorf("dumpsxiso: %w; cleanup failed: %v", err, cleanupErr)
		}
		return fmt.Errorf("dumpsxiso: %w", err)
	}
	if !p.validExtraction() {
		if err := os.RemoveAll(p.extractRoot); err != nil {
			return fmt.Errorf("dumpsxiso finished without producing the expected extraction; cleanup failed: %w", err)
		}
		return errors.New("dumpsxiso finished without producing the expected extraction")
	}
	fmt.Printf("extracted %d ISO files to build/extracted/files\n", expectedISOFileCount)
	if err := p.buildInventory(); err != nil {
		return err
	}
	return atomicWrite(stampPath, []byte(stamp))
}

func dumpsxisoArgs(filesRoot, xmlPath, input string) []string {
	return []string{"--quiet", "--lba", "-x", filesRoot, "-s", xmlPath, input}
}

func psxEXEMetadata(path string) (inventoryRecord, bool, error) {
	f, err := os.Open(path)
	if err != nil {
		return inventoryRecord{}, false, err
	}
	defer f.Close()
	header := make([]byte, 0x38)
	if _, err := io.ReadFull(f, header); err != nil {
		if errors.Is(err, io.EOF) || errors.Is(err, io.ErrUnexpectedEOF) {
			return inventoryRecord{}, false, nil
		}
		return inventoryRecord{}, false, err
	}
	if string(header[:8]) != "PS-X EXE" {
		return inventoryRecord{}, false, nil
	}
	word := func(offset int) uint32 { return binary.LittleEndian.Uint32(header[offset : offset+4]) }
	hex32 := func(value uint32) string { return fmt.Sprintf("0x%08x", value) }
	return inventoryRecord{
		Format: "ps-x-exe", EntryPoint: hex32(word(0x10)), InitialGP: hex32(word(0x14)),
		TextAddress: hex32(word(0x18)), TextSize: int(word(0x1c)),
		DataAddress: hex32(word(0x20)), DataSize: int(word(0x24)),
		BSSAddress: hex32(word(0x28)), BSSSize: int(word(0x2c)),
		StackAddress: hex32(word(0x30)), StackSize: int(word(0x34)),
	}, true, nil
}

func (p project) buildInventory() error {
	if !p.validExtraction() {
		return errNotExtracted
	}
	projectXML, err := readISOProject(p.xmlPath)
	if err != nil {
		return err
	}
	var files []isoFile
	for _, track := range projectXML.Tracks {
		flattenFiles(track.Directory, &files)
	}
	records := make([]inventoryRecord, 0, len(files))
	for _, file := range files {
		path, relative, err := extractionSourcePath(p.filesRoot, p.extractRoot, file.Source)
		if err != nil {
			return fmt.Errorf("disc.xml source %q: %w", file.Source, err)
		}
		info, err := os.Stat(path)
		if err != nil {
			return fmt.Errorf("disc.xml source %q: %w", file.Source, err)
		}
		sum, err := sha256File(path)
		if err != nil {
			return err
		}
		record := inventoryRecord{Path: filepath.ToSlash(relative), Bytes: info.Size(), SHA256: sum, Type: file.Type, Date: file.Date}
		if file.LBA != "" {
			record.LBA, err = strconv.Atoi(file.LBA)
			if err != nil {
				return fmt.Errorf("invalid LBA %q for %s", file.LBA, file.Source)
			}
		}
		if metadata, ok, err := psxEXEMetadata(path); err != nil {
			return err
		} else if ok {
			metadata.Path, metadata.Bytes, metadata.SHA256 = record.Path, record.Bytes, record.SHA256
			metadata.LBA, metadata.Type, metadata.Date = record.LBA, record.Type, record.Date
			record = metadata
		}
		records = append(records, record)
	}
	payload := inventory{SchemaVersion: 1, TargetID: targetID, Source: "build/extracted/disc.xml", FileCount: len(records), Files: records}
	data, err := json.MarshalIndent(payload, "", "  ")
	if err != nil {
		return err
	}
	data = append(data, '\n')
	if err := os.WriteFile(p.inventoryPath, data, 0o644); err != nil {
		return err
	}
	fmt.Printf("wrote %d records to build/extracted/inventory.json\n", len(records))
	return p.buildAnalysis()
}

func extractionSourcePath(filesRoot, extractRoot, source string) (string, string, error) {
	path := filepath.FromSlash(source)
	if !filepath.IsAbs(path) {
		path = filepath.Join(extractRoot, path)
	}
	relative, err := filepath.Rel(filesRoot, path)
	if err != nil {
		return "", "", err
	}
	if relative == ".." || strings.HasPrefix(relative, ".."+string(filepath.Separator)) || filepath.IsAbs(relative) {
		return "", "", errors.New("path is outside the extracted files directory")
	}
	return path, relative, nil
}

type compilerProfile struct {
	name         string
	compilerPath string
	aspsxVersion string
	optimization string
	// expandDiv makes maspsx emit ASPSX's checked division sequence (a
	// divide-by-zero trap plus the INT_MIN/-1 overflow trap) around each
	// division, instead of a bare div/mflo pair.
	expandDiv bool
}

func runInDir(dir string, arguments ...string) error {
	cmd := exec.Command(arguments[0], arguments[1:]...)
	cmd.Dir, cmd.Stdout, cmd.Stderr = dir, os.Stdout, os.Stderr
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("%s: %w", arguments[0], err)
	}
	return nil
}

func transformFile(dir, input, output string, arguments ...string) error {
	in, err := os.Open(input)
	if err != nil {
		return err
	}
	defer in.Close()
	out, err := os.Create(output)
	if err != nil {
		return err
	}
	cmd := exec.Command(arguments[0], arguments[1:]...)
	cmd.Dir, cmd.Stdin, cmd.Stdout, cmd.Stderr = dir, in, out, os.Stderr
	if err := cmd.Run(); err != nil {
		out.Close()
		return fmt.Errorf("%s: %w", arguments[0], err)
	}
	return out.Close()
}

func mismatch(expected, actual []byte) (int, bool) {
	limit := min(len(expected), len(actual))
	for offset := range limit {
		if expected[offset] != actual[offset] {
			return offset, true
		}
	}
	if len(expected) != len(actual) {
		return limit, true
	}
	return 0, false
}

func instructionDifference(expected, actual []byte, offset int) string {
	instructionOffset := offset &^ 3
	wordAt := func(code []byte) string {
		if instructionOffset+4 > len(code) {
			return "<truncated>"
		}
		return fmt.Sprintf("0x%08x", binary.LittleEndian.Uint32(code[instructionOffset:]))
	}
	return fmt.Sprintf("instruction +0x%x: expected %s, got %s", instructionOffset, wordAt(expected), wordAt(actual))
}

// cc1Command runs an old cc1, a static i386 executable: natively on x86
// hosts, under qemu-i386 elsewhere (such as Apple Silicon).
func cc1Command(compilerPath string) []string {
	if runtime.GOARCH == "amd64" || runtime.GOARCH == "386" {
		return []string{compilerPath}
	}
	return []string{"qemu-i386", compilerPath}
}

// repreprocess redoes the preprocessing step into the same destination, for the
// retries after a compile failure that a concurrent source write can cause.
func (p project) repreprocess(sourcePath, preprocessed string) error {
	staging := preprocessed + ".staging"
	cppDir, cppSource := p.preprocessInput(sourcePath)
	if err := runInDir(cppDir, "mipsel-linux-gnu-cpp", "-P", "-undef", "-nostdinc", "-Iinclude",
		cppSource, "-o", staging); err != nil {
		return err
	}
	info, err := os.Stat(staging)
	if err != nil {
		return err
	}
	if info.Size() == 0 {
		return fmt.Errorf("preprocessing %s produced no output", sourcePath)
	}
	return os.Rename(staging, preprocessed)
}

func (p project) compileHistoricalSections(sourcePath, linkPath, buildDir string, profile compilerProfile) (map[string][]byte, error) {
	if err := os.MkdirAll(buildDir, 0o755); err != nil {
		return nil, err
	}
	assembly := filepath.Join(buildDir, profile.name+".s")
	preprocessed := filepath.Join(buildDir, profile.name+".i")
	translated := filepath.Join(buildDir, profile.name+".maspsx.s")
	object := filepath.Join(buildDir, profile.name+".o")
	linkedPath := filepath.Join(buildDir, profile.name+".elf")
	binary := filepath.Join(buildDir, profile.name+".bin")
	language, err := sourceLanguage(sourcePath)
	if err != nil {
		return nil, err
	}
	if language == "asm" {
		data, err := os.ReadFile(sourcePath)
		if err != nil {
			return nil, err
		}
		if err := validateStandaloneAssembly(data); err != nil {
			return nil, err
		}
		if err := os.WriteFile(preprocessed, data, 0o644); err != nil {
			return nil, err
		}
	} else {
		// Preprocess to a temporary and rename: the cache key is hashed from
		// this file, so no reader may see a partial write.
		staging := preprocessed + ".staging"
		cppDir, cppSource := p.preprocessInput(sourcePath)
		cpp := []string{
			"mipsel-linux-gnu-cpp", "-P", "-undef", "-nostdinc", "-Iinclude",
			cppSource, "-o", staging,
		}
		if err := runInDir(cppDir, cpp...); err != nil {
			return nil, err
		}
		info, err := os.Stat(staging)
		if err != nil {
			return nil, err
		}
		if info.Size() == 0 {
			return nil, fmt.Errorf("preprocessing %s produced no output", sourcePath)
		}
		if err := os.Rename(staging, preprocessed); err != nil {
			return nil, err
		}
	}
	cacheKey, err := compilationCacheKey(preprocessed, linkPath, profile, language)
	if err != nil {
		return nil, err
	}
	cachePath := filepath.Join(p.root, "build", "cache", "tools", cacheKey+".json")
	if os.Getenv("TOOLS_CACHE") != "0" {
		if data, err := os.ReadFile(cachePath); err == nil {
			var sections map[string][]byte
			if json.Unmarshal(data, &sections) == nil && sections[".text"] != nil {
				return sections, nil
			}
		}
	}
	if language == "asm" {
		assembly = preprocessed
	} else {
		cc1 := append(cc1Command(profile.compilerPath), preprocessed, "-o", assembly,
			"-G0", "-w", "-funsigned-char", "-fpeephole", "-ffunction-cse",
			"-fpcc-struct-return", "-fcommon", "-fverbose-asm", "-msoft-float",
			"-quiet", "-mcpu=3000", "-fgnu-linker", "-mgas", "-gcoff", profile.optimization,
		)
		if compileErr := runInDir(p.root, cc1...); compileErr != nil {
			// A source read mid-write preprocesses short and fails to parse:
			// re-preprocess and retry briefly before reporting the error.
			for retry := 0; retry < historicalCompileRetries; retry++ {
				time.Sleep(historicalCompileRetryDelay)
				if retryErr := p.repreprocess(sourcePath, preprocessed); retryErr != nil {
					return nil, errors.Join(compileErr, retryErr)
				}
				if compileErr = runInDir(p.root, cc1...); compileErr == nil {
					break
				}
			}
			if compileErr != nil {
				return nil, compileErr
			}
		}
	}
	maspsx := []string{"python3", "/opt/maspsx/maspsx.py", "--aspsx-version=" + profile.aspsxVersion}
	if profile.expandDiv {
		maspsx = append(maspsx, "--expand-div")
	}
	if err := transformFile(p.root, assembly, translated, maspsx...); err != nil {
		return nil, err
	}
	if err := runInDir(p.root, "mipsel-linux-gnu-as", "-EL", "-march=r3000", "-mtune=r3000", "-no-pad-sections", "-O1", "-G0", "-o", object, translated); err != nil {
		return nil, err
	}
	if err := runInDir(p.root, "mipsel-linux-gnu-ld", "-EL", "-T", linkPath, "-o", linkedPath, object); err != nil {
		return nil, err
	}
	if err := runInDir(p.root, "mipsel-linux-gnu-objcopy", "-O", "binary", "--only-section=.text", linkedPath, binary); err != nil {
		return nil, err
	}
	text, err := os.ReadFile(binary)
	if err != nil {
		return nil, err
	}
	sections := map[string][]byte{".text": text}
	linked, err := elf.Open(linkedPath)
	if err != nil {
		return nil, err
	}
	defer linked.Close()
	for _, section := range linked.Sections {
		if section.Size == 0 || section.Flags&elf.SHF_ALLOC == 0 || section.Name == ".text" || section.Name == ".MIPS.abiflags" {
			continue
		}
		if section.Type == elf.SHT_NOBITS {
			sections[section.Name] = nil
			continue
		}
		data, err := section.Data()
		if err != nil {
			return nil, fmt.Errorf("read linked section %s: %w", section.Name, err)
		}
		sections[section.Name] = data
	}
	if os.Getenv("TOOLS_CACHE") != "0" {
		if err := os.MkdirAll(filepath.Dir(cachePath), 0o755); err != nil {
			return nil, err
		}
		data, err := json.Marshal(sections)
		if err != nil {
			return nil, err
		}
		// Concurrent workers compile the same preprocessed source, so they
		// share this path; an in-place write can be read half-finished.
		temporary, err := os.CreateTemp(filepath.Dir(cachePath), ".cache-")
		if err != nil {
			return nil, err
		}
		if _, err := temporary.Write(data); err != nil {
			temporary.Close()
			os.Remove(temporary.Name())
			return nil, err
		}
		if err := temporary.Close(); err != nil {
			os.Remove(temporary.Name())
			return nil, err
		}
		if err := os.Rename(temporary.Name(), cachePath); err != nil {
			os.Remove(temporary.Name())
			return nil, err
		}
	}
	return sections, nil
}

var toolDigestCache sync.Map

func toolDigest(path string) (string, error) {
	if value, ok := toolDigestCache.Load(path); ok {
		return value.(string), nil
	}
	digest, err := sha256File(path)
	if err != nil {
		return "", err
	}
	toolDigestCache.Store(path, digest)
	return digest, nil
}

var toolchainDigestOnce = sync.OnceValues(func() (string, error) {
	paths, err := filepath.Glob("/opt/maspsx/maspsx/*.py")
	if err != nil {
		return "", err
	}
	sort.Strings(paths)
	for _, name := range []string{"mipsel-linux-gnu-as", "mipsel-linux-gnu-ld", "mipsel-linux-gnu-objcopy"} {
		path, err := exec.LookPath(name)
		if err != nil {
			return "", err
		}
		paths = append(paths, path)
	}
	hash := sha256.New()
	for _, path := range paths {
		digest, err := toolDigest(path)
		if err != nil {
			return "", err
		}
		hash.Write([]byte(path + "\x00" + digest + "\x00"))
	}
	return hex.EncodeToString(hash.Sum(nil)), nil
})

func toolchainDigest() (string, error) {
	return toolchainDigestOnce()
}

func compilationCacheKey(preprocessed, linkPath string, profile compilerProfile, language string) (string, error) {
	hash := sha256.New()
	if language == "asm" {
		hash.Write([]byte("standalone-asm-v1\x00"))
	}
	hash.Write([]byte("tools-historical-pipeline-v1\x00" + profile.name + "\x00" + profile.optimization + "\x00" + profile.aspsxVersion + "\x00"))
	if profile.expandDiv {
		hash.Write([]byte("expand-div-v1\x00"))
	}
	for _, path := range []string{profile.compilerPath, "/opt/maspsx/maspsx.py"} {
		digest, err := toolDigest(path)
		if err != nil {
			return "", err
		}
		hash.Write([]byte(digest))
	}
	// maspsx's package, the assembler and the linker also shape the bytes.
	toolchain, err := toolchainDigest()
	if err != nil {
		return "", err
	}
	hash.Write([]byte("toolchain-v2\x00" + toolchain))
	for _, path := range []string{preprocessed, linkPath} {
		data, err := os.ReadFile(path)
		if err != nil {
			return "", err
		}
		hash.Write(data)
	}
	return hex.EncodeToString(hash.Sum(nil)), nil
}

func sourceLanguage(path string) (string, error) {
	switch filepath.Ext(path) {
	case ".c":
		return "C", nil
	case ".s":
		return "asm", nil
	default:
		return "", fmt.Errorf("unsupported source extension: %s", path)
	}
}

func validateStandaloneAssembly(data []byte) error {
	// No embedded media or unhashed include dependencies in standalone sources.
	for _, line := range strings.Split(strings.ToLower(string(data)), "\n") {
		for _, directive := range []string{".include", ".incbin"} {
			if strings.Contains(line, directive) {
				return fmt.Errorf("standalone assembly does not support %s", directive)
			}
		}
	}
	return nil
}

func xmlAttribute(value string) string {
	var escaped bytes.Buffer
	_ = xml.EscapeText(&escaped, []byte(value))
	return escaped.String()
}

func readMode2File(imagePath string, lba, size int) ([]byte, error) {
	const rawSectorSize, dataOffset, dataSize = 2352, 24, 2048
	image, err := os.Open(imagePath)
	if err != nil {
		return nil, err
	}
	defer image.Close()
	output := make([]byte, size)
	for offset := 0; offset < size; offset += dataSize {
		chunk := min(dataSize, size-offset)
		position := int64((lba+offset/dataSize)*rawSectorSize + dataOffset)
		if _, err := image.ReadAt(output[offset:offset+chunk], position); err != nil {
			return nil, err
		}
	}
	return output, nil
}
