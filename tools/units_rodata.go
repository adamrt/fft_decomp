package main

import (
	"encoding/binary"
	"fmt"
	"sort"
)

// Jump tables are emitted with `.align 3`, so within one object every jump
// table sits at the same address modulo 8; the linker places objects'
// read-only sections at 4-byte alignment, so two tables at different
// residues came from different objects. That is a visible file boundary.

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

// rodataAlignmentEvidence adds boundary evidence between consecutive
// jump-table-bearing functions whose tables differ in alignment: a certain
// object boundary when they are adjacent, otherwise +2 on every cut between.
func (an *unitModuleAnalysis) rodataAlignmentEvidence(group *unitGroup) {
	previous := -1
	for i := group.first; i <= group.last; i++ {
		fn := &an.functions[i]
		if fn.jumpAlign < 0 {
			continue
		}
		if previous >= 0 && an.functions[previous].jumpAlign != fn.jumpAlign {
			detail := fmt.Sprintf("jump tables of %s (addr mod 8 = %d) and %s (%d) lie in different objects",
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

// unitJumpTableConflict names two members whose jump tables could not have
// shared one object, or returns "".
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
			return fmt.Sprintf("jump tables of %s (addr mod 8 = %d) and %s (%d) were separate objects", seen[0].name, seen[0].align, item.name, item.align)
		}
	}
	return ""
}
