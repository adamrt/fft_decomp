// diff_summary.go condenses a function's instruction diff into mismatch runs.
//
// It reuses mips_score.go's asm-differ-compatible alignment, so its rows and
// score follow the same rules as asm-differ's "CURRENT (N)" listing.
package main

import (
	"fmt"
	"strings"
)

// Row markers follow asm-differ's plain-format columns.
const (
	diffRowEqual     = ' '
	diffRowRegister  = 'r' // same opcode, only register fields differ
	diffRowImmediate = 'i' // same opcode, an immediate, offset or branch target differs
	diffRowOpcode    = '|' // aligned, but a different opcode
	diffRowMissing   = '<' // target instruction with no compiled counterpart
	diffRowExtra     = '>' // compiled instruction with no target counterpart
)

var diffRowMarkers = []byte{diffRowRegister, diffRowImmediate, diffRowOpcode, diffRowMissing, diffRowExtra}

type diffSummaryRow struct {
	marker     byte
	base, mine int // instruction indexes, -1 when absent
}

type diffRun struct {
	rows []diffSummaryRow
	// Target and compiled instruction index spans (inclusive); a side with
	// no rows reports the position its instructions would occupy.
	baseStart, baseEnd, mineStart, mineEnd int
	counts                                 map[byte]int
	kind                                   string
}

// runKind names the most severe mismatch in a run. Missing and extra rows
// whose instruction text also occurs on the other side somewhere in the diff
// are asm-differ's reorderings; a run made only of those (plus register or
// immediate rows) is "reorder": the code is there, the schedule differs.
func runKind(run diffRun, moved func(diffSummaryRow) bool) string {
	missing, extra := run.counts[diffRowMissing], run.counts[diffRowExtra]
	if run.counts[diffRowOpcode] == 0 && missing+extra > 0 {
		reordered := true
		for _, row := range run.rows {
			if (row.marker == diffRowMissing || row.marker == diffRowExtra) && !moved(row) {
				reordered = false
			}
		}
		if reordered {
			return "reorder"
		}
	}
	switch {
	case run.counts[diffRowOpcode] > 0 || missing > 0 && extra > 0:
		return "struct"
	case missing > 0:
		return "missing"
	case extra > 0:
		return "extra"
	case run.counts[diffRowImmediate] > 0:
		return "imm"
	}
	return "reg"
}

var mipsRegisterFields = func() map[string]bool {
	names := make(map[string]bool)
	for _, name := range mipsRegisterNames {
		names[name] = true
	}
	return names
}()

// classifyAlignedRow mirrors sameLinePenalties' field split: a same-opcode
// row whose differing fields are all registers is register-only.
func classifyAlignedRow(row mipsAlignedRow, base, mine []mipsLine) byte {
	switch {
	case row.base < 0:
		return diffRowExtra
	case row.mine < 0:
		return diffRowMissing
	case !row.sameMnemonic(base, mine):
		return diffRowOpcode
	}
	left, right := base[row.base], row.retargeted
	if left.row() == right.row() {
		return diffRowEqual
	}
	if leftOffset, ok := stackOffset(left.args); ok {
		if rightOffset, ok := stackOffset(right.args); ok && leftOffset != rightOffset {
			return diffRowImmediate
		}
	}
	split := func(args string) []string {
		if args == "" {
			return nil
		}
		var fields []string
		for _, field := range strings.Split(args, ",") {
			for _, part := range strings.Split(field, "(") {
				fields = append(fields, strings.TrimSuffix(part, ")"))
			}
		}
		return fields
	}
	leftFields, rightFields := split(left.args), split(right.args)
	if len(leftFields) != len(rightFields) {
		return diffRowImmediate
	}
	for index := range leftFields {
		if leftFields[index] == rightFields[index] {
			continue
		}
		if !isRegisterField(leftFields[index]) || !isRegisterField(rightFields[index]) {
			return diffRowImmediate
		}
	}
	return diffRowRegister
}

func isRegisterField(field string) bool {
	return mipsRegisterFields[field] || strings.HasPrefix(field, "$")
}

// summarizeMIPSDiff groups maximal runs of mismatching aligned rows.
func summarizeMIPSDiff(base, mine []mipsLine) []diffRun {
	var runs []diffRun
	var current *diffRun
	nextBase, nextMine := 0, 0
	for _, aligned := range alignMIPSLines(base, mine) {
		marker := classifyAlignedRow(aligned, base, mine)
		if marker == diffRowEqual {
			current = nil
		} else {
			if current == nil {
				runs = append(runs, diffRun{baseStart: nextBase, baseEnd: nextBase - 1, mineStart: nextMine, mineEnd: nextMine - 1, counts: map[byte]int{}})
				current = &runs[len(runs)-1]
			}
			current.rows = append(current.rows, diffSummaryRow{marker, aligned.base, aligned.mine})
			current.counts[marker]++
			if aligned.base >= 0 {
				current.baseEnd = aligned.base
			}
			if aligned.mine >= 0 {
				current.mineEnd = aligned.mine
			}
		}
		if aligned.base >= 0 {
			nextBase = aligned.base + 1
		}
		if aligned.mine >= 0 {
			nextMine = aligned.mine + 1
		}
	}
	missingRows, extraRows := map[string]bool{}, map[string]bool{}
	for _, run := range runs {
		for _, row := range run.rows {
			if row.marker == diffRowMissing {
				missingRows[base[row.base].row()] = true
			} else if row.marker == diffRowExtra {
				extraRows[mine[row.mine].row()] = true
			}
		}
	}
	moved := func(row diffSummaryRow) bool {
		if row.marker == diffRowMissing {
			return extraRows[base[row.base].row()]
		}
		return missingRows[mine[row.mine].row()]
	}
	for index := range runs {
		runs[index].kind = runKind(runs[index], moved)
	}
	return runs
}

func diffSpan(start, end int) string {
	if end < start {
		return fmt.Sprintf("@0x%03x", start*4) // insertion point: no instructions on this side
	}
	return fmt.Sprintf("0x%03x-0x%03x", start*4, end*4)
}

func diffSampleSide(lines []mipsLine, index int) string {
	if index < 0 {
		return "-"
	}
	return strings.ReplaceAll(lines[index].row(), "\t", " ")
}

// diffRunSample shows the run's first row of its most severe marker. With
// no changed opcode but both missing and extra rows, it pairs the first of
// each, which is what a reader compares.
func diffRunSample(run diffRun, base, mine []mipsLine) string {
	first := func(marker byte) (diffSummaryRow, bool) {
		for _, row := range run.rows {
			if row.marker == marker {
				return row, true
			}
		}
		return diffSummaryRow{}, false
	}
	if _, changed := first(diffRowOpcode); !changed {
		missing, hasMissing := first(diffRowMissing)
		extra, hasExtra := first(diffRowExtra)
		if hasMissing && hasExtra {
			return fmt.Sprintf("<> %s / %s", diffSampleSide(base, missing.base), diffSampleSide(mine, extra.mine))
		}
	}
	for _, marker := range []byte{diffRowOpcode, diffRowMissing, diffRowExtra, diffRowImmediate, diffRowRegister} {
		if row, ok := first(marker); ok {
			return fmt.Sprintf("%c %s / %s", marker, diffSampleSide(base, row.base), diffSampleSide(mine, row.mine))
		}
	}
	return ""
}

func diffMarkerCounts(counts map[byte]int) string {
	var parts []string
	for _, marker := range diffRowMarkers {
		if counts[marker] > 0 {
			parts = append(parts, fmt.Sprintf("%c%d", marker, counts[marker]))
		}
	}
	return strings.Join(parts, " ")
}

// formatDiffSummary renders the run table for target and compiled text.
// Offsets are relative to the function start, like the asm-differ listing.
func formatDiffSummary(target, compiled []byte) string {
	base := trimTrailingNops(disassembleMIPS(target))
	mine := trimTrailingNops(disassembleMIPS(compiled))
	runs := summarizeMIPSDiff(base, mine)
	var out strings.Builder
	fmt.Fprintf(&out, "diff summary: target %d insns, compiled %d insns (%+d), score %d\n", len(base), len(mine), len(mine)-len(base), scoreMIPSLines(base, mine))
	rows := map[byte]int{}
	kinds := map[string]int{}
	for _, run := range runs {
		for marker, count := range run.counts {
			rows[marker] += count
		}
		kinds[run.kind]++
	}
	var kindParts []string
	for _, kind := range []string{"struct", "reorder", "missing", "extra", "imm", "reg"} {
		if kinds[kind] > 0 {
			kindParts = append(kindParts, fmt.Sprintf("%s %d", kind, kinds[kind]))
		}
	}
	if len(runs) == 0 {
		out.WriteString("  no mismatching instructions\n")
		return out.String()
	}
	fmt.Fprintf(&out, "  %d runs (%s); rows %s\n", len(runs), strings.Join(kindParts, ", "), diffMarkerCounts(rows))
	fmt.Fprintf(&out, "  %-15s %-15s %4s  %-7s  %-14s %s\n", "target", "compiled", "len", "kind", "rows", "sample (target / compiled)")
	for _, run := range runs {
		fmt.Fprintf(&out, "  %-15s %-15s %4d  %-7s  %-14s %s\n",
			diffSpan(run.baseStart, run.baseEnd), diffSpan(run.mineStart, run.mineEnd), len(run.rows),
			run.kind, diffMarkerCounts(run.counts), diffRunSample(run, base, mine))
	}
	return out.String()
}
