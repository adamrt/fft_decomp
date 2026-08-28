package main

import (
	"encoding/binary"
	"fmt"
	"strings"
)

// mismatchDiagnostics explains the first difference for `make diff`; it is
// advisory, the byte comparison decides.
func mismatchDiagnostics(expected, actual []byte, offset int, address uint64) string {
	var out strings.Builder
	fmt.Fprintf(&out, "  sizes: target %d bytes, compiled %d bytes; first difference +0x%x (%s alignment)\n", len(expected), len(actual), offset, alignmentDescription(offset))
	if offset == 0 {
		out.WriteString("  boundary: mismatch starts at the function boundary\n")
	} else {
		fmt.Fprintf(&out, "  boundary: mismatch is inside the function (instruction +0x%x)\n", offset&^3)
	}
	fmt.Fprintf(&out, "  context (target / compiled):\n%s", instructionContext(expected, actual, offset, address))
	if offset >= 4 && isControlTransfer(wordAt(expected, offset-4)) {
		out.WriteString("  hint: the preceding target instruction is a control transfer; inspect its delay slot\n")
	}
	if offset+4 <= len(expected) && offset+4 <= len(actual) {
		if fields := changedRegisterFields(wordAt(expected, offset), wordAt(actual, offset)); fields != "" {
			fmt.Fprintf(&out, "  hint: register fields differ (%s); this may be allocation or expression-order related\n", fields)
		}
	}
	return out.String()
}

func alignmentDescription(offset int) string {
	if offset%4 == 0 {
		return "instruction-aligned"
	}
	return "mid-instruction"
}

func wordAt(code []byte, offset int) uint32 {
	if offset < 0 || offset+4 > len(code) {
		return 0
	}
	return binary.LittleEndian.Uint32(code[offset : offset+4])
}

func instructionContext(expected, actual []byte, offset int, address uint64) string {
	start := offset &^ 3
	if start >= 8 {
		start -= 8
	}
	var out strings.Builder
	for current := start; current < offset+12; current += 4 {
		if current >= len(expected) && current >= len(actual) {
			break
		}
		marker := " "
		if current == offset&^3 {
			marker = ">"
		}
		left, right := "<truncated>", "<truncated>"
		if current+4 <= len(expected) {
			left = fmt.Sprintf("0x%08x", wordAt(expected, current))
		}
		if current+4 <= len(actual) {
			right = fmt.Sprintf("0x%08x", wordAt(actual, current))
		}
		fmt.Fprintf(&out, "    %s 0x%08x +0x%x: %s / %s\n", marker, address+uint64(current), current, left, right)
	}
	return out.String()
}

func isControlTransfer(word uint32) bool {
	op := word >> 26
	return word&0x3f == 8 || word&0x3f == 9 || op == 1 || op == 2 || op == 3 || op >= 4 && op <= 7 || op >= 20 && op <= 23
}

func changedRegisterFields(expected, actual uint32) string {
	if expected == actual {
		return ""
	}
	var fields []string
	if expected>>21&31 != actual>>21&31 {
		fields = append(fields, fmt.Sprintf("rs $%d->$%d", expected>>21&31, actual>>21&31))
	}
	if expected>>16&31 != actual>>16&31 {
		fields = append(fields, fmt.Sprintf("rt $%d->$%d", expected>>16&31, actual>>16&31))
	}
	if expected>>11&31 != actual>>11&31 {
		fields = append(fields, fmt.Sprintf("rd $%d->$%d", expected>>11&31, actual>>11&31))
	}
	return strings.Join(fields, ", ")
}
