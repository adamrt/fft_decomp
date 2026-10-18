package main

import (
	"crypto/sha256"
	"fmt"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
)

func unitTrialDataFixture() (*unitInstance, []byte) {
	image := make([]byte, 256)
	for i := range image {
		image[i] = byte(i)
	}
	const base = uint32(0x80000000)
	owned := rodataSpec{Addr: base + 128, Size: 8, Hash: shortHash(image[128:136])}
	m := &moduleSpec{ID: "battle", File: "BATTLE.BIN", Load: base, Size: len(image), SHA256: fmt.Sprintf("%x", sha256.Sum256(image)),
		Functions: []functionSpec{{Name: "entry", Addr: base + 64, Size: 32, Hash: shortHash(image[64:96]), Rodata: []rodataSpec{{Addr: base + 16, Size: 4, Hash: shortHash(image[16:20])}}}},
		Units:     []unitDef{{Name: "owner", Data: []rodataSpec{owned}, Rodata: []rodataSpec{{Addr: base + 24, Size: 4, Hash: shortHash(image[24:28])}}}}}
	return &unitInstance{module: m, data: []rodataSpec{owned}}, image
}

func TestUnitTrialDataRetainsOriginalContractsAndRejectsChangedBytes(t *testing.T) {
	unit, image := unitTrialDataFixture()
	original := unit.data[0]
	originalFunction := unit.module.Functions[0]
	// Input order need not be address order; the existing span stays intact.
	additions := []unitTrialRangeYAML{{Addr: unit.module.Load + 136, Size: 4}, {Addr: unit.module.Load + 124, Size: 4}}
	if err := extendUnitTrialRanges(unit, additions, nil, image); err != nil {
		t.Fatal(err)
	}
	if len(unit.data) != 3 || unit.data[1] != original || unit.module.Units[0].Data[0] != original || !reflect.DeepEqual(unit.module.Functions[0], originalFunction) {
		t.Fatalf("existing metadata changed: %+v", unit)
	}
	if unit.data[0].Hash != shortHash(image[124:128]) || unit.data[2].Hash != shortHash(image[136:140]) {
		t.Fatalf("new hashes do not describe original data: %+v", unit.data)
	}
	got := append([]byte(nil), image[124:140]...)
	if problems := compareUnitRanges(".data", unit.data, got); len(problems) != 0 {
		t.Fatal(problems)
	}
	for _, offset := range []int{0, 4, 12} {
		changed := append([]byte(nil), got...)
		changed[offset] ^= 1
		if problems := compareUnitRanges(".data", unit.data, changed); len(problems) == 0 {
			t.Fatalf("accepted changed byte at%d", offset)
		}
	}
}

func TestUnitTrialDataRejectsInvalidOverlappingOrRemoteRanges(t *testing.T) {
	const base = uint32(0x80000000)
	tests := []struct {
		name string
		data []unitTrialRangeYAML
		want string
	}{
		{"zero size", []unitTrialRangeYAML{{Addr: base + 124}}, "invalid"},
		{"negative size", []unitTrialRangeYAML{{Addr: base + 124, Size: -1}}, "invalid"},
		{"large size", []unitTrialRangeYAML{{Addr: base + 124, Size: int(^uint(0) >> 1)}}, "invalid"},
		{"below module", []unitTrialRangeYAML{{Addr: base - 1, Size: 1}}, "outside"},
		{"past module", []unitTrialRangeYAML{{Addr: base + 255, Size: 2}}, "outside"},
		{"text", []unitTrialRangeYAML{{Addr: base + 92, Size: 8}}, "function"},
		{"function rodata", []unitTrialRangeYAML{{Addr: base + 16, Size: 4}}, "rodata"},
		{"unit rodata", []unitTrialRangeYAML{{Addr: base + 24, Size: 4}}, "unit owner"},
		{"existing data", []unitTrialRangeYAML{{Addr: base + 128, Size: 4}}, "unit owner"},
		{"duplicate", []unitTrialRangeYAML{{Addr: base + 124, Size: 4}, {Addr: base + 124, Size: 4}}, "overlap"},
		{"partial overlap", []unitTrialRangeYAML{{Addr: base + 120, Size: 8}, {Addr: base + 124, Size: 4}}, "overlap"},
		{"remote data", []unitTrialRangeYAML{{Addr: base + 144, Size: 4}}, "not contiguous"},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			unit, image := unitTrialDataFixture()
			before := append([]rodataSpec(nil), unit.data...)
			if err := extendUnitTrialRanges(unit, tt.data, nil, image); err == nil || !strings.Contains(err.Error(), tt.want) {
				t.Fatalf("error=%v, want%q", err, tt.want)
			}
			if !reflect.DeepEqual(unit.data, before) {
				t.Fatal("failed trial mutated inherited data contracts")
			}
		})
	}
}

func TestUnitTrialDataUsesVerifiedExtractedImage(t *testing.T) {
	unit, image := unitTrialDataFixture()
	p := project{filesRoot: t.TempDir()}
	path := filepath.Join(p.filesRoot, unit.module.File)
	if err := os.WriteFile(path, image, 0o644); err != nil {
		t.Fatal(err)
	}
	addition := []unitTrialRangeYAML{{Addr: unit.module.Load + 124, Size: 4}}
	if err := p.addUnitTrialRanges(unit, addition, nil); err != nil {
		t.Fatal(err)
	}
	unit, image = unitTrialDataFixture()
	image[124] ^= 1
	if err := os.WriteFile(path, image, 0o644); err != nil {
		t.Fatal(err)
	}
	if err := p.addUnitTrialRanges(unit, addition, nil); err == nil || !strings.Contains(err.Error(), "sha256") {
		t.Fatalf("accepted corrupt extracted image: %v", err)
	}
	if len(unit.data) != 1 {
		t.Fatal("corrupt original changed inherited data contracts")
	}
}

func TestUnitTrialDataSchemaAcceptsRangesButNotSuppliedHashes(t *testing.T) {
	base := "functions: [entry]\ninstances:\n  - module: battle\n    original_unit: owner\n    data:\n      - {addr: 0x8000007c, size: 4%s}\n"
	if _, err := parseUnitSpec("build/trial.yaml", []byte(fmt.Sprintf(base, ""))); err != nil {
		t.Fatal(err)
	}
	if _, err := parseUnitSpec("build/trial.yaml", []byte(fmt.Sprintf(base, ", hash: 0000000000000000"))); err == nil {
		t.Fatal("accepted a supplied scratch data hash")
	}
}
