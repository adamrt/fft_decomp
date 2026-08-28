// mips_score.go disassembles function text and scores it against its target.
//
// The score mirrors asm-differ's "CURRENT (N)": instructions are rendered in
// an objdump-like form, aligned by the same minimal edit script over
// mnemonics (rapidfuzz tie-breaking), branch targets are compared through
// that alignment, and penalties are 5 per differing field, 1 per stack-offset
// unit, 100 per unmatched line and 60 per reordered line. Rare alignments can
// still disagree with asm-differ; 0 always means byte-identical text.
package main

import (
	"encoding/binary"
	"fmt"
	"strconv"
	"strings"
)

const (
	scorePenaltyStack      = 1
	scorePenaltyRegalloc   = 5
	scorePenaltyReordering = 60
	scorePenaltyInsertion  = 100
	scorePenaltyDeletion   = 100
)

var mipsRegisterNames = [32]string{
	"zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
	"t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
	"s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
	"t8", "t9", "k0", "k1", "gp", "sp", "s8", "ra",
}

// mipsLine is one disassembled instruction: its mnemonic and argument text.
type mipsLine struct {
	mnemonic string
	args     string
	branch   bool   // conditional or PC-relative branch
	target   uint32 // branch destination offset when branch is set
}

func (l mipsLine) row() string {
	if l.args == "" {
		return l.mnemonic
	}
	return l.mnemonic + "\t" + l.args
}

func disassembleMIPS(code []byte) []mipsLine {
	lines := make([]mipsLine, 0, len(code)/4)
	for offset := 0; offset+4 <= len(code); offset += 4 {
		lines = append(lines, decodeMIPS(binary.LittleEndian.Uint32(code[offset:]), uint32(offset)))
	}
	return lines
}

func decodeMIPS(word, pc uint32) mipsLine {
	op := word >> 26
	rs := mipsRegisterNames[word>>21&31]
	rt := mipsRegisterNames[word>>16&31]
	rd := mipsRegisterNames[word>>11&31]
	rsn, rtn := word>>21&31, word>>16&31
	sa := word >> 6 & 31
	imm := int32(int16(word & 0xffff))
	uimm := word & 0xffff
	target := pc + 4 + uint32(imm<<2)
	branch := fmt.Sprintf("0x%x", target)
	line := func(mnemonic string, args ...string) mipsLine {
		return mipsLine{mnemonic: mnemonic, args: strings.Join(args, ",")}
	}
	branchLine := func(mnemonic string, args ...string) mipsLine {
		return mipsLine{mnemonic: mnemonic, args: strings.Join(append(args, branch), ","), branch: true, target: target}
	}
	memory := func(mnemonic, reg string) mipsLine {
		return line(mnemonic, reg, fmt.Sprintf("%d(%s)", imm, rs))
	}
	if word == 0 {
		return line("nop")
	}
	switch op {
	case 0:
		switch word & 63 {
		case 0:
			return line("sll", rd, rt, fmt.Sprintf("0x%x", sa))
		case 2:
			return line("srl", rd, rt, fmt.Sprintf("0x%x", sa))
		case 3:
			return line("sra", rd, rt, fmt.Sprintf("0x%x", sa))
		case 4:
			return line("sllv", rd, rt, rs)
		case 6:
			return line("srlv", rd, rt, rs)
		case 7:
			return line("srav", rd, rt, rs)
		case 8:
			return line("jr", rs)
		case 9:
			if word>>11&31 == 31 {
				return line("jalr", rs)
			}
			return line("jalr", rd, rs)
		case 12:
			return line("syscall")
		case 13:
			return line("break", fmt.Sprintf("0x%x", word>>16&0x3ff))
		case 16:
			return line("mfhi", rd)
		case 17:
			return line("mthi", rs)
		case 18:
			return line("mflo", rd)
		case 19:
			return line("mtlo", rs)
		case 24:
			return line("mult", rs, rt)
		case 25:
			return line("multu", rs, rt)
		case 26:
			return line("div", "zero", rs, rt)
		case 27:
			return line("divu", "zero", rs, rt)
		case 32:
			return line("add", rd, rs, rt)
		case 33:
			if rtn == 0 {
				return line("move", rd, rs)
			}
			return line("addu", rd, rs, rt)
		case 34:
			return line("sub", rd, rs, rt)
		case 35:
			if rsn == 0 {
				return line("negu", rd, rt)
			}
			return line("subu", rd, rs, rt)
		case 36:
			return line("and", rd, rs, rt)
		case 37:
			if rtn == 0 {
				return line("move", rd, rs)
			}
			return line("or", rd, rs, rt)
		case 38:
			return line("xor", rd, rs, rt)
		case 39:
			if rtn == 0 {
				return line("not", rd, rs)
			}
			return line("nor", rd, rs, rt)
		case 42:
			return line("slt", rd, rs, rt)
		case 43:
			return line("sltu", rd, rs, rt)
		}
	case 1:
		switch rtn {
		case 0:
			return branchLine("bltz", rs)
		case 1:
			return branchLine("bgez", rs)
		case 16:
			return branchLine("bltzal", rs)
		case 17:
			if rsn == 0 {
				return branchLine("bal")
			}
			return branchLine("bgezal", rs)
		}
	case 2, 3:
		destination := fmt.Sprintf("0x%x", (pc+4)&0xf0000000|(word&0x3ffffff)<<2)
		if op == 2 {
			return line("j", destination)
		}
		return line("jal", destination)
	case 4:
		if rsn == 0 && rtn == 0 {
			return branchLine("b")
		}
		if rtn == 0 {
			return branchLine("beqz", rs)
		}
		return branchLine("beq", rs, rt)
	case 5:
		if rtn == 0 {
			return branchLine("bnez", rs)
		}
		return branchLine("bne", rs, rt)
	case 6:
		return branchLine("blez", rs)
	case 7:
		return branchLine("bgtz", rs)
	case 8:
		return line("addi", rt, rs, strconv.Itoa(int(imm)))
	case 9:
		if rsn == 0 {
			return line("li", rt, strconv.Itoa(int(imm)))
		}
		return line("addiu", rt, rs, strconv.Itoa(int(imm)))
	case 10:
		return line("slti", rt, rs, strconv.Itoa(int(imm)))
	case 11:
		return line("sltiu", rt, rs, strconv.Itoa(int(imm)))
	case 12:
		return line("andi", rt, rs, fmt.Sprintf("0x%x", uimm))
	case 13:
		if rsn == 0 {
			return line("li", rt, fmt.Sprintf("0x%x", uimm))
		}
		return line("ori", rt, rs, fmt.Sprintf("0x%x", uimm))
	case 14:
		return line("xori", rt, rs, fmt.Sprintf("0x%x", uimm))
	case 15:
		return line("lui", rt, fmt.Sprintf("0x%x", uimm))
	case 16, 18:
		unit := strconv.Itoa(int(op - 16))
		if word>>25&1 == 1 {
			if op == 16 && word&63 == 16 {
				return line("rfe")
			}
			return line("c"+unit, fmt.Sprintf("0x%x", word&0x1ffffff))
		}
		register := strconv.Itoa(int(word >> 11 & 31))
		switch rsn {
		case 0:
			return line("mfc"+unit, rt, "$"+register)
		case 2:
			return line("cfc"+unit, rt, "$"+register)
		case 4:
			return line("mtc"+unit, rt, "$"+register)
		case 6:
			return line("ctc"+unit, rt, "$"+register)
		}
	case 32:
		return memory("lb", rt)
	case 33:
		return memory("lh", rt)
	case 34:
		return memory("lwl", rt)
	case 35:
		return memory("lw", rt)
	case 36:
		return memory("lbu", rt)
	case 37:
		return memory("lhu", rt)
	case 38:
		return memory("lwr", rt)
	case 40:
		return memory("sb", rt)
	case 41:
		return memory("sh", rt)
	case 42:
		return memory("swl", rt)
	case 43:
		return memory("sw", rt)
	case 46:
		return memory("swr", rt)
	case 50:
		return memory("lwc2", "$"+strconv.Itoa(int(rtn)))
	case 58:
		return memory("swc2", "$"+strconv.Itoa(int(rtn)))
	}
	return line(".word", fmt.Sprintf("0x%08x", word))
}

func trimTrailingNops(lines []mipsLine) []mipsLine {
	delay := map[string]bool{"j": true, "jal": true, "jr": true, "jalr": true, "bal": true, "b": true,
		"beq": true, "beqz": true, "bne": true, "bnez": true, "blez": true, "bgtz": true,
		"bltz": true, "bgez": true, "bltzal": true, "bgezal": true}
	for len(lines) > 0 && lines[len(lines)-1].mnemonic == "nop" && (len(lines) == 1 || !delay[lines[len(lines)-2].mnemonic]) {
		lines = lines[:len(lines)-1]
	}
	return lines
}

type editOp struct {
	tag            byte // 'e' equal, 'r' replace, 'd' delete (base only), 'i' insert (mine only)
	i1, i2, j1, j2 int
}

// levenshteinOpcodes returns a minimal edit script from a to b as opcodes.
func levenshteinOpcodes(a, b []string) []editOp {
	prefix := 0
	for prefix < len(a) && prefix < len(b) && a[prefix] == b[prefix] {
		prefix++
	}
	suffix := 0
	for suffix < len(a)-prefix && suffix < len(b)-prefix && a[len(a)-1-suffix] == b[len(b)-1-suffix] {
		suffix++
	}
	ca, cb := a[prefix:len(a)-suffix], b[prefix:len(b)-suffix]
	n, m := len(ca), len(cb)
	width := m + 1
	distance := make([]int32, (n+1)*width)
	for i := 0; i <= n; i++ {
		distance[i*width] = int32(i)
	}
	for j := 0; j <= m; j++ {
		distance[j] = int32(j)
	}
	for i := 1; i <= n; i++ {
		for j := 1; j <= m; j++ {
			cost := int32(1)
			if ca[i-1] == cb[j-1] {
				cost = 0
			}
			best := distance[(i-1)*width+j-1] + cost
			if value := distance[(i-1)*width+j] + 1; value < best {
				best = value
			}
			if value := distance[i*width+j-1] + 1; value < best {
				best = value
			}
			distance[i*width+j] = best
		}
	}
	// Backtrack exactly as rapidfuzz's Hyyrö alignment recovery does (the
	// Python Levenshtein package asm-differ uses): vertical deltas along a
	// decide deletions first, then insertions, then the diagonal.
	vertical := func(col, row int) int32 {
		return distance[col*width+row] - distance[(col-1)*width+row]
	}
	var steps []byte
	col, row := n, m
	for row != 0 && col != 0 {
		if vertical(col, row) == 1 {
			steps = append(steps, 'd')
			col--
			continue
		}
		row--
		if row != 0 && vertical(col, row) == -1 {
			steps = append(steps, 'i')
			continue
		}
		col--
		if ca[col] == cb[row] {
			steps = append(steps, 'e')
		} else {
			steps = append(steps, 'r')
		}
	}
	for ; col != 0; col-- {
		steps = append(steps, 'd')
	}
	for ; row != 0; row-- {
		steps = append(steps, 'i')
	}
	for left, right := 0, len(steps)-1; left < right; left, right = left+1, right-1 {
		steps[left], steps[right] = steps[right], steps[left]
	}
	all := make([]byte, 0, prefix+len(steps)+suffix)
	for range prefix {
		all = append(all, 'e')
	}
	all = append(all, steps...)
	for range suffix {
		all = append(all, 'e')
	}
	var ops []editOp
	ai, bj := 0, 0
	for _, step := range all {
		di, dj := 0, 0
		switch step {
		case 'e', 'r':
			di, dj = 1, 1
		case 'd':
			di = 1
		case 'i':
			dj = 1
		}
		if len(ops) > 0 && ops[len(ops)-1].tag == step {
			ops[len(ops)-1].i2 += di
			ops[len(ops)-1].j2 += dj
		} else {
			ops = append(ops, editOp{step, ai, ai + di, bj, bj + dj})
		}
		ai += di
		bj += dj
	}
	return ops
}

// mipsAlignedRow is one row of the asm-differ-style alignment: an index into
// the target (base) and/or compiled (mine) lines, -1 for an unmatched side.
// When both mnemonics agree and mine is a branch, mine's target is rewritten
// to where it lands in the alignment, so a branch that only moved with an
// earlier size change compares equal.
type mipsAlignedRow struct {
	base, mine int
	retargeted mipsLine
}

func (r mipsAlignedRow) sameMnemonic(base, mine []mipsLine) bool {
	return r.base >= 0 && r.mine >= 0 && base[r.base].mnemonic == mine[r.mine].mnemonic
}

func alignMIPSLines(base, mine []mipsLine) []mipsAlignedRow {
	baseMnemonics := make([]string, len(base))
	for index, line := range base {
		baseMnemonics[index] = line.mnemonic
	}
	mineMnemonics := make([]string, len(mine))
	for index, line := range mine {
		mineMnemonics[index] = line.mnemonic
	}
	var rows []mipsAlignedRow
	for _, op := range levenshteinOpcodes(baseMnemonics, mineMnemonics) {
		for k := range max(op.i2-op.i1, op.j2-op.j1) {
			entry := mipsAlignedRow{base: -1, mine: -1}
			if op.i1+k < op.i2 {
				entry.base = op.i1 + k
			}
			if op.j1+k < op.j2 {
				entry.mine = op.j1 + k
			}
			rows = append(rows, entry)
		}
	}
	// Like asm-differ, map each of our instructions to the aligned target
	// instruction (plus a count of unaligned followers) so a branch is scored
	// on where it lands in the alignment, not on its raw offset.
	type position struct{ base, extra int }
	mineToBase := map[int]position{}
	lineBase, lineExtra := -1, 0
	for _, entry := range rows {
		if entry.base >= 0 {
			lineBase, lineExtra = entry.base*4, 0
		} else {
			lineExtra++
		}
		if entry.mine >= 0 {
			mineToBase[entry.mine*4] = position{lineBase, lineExtra}
		}
	}
	for index, entry := range rows {
		if !entry.sameMnemonic(base, mine) {
			continue
		}
		right := mine[entry.mine]
		if right.branch {
			landing, ok := mineToBase[int(right.target)]
			if !ok {
				origin := mineToBase[entry.mine*4]
				landing = position{origin.base + int(right.target) - entry.mine*4, 0}
			}
			retargeted := fmt.Sprintf("0x%x", landing.base)
			if landing.extra != 0 {
				retargeted += fmt.Sprintf("+%d", landing.extra)
			}
			fields := strings.Split(right.args, ",")
			fields[len(fields)-1] = retargeted
			right.args = strings.Join(fields, ",")
		}
		rows[index].retargeted = right
	}
	return rows
}

func scoreMIPSLines(base, mine []mipsLine) int {
	stack, regalloc := 0, 0
	insertions := map[string]int{}
	deletions := map[string]int{}
	for _, entry := range alignMIPSLines(base, mine) {
		if entry.sameMnemonic(base, mine) {
			s, r := sameLinePenalties(base[entry.base], entry.retargeted)
			stack += s
			regalloc += r
			continue
		}
		if entry.base >= 0 {
			deletions[base[entry.base].row()]++
		}
		if entry.mine >= 0 {
			insertions[mine[entry.mine].row()]++
		}
	}
	reorderings, inserted, deleted := 0, 0, 0
	for row, count := range insertions {
		common := min(count, deletions[row])
		reorderings += common
		inserted += count - common
	}
	for row, count := range deletions {
		deleted += count - min(count, insertions[row])
	}
	return stack*scorePenaltyStack + regalloc*scorePenaltyRegalloc +
		reorderings*scorePenaltyReordering + inserted*scorePenaltyInsertion + deleted*scorePenaltyDeletion
}

func stackOffset(args string) (int, bool) {
	fields := strings.Split(args, ",")
	if len(fields) < 2 {
		return 0, false
	}
	last := fields[len(fields)-1]
	if !strings.HasSuffix(last, "(sp)") {
		return 0, false
	}
	value, err := strconv.ParseInt(strings.TrimSuffix(last, "(sp)"), 0, 64)
	if err != nil || value < 0 {
		return 0, false
	}
	return int(value), true
}

func sameLinePenalties(old, new mipsLine) (int, int) {
	if old.row() == new.row() {
		return 0, 0
	}
	stack := 0
	ignoreLast := false
	if oldOffset, ok := stackOffset(old.args); ok {
		if newOffset, ok := stackOffset(new.args); ok {
			stack = abs(oldOffset - newOffset)
			ignoreLast = true
		}
	}
	split := func(args string) []string {
		if args == "" {
			return nil
		}
		fields := strings.Split(args, ",")
		if ignoreLast {
			return fields[:len(fields)-1]
		}
		last := fields[len(fields)-1]
		return append(fields[:len(fields)-1], strings.Split(last, "(")...)
	}
	oldFields, newFields := split(old.args), split(new.args)
	regalloc := 0
	for index := range min(len(oldFields), len(newFields)) {
		if oldFields[index] != newFields[index] {
			regalloc++
		}
	}
	regalloc += abs(len(oldFields) - len(newFields))
	return stack, regalloc
}

func abs(value int) int {
	if value < 0 {
		return -value
	}
	return value
}
