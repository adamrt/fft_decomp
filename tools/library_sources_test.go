package main

import (
	"crypto/sha256"
	"fmt"
	"path/filepath"
	"testing"
)

// Exercise each analysis entrypoint: a move under src/lib must not silently
// remove game functions from any of the reports.
func TestAnalysesDistinguishGameLibrariesFromPlatformAndMetadataLibraries(t *testing.T) {
	for _, test := range []struct {
		name, source string
		library      bool
		metadata     bool
		overlap      bool
	}{
		{"program", "src/world/right.c", false, false, false},
		{"event library", "src/lib/event/right.c", false, false, false},
		{"menu library", "src/lib/menu/right.c", false, false, false},
		{"root library", "src/lib/right.c", false, false, false},
		{"platform", "src/platform/psyq/libgpu/right.c", true, false, false},
		{"Suzuki metadata", "src/lib/suzuki/right.c", true, true, false},
		{"library metadata without folder", "src/world/right.c", true, true, false},
		{"partial library range", "src/lib/event/right.c", true, true, true},
	} {
		t.Run(test.name, func(t *testing.T) {
			p, config := shareAuditFixture(t, "int left(int n) { return n; }", "int right(int n) { return n; }", "")
			world, _ := config.module("world")
			world.Functions[0].Source = test.source
			writeTestFile(t, filepath.Join(p.root, filepath.FromSlash(test.source)), []byte("int right(int n) { return n; }"))
			code := mipsWords(0x03e00008, 0x00801021) // jr ra; move v0,a0
			for _, m := range config.Modules {
				m.Load, m.Size, m.File = m.Functions[0].Addr, len(code), m.ID+".bin"
				m.Functions[0].Size = len(code)
				m.SHA256 = fmt.Sprintf("%x", sha256.Sum256(code))
				writeTestFile(t, filepath.Join(p.filesRoot, m.File), code)
			}
			if test.metadata {
				start := world.Load
				if test.overlap {
					start += 4
				}
				world.Libraries = []librarySpec{{ID: "suzuki", Kind: "middleware", Addr: start, End: world.Load + 8}}
			}

			var diagnostics unitsReportDiagnostic
			functions := p.loadUnitFunctions(world, map[string]map[string]struct{}{}, map[string]bool{}, &diagnostics)
			if len(functions) != 1 || functions[0].library != test.library || len(diagnostics.UnreadableSources) != 0 {
				t.Fatalf("unit classification = %+v, diagnostics = %+v", functions, diagnostics)
			}
			share, err := p.shareAudit(config, shareAuditOptions{})
			if err != nil {
				t.Fatal(err)
			}
			twins, err := p.findTwins(config, twinsOptions{minBytes: 8})
			if err != nil {
				t.Fatal(err)
			}
			wantFunctions, wantGroups, wantExcluded := 2, 1, 0
			if test.library {
				wantFunctions, wantGroups, wantExcluded = 1, 0, 1
			}
			if len(share.Groups) != wantGroups || share.Excluded["library"] != wantExcluded {
				t.Fatalf("sharing: groups %d, excluded %v", len(share.Groups), share.Excluded)
			}
			if twins.Totals.Functions != wantFunctions || len(twins.Groups) != wantGroups {
				t.Fatalf("twins: functions %d, groups %d", twins.Totals.Functions, len(twins.Groups))
			}
		})
	}
}
