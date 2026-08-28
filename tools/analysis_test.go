package main

import "testing"

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

func TestDropRegionPoolCandidatesAreModuleQualified(t *testing.T) {
	config := &projectConfig{byID: map[string]*moduleSpec{
		"opening": {ID: "opening", Regions: []regionSpec{{Addr: 0x80073b9c, End: 0x80073ba0, Kind: "data", Why: "word"}}},
		"battle":  {ID: "battle"},
		"world":   {ID: "world"},
	}}
	pool := []discoveryPoolCandidate{
		{Module: "world", Start: 0x800e1000, End: 0x800e1020},
		{Module: "opening", Start: 0x80073b9c, End: 0x80073ba0},
		{Module: "battle", Start: 0x80073b9c, End: 0x80073eec},
	}
	kept := dropRegionPoolCandidates(pool, config)
	if len(kept) != 2 {
		t.Fatalf("expected only OPEN's region to be dropped, got %#v", kept)
	}
	for _, candidate := range kept {
		if candidate.Module == "opening" {
			t.Fatalf("OPEN region survived: %#v", kept)
		}
	}
}
