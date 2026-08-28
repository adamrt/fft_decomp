package main

import (
	"encoding/binary"
	"strings"
	"testing"
)

const (
	diffTestAddiuSP   = 0x27bdffd8 // addiu sp,sp,-40
	diffTestSwS0      = 0xafb00018 // sw s0,24(sp)
	diffTestSwS0Other = 0xafb0001c // sw s0,28(sp)
	diffTestMoveS0A0  = 0x00808021 // move s0,a0
	diffTestMoveS1A0  = 0x00808821 // move s1,a0
	diffTestLwV0      = 0x8e020010 // lw v0,16(s0)
	diffTestLwV1      = 0x8e030010 // lw v1,16(s0)
	diffTestAddiuV0   = 0x24420001 // addiu v0,v0,1
	diffTestAddiuV02  = 0x24420002 // addiu v0,v0,2
	diffTestOriV0     = 0x34420001 // ori v0,v0,0x1
	diffTestAdduV0    = 0x00851021 // addu v0,a0,a1
	diffTestBeqzV0    = 0x10400002 // beqz v0,.+12
	diffTestJrRA      = 0x03e00008
	diffTestNop       = 0x00000000
)

func diffTestRuns(t *testing.T, target, compiled []uint32) []diffRun {
	t.Helper()
	base := trimTrailingNops(disassembleMIPS(testWords(target...)))
	mine := trimTrailingNops(disassembleMIPS(testWords(compiled...)))
	return summarizeMIPSDiff(base, mine)
}

func TestDiffSummaryClassifiesRuns(t *testing.T) {
	prologue := []uint32{diffTestAddiuSP, diffTestSwS0, diffTestMoveS0A0, diffTestLwV0, diffTestAddiuV0, diffTestJrRA, diffTestNop}
	replace := func(index int, word uint32) []uint32 {
		words := append([]uint32(nil), prologue...)
		words[index] = word
		return words
	}
	cases := []struct {
		name     string
		compiled []uint32
		kind     string
		target   string
		mine     string
	}{
		{"register move", replace(2, diffTestMoveS1A0), "reg", "0x008-0x008", "0x008-0x008"},
		{"register load", replace(3, diffTestLwV1), "reg", "0x00c-0x00c", "0x00c-0x00c"},
		{"immediate", replace(4, diffTestAddiuV02), "imm", "0x010-0x010", "0x010-0x010"},
		{"stack offset", replace(1, diffTestSwS0Other), "imm", "0x004-0x004", "0x004-0x004"},
		{"opcode", replace(4, diffTestOriV0), "struct", "0x010-0x010", "0x010-0x010"},
		{"missing", append(append([]uint32(nil), prologue[:2]...), prologue[3:]...), "missing", "0x008-0x008", "@0x008"},
		{"extra", append(append(append([]uint32(nil), prologue[:2]...), diffTestAdduV0), prologue[2:]...), "extra", "@0x008", "0x008-0x008"},
	}
	for _, test := range cases {
		runs := diffTestRuns(t, prologue, test.compiled)
		if len(runs) != 1 {
			t.Fatalf("%s: %d runs, want 1: %+v", test.name, len(runs), runs)
		}
		run := runs[0]
		if run.kind != test.kind || diffSpan(run.baseStart, run.baseEnd) != test.target || diffSpan(run.mineStart, run.mineEnd) != test.mine {
			t.Fatalf("%s: kind %s target %s compiled %s, want %s %s %s", test.name, run.kind,
				diffSpan(run.baseStart, run.baseEnd), diffSpan(run.mineStart, run.mineEnd), test.kind, test.target, test.mine)
		}
	}
	if runs := diffTestRuns(t, prologue, prologue); len(runs) != 0 {
		t.Fatalf("identical text produced runs %+v", runs)
	}
}

func TestDiffSummaryRetargetsShiftedBranchesAndSplitsRuns(t *testing.T) {
	// An extra instruction ahead of a branch shifts its raw target; through
	// the alignment it still lands on the same target instruction.
	target := []uint32{diffTestBeqzV0, diffTestNop, diffTestAddiuV0, diffTestJrRA, diffTestNop}
	compiled := append([]uint32{diffTestAdduV0}, target...)
	runs := diffTestRuns(t, target, compiled)
	if len(runs) != 1 || runs[0].kind != "extra" || len(runs[0].rows) != 1 {
		t.Fatalf("shifted branch was reported: %+v", runs)
	}

	// Two separated mismatches form two runs; adjacent ones merge and take
	// the most severe kind.
	base := []uint32{diffTestAddiuSP, diffTestSwS0, diffTestMoveS0A0, diffTestLwV0, diffTestAddiuV0, diffTestAdduV0, diffTestJrRA, diffTestNop}
	mine := []uint32{diffTestAddiuSP, diffTestSwS0, diffTestMoveS1A0, diffTestLwV1, diffTestAddiuV0, diffTestAdduV0, diffTestOriV0, diffTestJrRA, diffTestNop}
	runs = diffTestRuns(t, base, mine)
	if len(runs) != 2 || runs[0].kind != "reg" || len(runs[0].rows) != 2 || runs[1].kind != "extra" {
		t.Fatalf("unexpected runs %+v", runs)
	}
	summary := formatDiffSummary(testWords(base...), testWords(mine...))
	for _, want := range []string{
		"diff summary: target 8 insns, compiled 9 insns (+1), score 110\n",
		"  2 runs (extra 1, reg 1); rows r2 >1\n",
		"0x008-0x00c     0x008-0x00c        2  reg      r2             r move s0,a0 / move s1,a0\n",
		"@0x018          0x018-0x018        1  extra    >1             > - / ori v0,v0,0x1\n",
	} {
		if !strings.Contains(summary, want) {
			t.Fatalf("summary missing %q:\n%s", want, summary)
		}
	}
	if !strings.Contains(formatDiffSummary(testWords(base...), testWords(base...)), "no mismatching instructions") {
		t.Fatal("identical summary did not say so")
	}
}

func TestDiffSummaryNamesReorderedRuns(t *testing.T) {
	// Swapping two instructions keeps both: asm-differ scores it as a
	// reordering, and the alignment splits it into an extra and a missing run.
	target := []uint32{diffTestAddiuSP, diffTestSwS0, diffTestLwV0, diffTestMoveS0A0, diffTestJrRA, diffTestNop}
	compiled := []uint32{diffTestAddiuSP, diffTestLwV0, diffTestSwS0, diffTestMoveS0A0, diffTestJrRA, diffTestNop}
	runs := diffTestRuns(t, target, compiled)
	if len(runs) != 2 || runs[0].kind != "reorder" || runs[1].kind != "reorder" {
		t.Fatalf("swap runs = %+v", runs)
	}
	summary := formatDiffSummary(testWords(target...), testWords(compiled...))
	if !strings.Contains(summary, "2 runs (reorder 2); rows <1 >1") || !strings.Contains(summary, "score 60") {
		t.Fatalf("reorder summary:\n%s", summary)
	}
	// A replacement is structural, and a lone loss is missing.
	compiled[2] = diffTestAdduV0
	runs = diffTestRuns(t, target, compiled)
	for _, run := range runs {
		if run.kind == "reorder" {
			t.Fatalf("replacement reported as reorder: %+v", runs)
		}
	}
	// Missing and extra rows in one run that are the same instruction.
	target = []uint32{diffTestAddiuSP, diffTestLwV0, diffTestSwS0, diffTestAdduV0, diffTestJrRA, diffTestNop}
	compiled = []uint32{diffTestAddiuSP, diffTestSwS0, diffTestAdduV0, diffTestLwV0, diffTestJrRA, diffTestNop}
	for _, run := range diffTestRuns(t, target, compiled) {
		if run.kind != "reorder" {
			t.Fatalf("moved load not a reorder: %+v", run)
		}
	}
}

// testWords lays out little-endian MIPS words.
func testWords(words ...uint32) []byte {
	data := make([]byte, len(words)*4)
	for index, word := range words {
		binary.LittleEndian.PutUint32(data[index*4:], word)
	}
	return data
}
