package main

import "testing"

// mipsScore is the asm-differ-compatible distance of compiled from target.
func mipsScore(target, compiled []byte) int {
	return scoreMIPSLines(trimTrailingNops(disassembleMIPS(target)), trimTrailingNops(disassembleMIPS(compiled)))
}

func scoreTestText(words ...uint32) []byte { return testWords(words...) }

func TestMIPSScoreMatchesAsmDifferPenalties(t *testing.T) {
	const (
		addiuSP    = 0x27bdffd8 // addiu sp,sp,-40
		swS0       = 0xafb00018 // sw s0,24(sp)
		swS0Other  = 0xafb0001c // sw s0,28(sp)
		moveS0A0   = 0x00808021 // move s0,a0
		moveS1A0   = 0x00808821 // move s1,a0
		jrRA       = 0x03e00008
		nop        = 0x00000000
		luiV0      = 0x3c02800e
		addiuV0V0  = 0x24420001 // addiu v0,v0,1
		addiuV0V01 = 0x24420002 // addiu v0,v0,2
	)
	base := scoreTestText(addiuSP, swS0, moveS0A0, luiV0, jrRA, nop)
	if score := mipsScore(base, base); score != 0 {
		t.Fatalf("identical text scored %d", score)
	}
	if score := mipsScore(base, scoreTestText(addiuSP, swS0, moveS1A0, luiV0, jrRA, nop)); score != scorePenaltyRegalloc {
		t.Fatalf("one register field scored %d, want %d", score, scorePenaltyRegalloc)
	}
	if score := mipsScore(base, scoreTestText(addiuSP, swS0Other, moveS0A0, luiV0, jrRA, nop)); score != 4*scorePenaltyStack {
		t.Fatalf("stack offset difference scored %d, want 4", score)
	}
	if score := mipsScore(base, scoreTestText(addiuSP, swS0, moveS0A0, addiuV0V0, luiV0, jrRA, nop)); score != scorePenaltyInsertion {
		t.Fatalf("an extra instruction scored %d, want %d", score, scorePenaltyInsertion)
	}
	if score := mipsScore(base, scoreTestText(addiuSP, moveS0A0, swS0, luiV0, jrRA, nop)); score != scorePenaltyReordering {
		t.Fatalf("a swapped pair scored %d, want %d", score, scorePenaltyReordering)
	}
	if score := mipsScore(scoreTestText(addiuV0V0), scoreTestText(addiuV0V01)); score != scorePenaltyRegalloc {
		t.Fatalf("immediate difference scored %d", score)
	}
	// A trailing nop that is not a delay slot is ignored, as in asm-differ.
	if score := mipsScore(base[:16], append(append([]byte(nil), base[:16]...), 0, 0, 0, 0)); score != 0 {
		t.Fatalf("trailing padding nop scored %d", score)
	}
}

func TestDecodeMIPSForms(t *testing.T) {
	for _, test := range []struct {
		word uint32
		pc   uint32
		want string
	}{
		{0x00000000, 0, "nop"},
		{0x00809021, 0, "move\ts2,a0"},
		{0x10a00012, 0x24, "beqz\ta1,0x70"},
		{0x0c05f056, 0x2c, "jal\t0x17c158"},
		{0x00021023, 0, "negu\tv0,v0"},
		{0x9245011e, 0, "lbu\ta1,286(s2)"},
		{0x34e72493, 0, "ori\ta3,a3,0x2493"},
		{0x00a70018, 0, "mult\ta1,a3"},
		{0x2402ffff, 0, "li\tv0,-1"},
		{0x03e00008, 0, "jr\tra"},
		{0x0040f809, 0, "jalr\tv0"},
		{0x48024800, 0, "mfc2\tv0,$9"},
	} {
		if got := decodeMIPS(test.word, test.pc).row(); got != test.want {
			t.Errorf("decode %08x = %q, want %q", test.word, got, test.want)
		}
	}
}
