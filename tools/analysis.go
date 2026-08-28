package main

// analysis.go writes advisory, read-only scans after extraction; its reports
// never feed target/.

import (
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"
)

type mipsInventory struct {
	SchemaVersion int                `json:"schema_version"`
	TargetID      string             `json:"target_id"`
	Source        string             `json:"source"`
	CandidatePool string             `json:"candidate_pool"`
	Modules       []mipsModuleReport `json:"modules"`
}

type mipsModuleReport struct {
	Module            string               `json:"module"`
	Path              string               `json:"path"`
	Bytes             int                  `json:"bytes"`
	LoadAddress       string               `json:"load_address"`
	ScanStart         string               `json:"scan_start,omitempty"`
	ScanEnd           string               `json:"scan_end,omitempty"`
	ConfiguredRanges  int                  `json:"configured_ranges"`
	ConfiguredBytes   int                  `json:"configured_bytes"`
	UncoveredBytes    int                  `json:"uncovered_bytes"`
	CallTargets       []mipsCandidate      `json:"call_targets,omitempty"`
	BranchTargets     []mipsCandidate      `json:"branch_targets,omitempty"`
	FunctionDiscovery *mipsDiscoveryReport `json:"function_discovery,omitempty"`
	Status            string               `json:"status"`
}

type mipsCandidate struct {
	Address    string `json:"address"`
	References int    `json:"references"`
	Confidence string `json:"confidence"`
}

type mipsDiscoveryReport struct {
	Detected          int                       `json:"detected"`
	Known             int                       `json:"known"`
	Rediscovered      int                       `json:"rediscovered"`
	ExtentSource      string                    `json:"extent_source"`
	Missing           []mipsDiscoveryBoundary   `json:"missing,omitempty"`
	SizeDisagreements []mipsDiscoveryDifference `json:"size_disagreements,omitempty"`
	Candidates        []mipsFunctionCandidate   `json:"candidates,omitempty"`
	Excluded          mipsDiscoveryExclusions   `json:"excluded"`
}

type mipsDiscoveryBoundary struct {
	Address string `json:"address"`
	Bytes   int    `json:"bytes"`
}

type mipsDiscoveryDifference struct {
	Address       string `json:"address"`
	KnownBytes    int    `json:"known_bytes"`
	DetectedBytes uint64 `json:"detected_bytes"`
}

type mipsFunctionCandidate struct {
	Address string `json:"address"`
	End     string `json:"end_exclusive"`
	Bytes   uint64 `json:"bytes"`
	Symbol  string `json:"symbol"`
}

type mipsDiscoveryExclusions struct {
	Known     int `json:"known"`
	Library   int `json:"library"`
	Outside   int `json:"outside"`
	Size      int `json:"size"`
	Truncated int `json:"truncated"`
}

func (p project) buildAnalysis() error {
	config, err := p.loadProjectConfig()
	if err != nil {
		return err
	}
	records, err := p.loadInventory()
	if err != nil {
		return err
	}
	exclusions := discoveryExclusionsFromConfig(config)
	report := mipsInventory{
		SchemaVersion: 1, TargetID: targetID,
		Source: "build/extracted/inventory.json", CandidatePool: "build/analysis/inventory/candidates.tsv",
	}
	var poolCandidates []discoveryPoolCandidate
	for _, m := range config.Modules {
		record, ok := records[m.File]
		if !ok {
			return fmt.Errorf("module %s path %s is missing from inventory", m.ID, m.File)
		}
		item, candidates, err := analyzeModule(p, m, record, exclusions)
		if err != nil {
			return err
		}
		report.Modules = append(report.Modules, item)
		poolCandidates = append(poolCandidates, candidates...)
	}
	poolCandidates = dropRegionPoolCandidates(poolCandidates, config)
	base := filepath.Join(p.root, "build", "analysis", "inventory")
	if err := os.MkdirAll(base, 0o755); err != nil {
		return err
	}
	data, err := json.MarshalIndent(report, "", "  ")
	if err != nil {
		return err
	}
	data = append(data, '\n')
	if err := atomicWrite(filepath.Join(base, "mips-inventory.json"), data); err != nil {
		return err
	}
	if err := atomicWrite(filepath.Join(base, "candidates.tsv"), []byte(formatDiscoveryPoolRows(poolCandidates))); err != nil {
		return err
	}
	var summary strings.Builder
	fmt.Fprintf(&summary, "FFT MIPS analysis inventory\nmodules: %d\nfunction candidates: %d\ncandidate pool: %s\n\n", len(report.Modules), len(poolCandidates), report.CandidatePool)
	for _, item := range report.Modules {
		fmt.Fprintf(&summary, "%s %s: %s", item.Module, item.Path, item.Status)
		if item.ScanStart != "" {
			fmt.Fprintf(&summary, ", scan %s-%s, configured %d ranges/%d bytes, uncovered %d bytes, call candidates %d, branch candidates %d", item.ScanStart, item.ScanEnd, item.ConfiguredRanges, item.ConfiguredBytes, item.UncoveredBytes, len(item.CallTargets), len(item.BranchTargets))
			if discovery := item.FunctionDiscovery; discovery != nil {
				fmt.Fprintf(&summary, ", functions %d, rediscovered %d/%d, pool %d", discovery.Detected, discovery.Rediscovered, discovery.Known, len(discovery.Candidates))
			}
		}
		summary.WriteByte('\n')
	}
	if err := atomicWrite(filepath.Join(base, "README.txt"), []byte(summary.String())); err != nil {
		return err
	}
	fmt.Printf("wrote MIPS analysis inventory to build/analysis/inventory\n")
	return nil
}

func (p project) loadInventory() (map[string]inventoryRecord, error) {
	data, err := os.ReadFile(p.inventoryPath)
	if errors.Is(err, os.ErrNotExist) {
		return nil, errNotExtracted
	} else if err != nil {
		return nil, err
	}
	var value inventory
	if err := json.Unmarshal(data, &value); err != nil {
		return nil, fmt.Errorf("parse extracted inventory: %w", err)
	}
	result := make(map[string]inventoryRecord, len(value.Files))
	for _, record := range value.Files {
		result[record.Path] = record
	}
	return result, nil
}

// moduleScan is one module image and the address model to read it by: the
// PS-X EXE header is skipped and only its text is scanned.
type moduleScan struct {
	data       []byte
	load       uint64 // runtime address of data[start]
	start, end int
}

func readModuleScan(p project, m *moduleSpec, record inventoryRecord) (moduleScan, error) {
	data, err := os.ReadFile(filepath.Join(p.filesRoot, filepath.FromSlash(m.File)))
	if err != nil {
		return moduleScan{}, err
	}
	scan := moduleScan{data: data, load: uint64(m.Load), start: 0, end: len(data)}
	if record.Format == "ps-x-exe" {
		textAddress, err := parseHex(record.TextAddress, m.ID+" text address")
		if err != nil {
			return moduleScan{}, err
		}
		scan.start, scan.end, err = analysisPSXTextRange(len(data), record.TextSize)
		if err != nil {
			return moduleScan{}, fmt.Errorf("%s: %w", m.ID, err)
		}
		scan.load = textAddress
	}
	return scan, nil
}

func analyzeModule(p project, m *moduleSpec, record inventoryRecord, exclusions discoveryExclusions) (mipsModuleReport, []discoveryPoolCandidate, error) {
	item := mipsModuleReport{Module: m.ID, Path: m.File, Bytes: m.Size, LoadAddress: fmt.Sprintf("0x%08x", m.Load)}
	scan, err := readModuleScan(p, m, record)
	if err != nil {
		return item, nil, err
	}
	data, load, start, end := scan.data, scan.load, scan.start, scan.end
	item.ScanStart = fmt.Sprintf("0x%08x", load)
	item.ScanEnd = fmt.Sprintf("0x%08x", load+uint64(end-start))
	known := exclusions.knownForModule(m.ID)
	knownStarts := make([]uint64, 0, len(known))
	for address := range known {
		knownStarts = append(knownStarts, address)
	}
	discovered, err := discoverFunctionRanges(data[:end], load, start, knownStarts)
	if err != nil {
		return item, nil, err
	}
	validation := validateDiscoveryBoundaries(m.ID, discovered, exclusions)
	options := defaultDiscoveryPoolOptions()
	options.MaxBytes = 0xffffffff
	var extentSource string
	options.Extent, extentSource = analysisDiscoveryExtent(known)
	pool, err := discoveryCandidatePool(m.ID, discovered, exclusions, options)
	if err != nil {
		return item, nil, err
	}
	discoveryReport := &mipsDiscoveryReport{
		Detected:     len(discovered),
		Known:        validation.Known,
		Rediscovered: validation.Rediscovered,
		ExtentSource: extentSource,
		Excluded: mipsDiscoveryExclusions{
			Known: pool.Known, Library: pool.Library,
			Outside: pool.Outside, Size: pool.Size, Truncated: pool.Truncated,
		},
	}
	for _, missing := range validation.Missing {
		discoveryReport.Missing = append(discoveryReport.Missing, mipsDiscoveryBoundary{fmt.Sprintf("0x%08x", missing.Address), missing.Bytes})
	}
	for _, difference := range validation.SizeDisagreements {
		discoveryReport.SizeDisagreements = append(discoveryReport.SizeDisagreements, mipsDiscoveryDifference{fmt.Sprintf("0x%08x", difference.Address), difference.KnownBytes, difference.DetectedBytes})
	}
	for _, candidate := range pool.Candidates {
		discoveryReport.Candidates = append(discoveryReport.Candidates, mipsFunctionCandidate{
			Address: fmt.Sprintf("0x%08x", candidate.Start), End: fmt.Sprintf("0x%08x", candidate.End),
			Bytes: candidate.End - candidate.Start, Symbol: fmt.Sprintf("func_%08x", candidate.Start),
		})
	}
	item.FunctionDiscovery = discoveryReport
	covered := make([]bool, end-start)
	for _, f := range m.Functions {
		markedBytes := markAnalysisCoverage(covered, int(uint64(f.Addr)-load), f.Size)
		if markedBytes == 0 {
			continue
		}
		item.ConfiguredRanges++
		item.ConfiguredBytes += markedBytes
	}
	configured := make(map[uint64]bool)
	for _, f := range m.Functions {
		configured[uint64(f.Addr)] = true
	}
	for _, symbol := range m.Data {
		configured[uint64(symbol.Addr)] = true
	}
	for _, value := range covered {
		if !value {
			item.UncoveredBytes++
		}
	}
	callRefs, branchRefs := make(map[uint64]int), make(map[uint64]int)
	for offset := start; offset+4 <= end; offset += 4 {
		word := binary.LittleEndian.Uint32(data[offset : offset+4])
		pc := load + uint64(offset-start)
		op := word >> 26
		if op == 2 || op == 3 {
			target := (pc+4)&0xf0000000 | uint64(word&0x03ffffff)<<2
			if target >= load && target < load+uint64(end-start) {
				callRefs[target]++
			}
			continue
		}
		if op == 1 || (op >= 4 && op <= 7) || (op >= 20 && op <= 23) {
			delta := int64(int16(word&0xffff)) << 2
			target := uint64(int64(pc+4) + delta)
			if target >= load && target < load+uint64(end-start) {
				branchRefs[target]++
			}
		}
	}
	item.CallTargets = candidates(callRefs, configured)
	item.BranchTargets = candidates(branchRefs, configured)
	item.Status = "scanned"
	return item, pool.Candidates, nil
}

func analysisPSXTextRange(fileBytes, textBytes int) (int, int, error) {
	if fileBytes < psxEXEHeaderSize || textBytes < 0 || textBytes > fileBytes-psxEXEHeaderSize {
		return 0, 0, errors.New("text range exceeds file")
	}
	return psxEXEHeaderSize, psxEXEHeaderSize + textBytes, nil
}

func analysisDiscoveryExtent(known map[uint64]int) (*discoveredFunction, string) {
	if extent := discoveryCodeExtent(known); extent != nil {
		return extent, "known-only"
	}
	// An unbounded scan of mostly-data bytes yields false positives, so an
	// empty extent suppresses candidates (reported as extent_source "none").
	return &discoveredFunction{}, "none"
}

// markAnalysisCoverage marks size bytes at scan offset start as covered.
func markAnalysisCoverage(covered []bool, start, size int) int {
	end := start + size
	if start < 0 || end > len(covered) {
		return 0
	}
	for index := start; index < end; index++ {
		covered[index] = true
	}
	return size
}

func candidates(refs map[uint64]int, configured map[uint64]bool) []mipsCandidate {
	addresses := make([]uint64, 0, len(refs))
	for address := range refs {
		if configured[address] {
			continue
		}
		addresses = append(addresses, address)
	}
	sort.Slice(addresses, func(i, j int) bool { return addresses[i] < addresses[j] })
	result := make([]mipsCandidate, 0, len(addresses))
	for _, address := range addresses {
		confidence := "low"
		if refs[address] >= 3 {
			confidence = "high"
		} else if refs[address] == 2 {
			confidence = "medium"
		}
		result = append(result, mipsCandidate{Address: fmt.Sprintf("0x%08x", address), References: refs[address], Confidence: confidence})
	}
	return result
}
