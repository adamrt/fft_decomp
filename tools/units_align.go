// units_align.go compares the function sequences of two modules. Unique
// current source paths, code hashes and names anchor an order-preserving
// alignment. Unmatched and reordered runs suggest candidate boundaries, but
// can also reflect edits or conditional function membership within one source.
package main

import (
	"fmt"
	"sort"
)

const (
	unitAlignMinimumMatches = 5
	unitAlignListedNames    = 12
)

type unitAlignPair struct{ a, b int }

func alignAllModules(config *projectConfig, analyses map[string]*unitModuleAnalysis) {
	for i, a := range config.Modules {
		for _, b := range config.Modules[i+1:] {
			alignModulePair(analyses[a.ID], analyses[b.ID])
		}
	}
}

// unitAlignmentKeys indexes a module's non-library functions by explicit
// source path, by code hash and by name.
func unitAlignmentKeys(an *unitModuleAnalysis) (source, hash, name map[string][]int) {
	source, hash, name = map[string][]int{}, map[string][]int{}, map[string][]int{}
	for i := range an.functions {
		fn := &an.functions[i]
		if fn.library {
			continue
		}
		if fn.f.Source != "" {
			source[fn.f.Source] = append(source[fn.f.Source], i)
		}
		hash[fn.f.Hash] = append(hash[fn.f.Hash], i)
		name[fn.f.Name] = append(name[fn.f.Name], i)
	}
	return source, hash, name
}

func alignModulePair(an, bn *unitModuleAnalysis) {
	sourceA, hashA, nameA := unitAlignmentKeys(an)
	sourceB, hashB, nameB := unitAlignmentKeys(bn)
	matchedA, matchedB := map[int]bool{}, map[int]bool{}
	keyKinds := map[string]int{"source": 0, "hash": 0, "name": 0}
	var pairs []unitAlignPair
	for _, kind := range []struct {
		name string
		a, b map[string][]int
	}{{"source", sourceA, sourceB}, {"hash", hashA, hashB}, {"name", nameA, nameB}} {
		keys := make([]string, 0, len(kind.a))
		for key := range kind.a {
			keys = append(keys, key)
		}
		sort.Strings(keys)
		for _, key := range keys {
			ia, ib := kind.a[key], kind.b[key]
			if len(ia) != 1 || len(ib) != 1 || matchedA[ia[0]] || matchedB[ib[0]] {
				continue
			}
			matchedA[ia[0]], matchedB[ib[0]] = true, true
			pairs = append(pairs, unitAlignPair{ia[0], ib[0]})
			keyKinds[kind.name]++
		}
	}
	if len(pairs) < unitAlignMinimumMatches {
		return
	}
	sort.Slice(pairs, func(i, j int) bool { return pairs[i].a < pairs[j].a })
	chain, reordered := longestIncreasingChain(pairs)
	if len(chain) < unitAlignMinimumMatches {
		return
	}
	alignment := unitsReportAlign{Matched: len(chain), Runs: 1, Keys: keyKinds, Divergences: []unitsReportDivergence{}}
	for k, pair := range chain {
		an.functions[pair.a].aligned[bn.m.ID] = bn.functions[pair.b].f.Name
		bn.functions[pair.b].aligned[an.m.ID] = an.functions[pair.a].f.Name
		if k == 0 {
			continue
		}
		previous := chain[k-1]
		ga, gb := pair.a-previous.a-1, pair.b-previous.b-1
		if ga == 0 && gb == 0 {
			continue
		}
		alignment.Runs++
		switch {
		case gb == 0:
			an.addAlignmentEvidence(previous.a, 3, fmt.Sprintf("%s's matched functions are adjacent; %d functions intervene here", bn.m.ID, ga))
			an.addAlignmentEvidence(pair.a-1, 3, fmt.Sprintf("%s matches again after %d intervening functions", bn.m.ID, ga))
			bn.addAlignmentEvidence(previous.b, 2, fmt.Sprintf("%s has %d intervening functions between these matches", an.m.ID, ga))
			alignment.Divergences = append(alignment.Divergences, unitsReportDivergence{Kind: "insertion", Module: an.m.ID,
				Start: hexAddr(an.functions[previous.a+1].f.Addr), End: hexAddr(an.functionEnd(pair.a - 1)),
				Functions: an.functionNames(previous.a+1, pair.a-1),
				Partner:   []string{bn.functions[previous.b].f.Name, bn.functions[pair.b].f.Name}})
		case ga == 0:
			bn.addAlignmentEvidence(previous.b, 3, fmt.Sprintf("%s's matched functions are adjacent; %d functions intervene here", an.m.ID, gb))
			bn.addAlignmentEvidence(pair.b-1, 3, fmt.Sprintf("%s matches again after %d intervening functions", an.m.ID, gb))
			an.addAlignmentEvidence(previous.a, 2, fmt.Sprintf("%s has %d intervening functions between these matches", bn.m.ID, gb))
			alignment.Divergences = append(alignment.Divergences, unitsReportDivergence{Kind: "insertion", Module: bn.m.ID,
				Start: hexAddr(bn.functions[previous.b+1].f.Addr), End: hexAddr(bn.functionEnd(pair.b - 1)),
				Functions: bn.functionNames(previous.b+1, pair.b-1),
				Partner:   []string{an.functions[previous.a].f.Name, an.functions[pair.a].f.Name}})
		default:
			if ga == gb {
				for step := 1; step <= ga; step++ {
					fa, fb := &an.functions[previous.a+step], &bn.functions[previous.b+step]
					fa.aligned[bn.m.ID], fa.positional[bn.m.ID] = fb.f.Name, true
					fb.aligned[an.m.ID], fb.positional[an.m.ID] = fa.f.Name, true
				}
			}
			alignment.Divergences = append(alignment.Divergences, unitsReportDivergence{Kind: "substitution", Module: "both",
				Start: hexAddr(an.functions[previous.a+1].f.Addr), End: hexAddr(an.functionEnd(pair.a - 1)),
				Functions: an.functionNames(previous.a+1, pair.a-1), Partner: bn.functionNames(previous.b+1, pair.b-1)})
		}
	}
	for start := 0; start < len(reordered); {
		end := start
		for end+1 < len(reordered) && reordered[end+1].a == reordered[end].a+1 && reordered[end+1].b == reordered[end].b+1 {
			end++
		}
		first, last := reordered[start], reordered[end]
		an.addReorderEvidence(first.a, last.a, fmt.Sprintf("%s places %d matched functions elsewhere in its sequence", bn.m.ID, end-start+1))
		bn.addReorderEvidence(first.b, last.b, fmt.Sprintf("%s places %d matched functions elsewhere in its sequence", an.m.ID, end-start+1))
		alignment.Divergences = append(alignment.Divergences, unitsReportDivergence{Kind: "reorder", Module: "both",
			Start: hexAddr(an.functions[first.a].f.Addr), End: hexAddr(an.functionEnd(last.a)),
			Functions: an.functionNames(first.a, last.a), Partner: bn.functionNames(first.b, last.b)})
		start = end + 1
	}
	alignDataRows(an, bn, &alignment)
	forA, forB := alignment, alignment
	forA.Partner, forB.Partner = bn.m.ID, an.m.ID
	an.alignments = append(an.alignments, forA)
	bn.alignments = append(bn.alignments, forB)
}

// longestIncreasingChain keeps the largest set of pairs that appear in the
// same order on both sides; the rest are reordered matches.
func longestIncreasingChain(pairs []unitAlignPair) (chain, reordered []unitAlignPair) {
	var tails []int
	parent := make([]int, len(pairs))
	for i := range pairs {
		low, high := 0, len(tails)
		for low < high {
			mid := (low + high) / 2
			if pairs[tails[mid]].b < pairs[i].b {
				low = mid + 1
			} else {
				high = mid
			}
		}
		parent[i] = -1
		if low > 0 {
			parent[i] = tails[low-1]
		}
		if low == len(tails) {
			tails = append(tails, i)
		} else {
			tails[low] = i
		}
	}
	inChain := make([]bool, len(pairs))
	if len(tails) > 0 {
		for i := tails[len(tails)-1]; i >= 0; i = parent[i] {
			inChain[i] = true
		}
	}
	for i, pair := range pairs {
		if inChain[i] {
			chain = append(chain, pair)
		} else {
			reordered = append(reordered, pair)
		}
	}
	return chain, reordered
}

func (an *unitModuleAnalysis) addAlignmentEvidence(after, weight int, detail string) {
	if after < 0 || after+1 >= len(an.functions) || an.functions[after].group != an.functions[after+1].group {
		return
	}
	an.evidence[after] = append(an.evidence[after], unitEvidence{"alignment_divergence", weight, detail})
}

func (an *unitModuleAnalysis) addReorderEvidence(first, last int, detail string) {
	for _, after := range []int{first - 1, last} {
		if after < 0 || after+1 >= len(an.functions) || an.functions[after].group != an.functions[after+1].group {
			continue
		}
		an.evidence[after] = append(an.evidence[after], unitEvidence{"alignment_reorder", 2, detail})
	}
}

// alignDataRows matches data rows by current name and finds changes in the
// distance between consecutive matches. These can arise from resized tables,
// alignment or different object layouts; they do not prove object boundaries.
func alignDataRows(an, bn *unitModuleAnalysis, alignment *unitsReportAlign) {
	byNameB := map[string]int{}
	for i := range bn.rows {
		for _, name := range bn.rows[i].names {
			byNameB[name] = i
		}
	}
	var pairs []unitAlignPair
	seenB := map[int]bool{}
	for i := range an.rows {
		for _, name := range an.rows[i].names {
			if j, ok := byNameB[name]; ok && !seenB[j] {
				seenB[j] = true
				pairs = append(pairs, unitAlignPair{i, j})
				break
			}
		}
	}
	chain, _ := longestIncreasingChain(pairs)
	alignment.DataMatched = len(chain)
	for k := 1; k < len(chain); k++ {
		previous, pair := chain[k-1], chain[k]
		deltaPrevious := bn.rows[previous.b].addr - an.rows[previous.a].addr
		delta := bn.rows[pair.b].addr - an.rows[pair.a].addr
		if delta == deltaPrevious {
			continue
		}
		alignment.DataShifts++
		an.addDataShiftEvidence(&an.rows[previous.a], &an.rows[pair.a],
			fmt.Sprintf("the distance between %s and %s differs from %s's", an.rows[previous.a].names[0], an.rows[pair.a].names[0], bn.m.ID))
		bn.addDataShiftEvidence(&bn.rows[previous.b], &bn.rows[pair.b],
			fmt.Sprintf("the distance between %s and %s differs from %s's", bn.rows[previous.b].names[0], bn.rows[pair.b].names[0], an.m.ID))
	}
}

// addDataShiftEvidence projects a data-distance change onto candidate cuts
// between nearby users of the two rows. Shared data can produce the same pattern.
func (an *unitModuleAnalysis) addDataShiftEvidence(before, after *unitDataRow, detail string) {
	if before.wide || after.wide || len(before.owners) == 0 || len(after.owners) == 0 {
		return
	}
	first, last := before.hi(), after.lo()
	if first >= last || last-first > 4 {
		return
	}
	kind, weight := "data_shift", 3
	if last-first > 1 {
		kind, weight = "data_shift_interval", 1
	}
	for cut := first; cut < last; cut++ {
		if an.functions[cut].group != an.functions[cut+1].group {
			continue
		}
		an.evidence[cut] = append(an.evidence[cut], unitEvidence{kind, weight, detail})
	}
}

func (an *unitModuleAnalysis) functionEnd(index int) uint32 {
	f := an.functions[index].f
	return f.Addr + uint32(f.Size)
}

func (an *unitModuleAnalysis) functionNames(from, to int) []string {
	names := []string{}
	for i := from; i <= to; i++ {
		if len(names) == unitAlignListedNames {
			names = append(names, fmt.Sprintf("(+%d more)", to-i+1))
			break
		}
		names = append(names, an.functions[i].f.Name)
	}
	return names
}
