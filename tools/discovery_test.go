package main

import (
	"encoding/binary"
	"reflect"
	"strings"
	"testing"
)

func discoveryTestWords(words ...uint32) []byte {
	data := make([]byte, len(words)*4)
	for index, word := range words {
		binary.LittleEndian.PutUint32(data[index*4:], word)
	}
	return data
}

func TestDiscoverFunctionRangesPaddingAndDelaySlots(t *testing.T) {
	data := discoveryTestWords(0, 0, 0x27bdfff0, discoveryJRRA, 0x27bd0010, 0,
		0x24020001, discoveryJRRA, 0, 0, 0x24020002)
	found, err := discoverFunctionRanges(data, 0x80010000, 0, nil)
	if err != nil {
		t.Fatal(err)
	}
	want := []discoveredFunction{
		{0x80010008, 0x80010014, discoveryReturnEnd},
		{0x80010018, 0x80010024, discoveryReturnEnd},
		{0x80010028, 0x8001002c, discoveryImageEnd},
	}
	if !reflect.DeepEqual(found, want) {
		t.Fatalf("got %#v, want %#v", found, want)
	}
}

func TestDiscoverFunctionRangesKnownStartsAreSeedsNotIndependentEvidence(t *testing.T) {
	data := discoveryTestWords(0x24020001, 0x24020002, 0x24020003, discoveryJRRA, 0)
	unseeded, err := discoverFunctionRanges(data, 0x80010000, 0, nil)
	if err != nil {
		t.Fatal(err)
	}
	found, err := discoverFunctionRanges(data, 0x80010000, 0, []uint64{0x80010008})
	if err != nil {
		t.Fatal(err)
	}
	want := []discoveredFunction{{0x80010000, 0x80010008, discoveryKnownEnd}, {0x80010008, 0x80010014, discoveryReturnEnd}}
	if len(unseeded) != 1 || !reflect.DeepEqual(found, want) {
		t.Fatalf("known start did not split candidate: unseeded=%#v seeded=%#v", unseeded, found)
	}
}

func TestDiscoveryValidationExposesEarlyReturnsAndLeadingNOPs(t *testing.T) {
	// An early return is still a heuristic boundary, even when the
	// configuration proves both return paths belong to one function.
	data := discoveryTestWords(0x24020001, discoveryJRRA, 0, 0x24020002, discoveryJRRA, 0)
	found, err := discoverFunctionRanges(data, 0x80010000, 0, []uint64{0x80010000})
	if err != nil {
		t.Fatal(err)
	}
	exclusions := discoveryExclusions{Known: map[discoveryAddress]int{
		{"world", 0x80010000}:  24,
		{"battle", 0x80010000}: 12,
	}}
	report := validateDiscoveryBoundaries("world", found, exclusions)
	if report.Known != 1 || report.Rediscovered != 1 || len(report.Missing) != 0 || !reflect.DeepEqual(report.SizeDisagreements, []discoverySizeDisagreement{{0x80010000, 24, 12}}) {
		t.Fatalf("early-return disagreement was hidden: %#v", report)
	}
	data = discoveryTestWords(0, discoveryJRRA, 0)
	found, err = discoverFunctionRanges(data, 0x80010000, 0, []uint64{0x80010000})
	if err != nil {
		t.Fatal(err)
	}
	report = validateDiscoveryBoundaries("battle", found, exclusions)
	if report.Rediscovered != 0 || !reflect.DeepEqual(report.Missing, []discoveryKnownBoundary{{0x80010000, 12}}) {
		t.Fatalf("leading-NOP entry was incorrectly claimed as rediscovered: %#v", report)
	}
}

func TestDiscoverySkipsEXEHeaderAndBoundsTruncatedInstructions(t *testing.T) {
	header := make([]byte, psxEXEHeaderSize)
	binary.LittleEndian.PutUint32(header, discoveryJRRA)
	data := append(header, discoveryTestWords(0x24020001, discoveryJRRA, 0)...)
	data = append(data, 0xaa, 0xbb, 0xcc) // Incomplete trailing words are ignored.
	found, err := discoverFunctionRanges(data, 0x80010000, psxEXEHeaderSize, nil)
	if err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(found, []discoveredFunction{{0x80010000, 0x8001000c, discoveryReturnEnd}}) {
		t.Fatalf("header was scanned or address bias changed: %#v", found)
	}
	found, err = discoverFunctionRanges(discoveryTestWords(discoveryJRRA), 0x80010000, 0, nil)
	if err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(found, []discoveredFunction{{0x80010000, 0x80010004, discoveryTruncatedEnd}}) {
		t.Fatalf("missing delay slot was fabricated: %#v", found)
	}
	pool, err := discoveryCandidatePool("world", found, discoveryExclusions{}, discoveryPoolOptions{MinBytes: 4, MaxBytes: 800})
	if err != nil || len(pool.Candidates) != 0 || pool.Truncated != 1 {
		t.Fatalf("truncated return entered candidate pool: %#v, %v", pool, err)
	}
	for _, data := range [][]byte{nil, {0, 0, 0}, discoveryTestWords(0, 0)} {
		found, err := discoverFunctionRanges(data, 0x80010000, 0, nil)
		if err != nil || len(found) != 0 {
			t.Fatalf("padding-only input produced functions: %#v, %v", found, err)
		}
	}
}

func TestDiscoveryRejectsInvalidAddressModels(t *testing.T) {
	for _, test := range []struct {
		load uint64
		bias int
	}{
		{0x80010000, -1}, {0x80010000, 1}, {0x80010000, 12},
		{0x80010001, 0}, {0x100000000, 0}, {0xfffffffc, 0},
	} {
		if _, err := discoverFunctionRanges(make([]byte, 8), test.load, test.bias, nil); err == nil {
			t.Fatalf("accepted load 0x%x bias %d", test.load, test.bias)
		}
	}
}

func discoveryTestConfig() *projectConfig {
	alpha := &moduleSpec{ID: "alpha",
		Functions: []functionSpec{{Addr: 0x80010000, Size: 16, Name: "accepted"}},
		Libraries: []librarySpec{{ID: "owned", Addr: 0x80010028, End: 0x80010030, Kind: "psyq", Library: "LIB"}},
	}
	beta := &moduleSpec{ID: "beta"}
	return &projectConfig{Modules: []*moduleSpec{alpha, beta}, byID: map[string]*moduleSpec{"alpha": alpha, "beta": beta}}
}

func TestDiscoveryPoolExclusionsAreModuleQualifiedAndHalfOpen(t *testing.T) {
	exclusions := discoveryExclusionsFromConfig(discoveryTestConfig())
	found := []discoveredFunction{
		{0x8000fffc, 0x80010004, discoveryReturnEnd}, // Starts before accepted range, but overlaps it.
		{0x80010004, 0x80010014, discoveryReturnEnd}, // Early-return fragment within accepted code.
		{0x80010010, 0x80010014, discoveryReturnEnd}, // Adjacent to accepted range: not excluded.
		{0x80010018, 0x80010024, discoveryReturnEnd}, // Unowned code.
		{0x80010020, 0x8001002c, discoveryReturnEnd}, // Crosses into owned library range.
		{0x80010028, 0x80010030, discoveryReturnEnd}, // Entirely owned by library.
		{0x80010030, 0x8001003c, discoveryReturnEnd}, // Adjacent to library range: not excluded.
	}
	options := discoveryPoolOptions{MinBytes: 4, MaxBytes: 800}
	pool, err := discoveryCandidatePool("alpha", found, exclusions, options)
	if err != nil {
		t.Fatal(err)
	}
	if pool.Known != 2 || pool.Library != 2 || len(pool.Candidates) != 3 {
		t.Fatalf("incorrect half-open exclusions: %#v", pool)
	}
	pool, err = discoveryCandidatePool("beta", found, exclusions, options)
	if err != nil || len(pool.Candidates) != len(found) {
		t.Fatalf("another overlay's metadata hid candidates: %#v, %v", pool, err)
	}
	if len(exclusions.knownForModule("beta")) != 0 {
		t.Fatal("known starts leaked between modules")
	}
}

func TestDiscoveryExtentCoversKnownRanges(t *testing.T) {
	extent := discoveryCodeExtent(map[uint64]int{0x80100ff0: 8, 0x80102100: 16})
	if extent == nil || extent.Start != 0x80100ff0 || extent.End != 0x80102110 {
		t.Fatalf("unexpected extent: %#v", extent)
	}
	if discoveryCodeExtent(nil) != nil {
		t.Fatal("empty known set produced an extent")
	}
}

func TestDiscoveryPoolSizeExtentAndDeterministicRows(t *testing.T) {
	options := defaultDiscoveryPoolOptions()
	if options.MinBytes != 12 || options.MaxBytes != 800 {
		t.Fatalf("script-compatible defaults changed: %#v", options)
	}
	options.Extent = &discoveredFunction{Start: 0x80010000, End: 0x80010400}
	found := []discoveredFunction{
		{0x80010800, 0x8001080c, discoveryReturnEnd},
		{0x80010020, 0x80010340, discoveryImageEnd},  // Exactly maximum size.
		{0x80010010, 0x8001001c, discoveryReturnEnd}, // Exactly minimum size.
		{0x80010000, 0x80010008, discoveryReturnEnd},
		{0x800103fc, 0x80010408, discoveryReturnEnd}, // Extent check is start-based.
	}
	pool, err := discoveryCandidatePool("world", found, discoveryExclusions{}, options)
	if err != nil || pool.Size != 1 || pool.Outside != 1 || len(pool.Candidates) != 3 {
		t.Fatalf("unexpected size/extent filtering: %#v, %v", pool, err)
	}
	rows := formatDiscoveryPoolRows(pool.Candidates)
	want := "world\t12\t0x80010010\t0x8001001c\tfunc_80010010\nworld\t800\t0x80010020\t0x80010340\tfunc_80010020\nworld\t12\t0x800103fc\t0x80010408\tfunc_800103fc\n"
	if rows != want {
		t.Fatalf("unexpected pool TSV:\n%s", rows)
	}
	reversed := []discoveryPoolCandidate{pool.Candidates[2], pool.Candidates[0], pool.Candidates[1]}
	if formatDiscoveryPoolRows(reversed) != want || reversed[0].Start != 0x800103fc {
		t.Fatal("row ordering depends on input order or mutates its caller")
	}
	if !strings.HasPrefix(formatDiscoveryPoolRows(append(reversed, discoveryPoolCandidate{"battle", 0x80010000, 0x8001000c})), "battle\t") {
		t.Fatal("cross-module rows are not deterministic")
	}
}
