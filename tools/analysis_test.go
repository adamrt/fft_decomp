package main

import (
	"encoding/binary"
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestAnalysisReferencesKeepCodeAndDataSitesDistinct(t *testing.T) {
	root := t.TempDir()
	image := make([]byte, 96)
	words := []uint32{
		0x0c004010, // JAL 0x80010040, in the configured function.
		0x1000000f, // BEQ to 0x80010044, in the configured function.
		discoveryJRRA, 0,
		0x0c004010, 0x0c004010, 0x0c004010, // The same encoding in data.
		0x08004014,                         // J to a configured data symbol: excluded as before.
		0x10000008,                         // BEQ to 0x80010044, in data.
		0x08004015, 0x08004015, 0x08004015, // A target referenced only by data.
	}
	for index, word := range words {
		binary.LittleEndian.PutUint32(image[index*4:], word)
	}
	if err := os.WriteFile(filepath.Join(root, "TEST.BIN"), image, 0o644); err != nil {
		t.Fatal(err)
	}
	m := &moduleSpec{
		ID: "test", File: "TEST.BIN", Load: 0x80010000, Size: len(image),
		Functions: []functionSpec{{Name: "test_entrypoint", Addr: 0x80010000, Size: 16}},
		Data:      []symbolSpec{{Name: "test_data", Addr: 0x80010050}},
	}
	exclusions := discoveryExclusionsFromConfig(&projectConfig{Modules: []*moduleSpec{m}})
	report, _, err := analyzeModule(project{filesRoot: root}, m, inventoryRecord{}, exclusions)
	if err != nil {
		t.Fatal(err)
	}
	encoded, err := json.Marshal(report)
	if err != nil {
		t.Fatal(err)
	}
	if strings.Contains(string(encoded), `"confidence"`) {
		t.Errorf("raw reference frequency was presented as confidence: %s", encoded)
	}
	if report.ReferenceScope != "raw_image_words" || len(report.JumpTargets) != 2 {
		t.Fatalf("missing scan provenance or changed configured-symbol exclusion: %s", encoded)
	}
	jump := report.JumpTargets[0]
	if jump.Address != "0x80010040" || jump.References != 4 || jump.ConfiguredReferences != 1 || len(jump.Sites) != 4 {
		t.Fatalf("data references obscured the known function site: %+v", jump)
	}
	for index, address := range []string{"0x80010000", "0x80010010", "0x80010014", "0x80010018"} {
		site := jump.Sites[index]
		if site.Address != address || site.Word != "0x0c004010" || (site.ConfiguredFunction == "test_entrypoint") != (index == 0) {
			t.Errorf("incorrect provenance at reference %d: %+v", index, site)
		}
	}
	dataOnly := report.JumpTargets[1]
	if dataOnly.Address != "0x80010054" || dataOnly.References != 3 || dataOnly.ConfiguredReferences != 0 {
		t.Errorf("data-only reference frequency implied configured code: %+v", dataOnly)
	}
	if len(report.BranchTargets) != 1 {
		t.Fatalf("branch references missing: %+v", report.BranchTargets)
	}
	branch := report.BranchTargets[0]
	if branch.Address != "0x80010044" || branch.References != 2 || branch.ConfiguredReferences != 1 || branch.Sites[0].Address != "0x80010004" || branch.Sites[1].Address != "0x80010020" {
		t.Errorf("branch reference provenance changed: %+v", branch)
	}
}

func TestAnalysisReferenceSitesUseEXELoadAddressAndPayloadBounds(t *testing.T) {
	root := t.TempDir()
	image := make([]byte, psxEXEHeaderSize+112)
	for _, offset := range []int{0, psxEXEHeaderSize, psxEXEHeaderSize + 4, psxEXEHeaderSize + 96} {
		binary.LittleEndian.PutUint32(image[offset:], 0x0c004010)
	}
	if err := os.WriteFile(filepath.Join(root, "TEST.EXE"), image, 0o644); err != nil {
		t.Fatal(err)
	}
	m := &moduleSpec{
		ID: "main", File: "TEST.EXE", Load: 0x8000f800, Size: len(image),
		Functions: []functionSpec{{Name: "test_entrypoint", Addr: 0x80010000, Size: 4}},
	}
	exclusions := discoveryExclusionsFromConfig(&projectConfig{Modules: []*moduleSpec{m}})
	record := inventoryRecord{Format: "ps-x-exe", TextAddress: "0x80010000", TextSize: 96}
	report, _, err := analyzeModule(project{filesRoot: root}, m, record, exclusions)
	if err != nil {
		t.Fatal(err)
	}
	if len(report.JumpTargets) != 1 {
		t.Fatalf("unexpected jump targets: %+v", report.JumpTargets)
	}
	jump := report.JumpTargets[0]
	if jump.References != 2 || jump.ConfiguredReferences != 1 || len(jump.Sites) != 2 || jump.Sites[0].Address != "0x80010000" || jump.Sites[1].Address != "0x80010004" {
		t.Fatalf("header/trailer included or source address biased: %+v", jump)
	}
}

func TestAnalysisPSXTextRange(t *testing.T) {
	start, end, err := analysisPSXTextRange(0x900, 0x100)
	if err != nil || start != 0x800 || end != 0x900 {
		t.Fatalf("start=%#x end=%#x err=%v", start, end, err)
	}
	for _, input := range [][2]int{{0x7ff, 0}, {0x900, -1}, {0x900, 0x101}} {
		if _, _, err := analysisPSXTextRange(input[0], input[1]); err == nil {
			t.Errorf("accepted file/text sizes %v", input)
		}
	}
}

func TestAnalysisDiscoveryExtentReportsEvidenceSource(t *testing.T) {
	extent, source := analysisDiscoveryExtent(nil)
	if source != "none" || extent.Start != 0 || extent.End != 0 {
		t.Fatalf("unexpected empty evidence extent: %#v %q", extent, source)
	}
	extent, source = analysisDiscoveryExtent(map[uint64]int{0x80001000: 16, 0x80001100: 8})
	if source != "known-only" || extent.Start != 0x80001000 || extent.End != 0x80001108 {
		t.Fatalf("unexpected known extent: %#v %q", extent, source)
	}
}

func TestMarkAnalysisCoverage(t *testing.T) {
	covered := make([]bool, 16)
	if marked := markAnalysisCoverage(covered, 4, 8); marked != 8 {
		t.Fatalf("marked=%v", marked)
	}
	for index, value := range covered {
		if value != (index >= 4 && index < 12) {
			t.Fatalf("covered[%d]=%v", index, value)
		}
	}
	for _, start := range []int{-4, 12} {
		if marked := markAnalysisCoverage(make([]bool, 16), start, 8); marked != 0 {
			t.Fatalf("outside range marked %d bytes", marked)
		}
	}
}

func TestMarkAnalysisCoverageRejectsMalformedSizesWithoutOverflow(t *testing.T) {
	maxInt := int(^uint(0) >> 1)
	for _, bounds := range [][2]int{{0, -4}, {4, -8}, {0, 0}, {1, maxInt}, {maxInt, 4}} {
		covered := make([]bool, 16)
		if got := markAnalysisCoverage(covered, bounds[0], bounds[1]); got != 0 {
			t.Errorf("malformed range %v reported %d covered bytes", bounds, got)
		}
		for index, marked := range covered {
			if marked {
				t.Errorf("malformed range %v marked byte %d", bounds, index)
			}
		}
	}
}

func TestDropRegionPoolCandidatesAreModuleQualified(t *testing.T) {
	config := &projectConfig{byID: map[string]*moduleSpec{
		"open":   {ID: "open", Regions: []regionSpec{{Addr: 0x80073b9c, End: 0x80073ba0, Kind: "data", Why: "word"}}},
		"battle": {ID: "battle"},
		"world":  {ID: "world"},
	}}
	pool := []discoveryPoolCandidate{
		{Module: "world", Start: 0x800e1000, End: 0x800e1020},
		{Module: "open", Start: 0x80073b9c, End: 0x80073ba0},
		{Module: "battle", Start: 0x80073b9c, End: 0x80073eec},
	}
	kept := dropRegionPoolCandidates(pool, config)
	if len(kept) != 2 {
		t.Fatalf("expected only OPEN's region to be dropped, got %#v", kept)
	}
	for _, candidate := range kept {
		if candidate.Module == "open" {
			t.Fatalf("OPEN region survived: %#v", kept)
		}
	}
}
