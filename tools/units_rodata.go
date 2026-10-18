package main

import (
	"encoding/binary"
	"fmt"
	"sort"
)

// With compiler-emitted `.align 3` jump tables, one read-only section
// contribution gives every table the same linked address modulo 8. Different
// residues conflict with that single-contribution model; they do not identify
// the original source files.

// jumpTableAlignment returns addr mod 8 of the function's first read-only
// range whose every word is an address inside the function, or -1.
func jumpTableAlignment(image []byte, m *moduleSpec, f *functionSpec) int {
	for _, rodata := range f.Rodata {
		start := m.offset(rodata.Addr)
		if start < 0 || start+rodata.Size > len(image) || rodata.Size < 8 || rodata.Size%4 != 0 {
			continue
		}
		table := true
		for at := start; at < start+rodata.Size; at += 4 {
			word := binary.LittleEndian.Uint32(image[at:])
			if word < f.Addr || word >= f.Addr+uint32(f.Size) {
				table = false
				break
			}
		}
		if table {
			return int(rodata.Addr % 8)
		}
	}
	return -1
}

// rodataAlignmentEvidence applies the jump-table alignment constraint to
// consecutive table-bearing functions: a required cut when adjacent,
// otherwise +2 on every cut between them.
func (an *unitModuleAnalysis) rodataAlignmentEvidence(group *unitGroup) {
	previous := -1
	for i := group.first; i <= group.last; i++ {
		fn := &an.functions[i]
		if fn.jumpAlign < 0 {
			continue
		}
		if previous >= 0 && an.functions[previous].jumpAlign != fn.jumpAlign {
			detail := fmt.Sprintf("jump tables of %s (addr mod 8 = %d) and %s (%d) require different read-only alignment bases",
				an.functions[previous].f.Name, an.functions[previous].jumpAlign, fn.f.Name, fn.jumpAlign)
			if i == previous+1 {
				an.evidence[previous] = append(an.evidence[previous], unitEvidence{"rodata_object", 6, detail})
			} else {
				for cut := previous; cut < i; cut++ {
					an.evidence[cut] = append(an.evidence[cut], unitEvidence{"rodata_alignment", 2, detail})
				}
			}
		}
		previous = i
	}
}

// unitJumpTableConflict names two members incompatible with one read-only
// alignment base for 8-byte-aligned jump tables, or returns "".
func unitJumpTableConflict(image []byte, m *moduleSpec, members []*functionSpec) string {
	type aligned struct {
		name  string
		align int
	}
	var seen []aligned
	for _, f := range members {
		if align := jumpTableAlignment(image, m, f); align >= 0 {
			seen = append(seen, aligned{f.Name, align})
		}
	}
	sort.SliceStable(seen, func(i, j int) bool { return false })
	for _, item := range seen[min(1, len(seen)):] {
		if item.align != seen[0].align {
			return fmt.Sprintf("jump tables of %s (addr mod 8 = %d) and %s (%d) cannot share one read-only alignment base", seen[0].name, seen[0].align, item.name, item.align)
		}
	}
	return ""
}
