package main

// Discovery proposes candidate ranges; agreement with configured function
// starts is a consistency check, not independent proof of code completeness.

import (
	"encoding/binary"
	"fmt"
	"sort"
	"strings"
)

const discoveryJRRA = uint32(0x03e00008)

type discoveryEndKind string

const (
	discoveryReturnEnd    discoveryEndKind = "return"
	discoveryKnownEnd     discoveryEndKind = "known-start"
	discoveryImageEnd     discoveryEndKind = "image-end"
	discoveryTruncatedEnd discoveryEndKind = "truncated-return"
)

type discoveredFunction struct {
	Start, End uint64 // Runtime addresses; End is exclusive.
	EndKind    discoveryEndKind
}

// discoverFunctionRanges follows jr ra through its delay slot, skips padding,
// and splits at supplied known starts. An early return can still split one
// function, and a function beginning with NOPs can lose its true entry address.
// The executable header is not scanned, and a missing delay slot never
// extends a candidate beyond the input image.
func discoverFunctionRanges(data []byte, loadAddress uint64, fileBias int, extraStarts []uint64) ([]discoveredFunction, error) {
	if fileBias < 0 || fileBias > len(data) || fileBias%4 != 0 || loadAddress%4 != 0 || loadAddress > 0xffffffff {
		return nil, fmt.Errorf("invalid discovery address model: load=0x%x bias=%d bytes=%d", loadAddress, fileBias, len(data))
	}
	wordCount := (len(data) - fileBias) / 4
	if uint64(wordCount)*4 > 0x100000000-loadAddress {
		return nil, fmt.Errorf("discovery input exceeds the 32-bit address space")
	}
	forced := make(map[uint64]bool, len(extraStarts))
	for _, address := range extraStarts {
		forced[address] = true
	}
	var found []discoveredFunction
	start := -1
	for index := 0; index < wordCount; {
		address := loadAddress + uint64(index)*4
		offset := fileBias + index*4
		word := binary.LittleEndian.Uint32(data[offset : offset+4])
		if start < 0 {
			if word == 0 {
				index++
				continue
			}
			start = index
		} else if forced[address] && index != start {
			found = append(found, discoveredFunction{loadAddress + uint64(start)*4, address, discoveryKnownEnd})
			start = index
		}
		if word == discoveryJRRA {
			end, kind := index+2, discoveryReturnEnd
			if end > wordCount {
				end, kind = wordCount, discoveryTruncatedEnd
			}
			found = append(found, discoveredFunction{loadAddress + uint64(start)*4, loadAddress + uint64(end)*4, kind})
			start, index = -1, end
			continue
		}
		index++
	}
	if start >= 0 {
		found = append(found, discoveredFunction{loadAddress + uint64(start)*4, loadAddress + uint64(wordCount)*4, discoveryImageEnd})
	}
	return found, nil
}

type discoveryAddress struct {
	Module  string
	Address uint64
}

type discoveryLibraryRange struct {
	Module, Library string
	Start, End      uint64
}

type discoveryExclusions struct {
	Known     map[discoveryAddress]int
	Libraries []discoveryLibraryRange
}

// discoveryExclusionsFromConfig keeps overlay address spaces separate.
func discoveryExclusionsFromConfig(config *projectConfig) discoveryExclusions {
	exclusions := discoveryExclusions{Known: make(map[discoveryAddress]int)}
	for _, m := range config.Modules {
		for _, f := range m.Functions {
			exclusions.Known[discoveryAddress{m.ID, uint64(f.Addr)}] = f.Size
		}
		for _, library := range m.Libraries {
			exclusions.Libraries = append(exclusions.Libraries, discoveryLibraryRange{m.ID, library.ID, uint64(library.Addr), uint64(library.End)})
		}
	}
	return exclusions
}

func (exclusions discoveryExclusions) knownForModule(moduleID string) map[uint64]int {
	known := make(map[uint64]int)
	for key, size := range exclusions.Known {
		if key.Module == moduleID {
			known[key.Address] = size
		}
	}
	return known
}

type discoveryKnownBoundary struct {
	Address uint64
	Bytes   int
}

type discoverySizeDisagreement struct {
	Address       uint64
	KnownBytes    int
	DetectedBytes uint64
}

type discoveryValidation struct {
	Known, Rediscovered int
	Missing             []discoveryKnownBoundary
	SizeDisagreements   []discoverySizeDisagreement
}

func validateDiscoveryBoundaries(moduleID string, found []discoveredFunction, exclusions discoveryExclusions) discoveryValidation {
	known := exclusions.knownForModule(moduleID)
	report := discoveryValidation{Known: len(known)}
	detected := make(map[uint64]uint64)
	for _, function := range found {
		detected[function.Start] = function.End - function.Start
	}
	for address, size := range known {
		actual, exists := detected[address]
		if !exists {
			report.Missing = append(report.Missing, discoveryKnownBoundary{address, size})
			continue
		}
		report.Rediscovered++
		if actual != uint64(size) {
			report.SizeDisagreements = append(report.SizeDisagreements, discoverySizeDisagreement{address, size, actual})
		}
	}
	sort.Slice(report.Missing, func(i, j int) bool { return report.Missing[i].Address < report.Missing[j].Address })
	sort.Slice(report.SizeDisagreements, func(i, j int) bool { return report.SizeDisagreements[i].Address < report.SizeDisagreements[j].Address })
	return report
}

// discoveryCodeExtent is an advisory envelope around the known functions,
// not a code/data classification. Configured byte counts are exclusive.
func discoveryCodeExtent(known map[uint64]int) *discoveredFunction {
	var extent *discoveredFunction
	for start, size := range known {
		if size <= 0 {
			continue
		}
		if extent == nil {
			extent = &discoveredFunction{Start: start, End: start + uint64(size)}
		} else {
			extent.Start = min(extent.Start, start)
			extent.End = max(extent.End, start+uint64(size))
		}
	}
	return extent
}

type discoveryPoolOptions struct {
	MinBytes, MaxBytes uint64
	Extent             *discoveredFunction
}

func defaultDiscoveryPoolOptions() discoveryPoolOptions {
	return discoveryPoolOptions{MinBytes: 12, MaxBytes: 800}
}

type discoveryPoolCandidate struct {
	Module string
	Start  uint64
	End    uint64
}

type discoveryPool struct {
	Candidates                               []discoveryPoolCandidate
	Known, Library, Outside, Size, Truncated int
}

func discoveryCandidatePool(moduleID string, found []discoveredFunction, exclusions discoveryExclusions, options discoveryPoolOptions) (discoveryPool, error) {
	var pool discoveryPool
	if moduleID == "" || strings.ContainsAny(moduleID, "\t\r\n ") || options.MaxBytes < options.MinBytes {
		return pool, fmt.Errorf("invalid discovery pool module or size bounds")
	}
	known := exclusions.knownForModule(moduleID)
	for _, function := range found {
		if function.End <= function.Start {
			return pool, fmt.Errorf("invalid discovered range 0x%x-0x%x", function.Start, function.End)
		}
		if function.EndKind == discoveryTruncatedEnd {
			pool.Truncated++
			continue
		}
		overlapsKnown := false
		for start, size := range known {
			if function.Start < start+uint64(size) && start < function.End {
				overlapsKnown = true
				break
			}
		}
		if overlapsKnown {
			pool.Known++
			continue
		}
		size := function.End - function.Start
		if size < options.MinBytes || size > options.MaxBytes {
			pool.Size++
			continue
		}
		if extent := options.Extent; extent != nil && (function.Start < extent.Start || function.Start >= extent.End) {
			pool.Outside++
			continue
		}
		owned := false
		for _, library := range exclusions.Libraries {
			if library.Module == moduleID && function.Start < library.End && library.Start < function.End {
				owned = true
				break
			}
		}
		if owned {
			pool.Library++
			continue
		}
		pool.Candidates = append(pool.Candidates, discoveryPoolCandidate{moduleID, function.Start, function.End})
	}
	sortDiscoveryCandidates(pool.Candidates)
	return pool, nil
}

func sortDiscoveryCandidates(candidates []discoveryPoolCandidate) {
	sort.Slice(candidates, func(i, j int) bool {
		if candidates[i].Module != candidates[j].Module {
			return candidates[i].Module < candidates[j].Module
		}
		if candidates[i].Start != candidates[j].Start {
			return candidates[i].Start < candidates[j].Start
		}
		return candidates[i].End < candidates[j].End
	})
}

func formatDiscoveryPoolRows(candidates []discoveryPoolCandidate) string {
	ordered := append([]discoveryPoolCandidate(nil), candidates...)
	sortDiscoveryCandidates(ordered)
	var output strings.Builder
	for _, candidate := range ordered {
		fmt.Fprintf(&output, "%s\t%d\t0x%08x\t0x%08x\tfunc_%08x\n", candidate.Module, candidate.End-candidate.Start, candidate.Start, candidate.End, candidate.Start)
	}
	return output.String()
}

// dropRegionPoolCandidates removes candidates that overlap a configured
// region: code the project deliberately does not reconstruct. Regions are
// module-qualified because overlays reuse runtime addresses, and half-open
// overlap catches scans that failed to split exactly at a boundary.
func dropRegionPoolCandidates(candidates []discoveryPoolCandidate, config *projectConfig) []discoveryPoolCandidate {
	kept := candidates[:0]
	for _, candidate := range candidates {
		m, _ := config.module(candidate.Module)
		if m != nil {
			if _, excluded := m.regionOverlapping(uint32(candidate.Start), uint32(candidate.End)); excluded {
				continue
			}
		}
		kept = append(kept, candidate)
	}
	return kept
}
