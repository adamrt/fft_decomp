package main

import (
	"encoding/binary"
	"testing"
)

func TestJumpTableAlignmentAndConflict(t *testing.T) {
	m := &moduleSpec{ID: "t", Load: 0x80010000, Size: 0x100}
	image := make([]byte, 0x100)
	f := &functionSpec{Addr: 0x80010000, Size: 0x40, Name: "a", Rodata: []rodataSpec{{Addr: 0x80010044, Size: 8}}}
	g := &functionSpec{Addr: 0x80010050, Size: 0x40, Name: "b", Rodata: []rodataSpec{{Addr: 0x800100a0, Size: 8}}}
	for _, at := range []int{0x44, 0x48} {
		binary.LittleEndian.PutUint32(image[at:], 0x80010010)
	}
	for _, at := range []int{0xa0, 0xa4} {
		binary.LittleEndian.PutUint32(image[at:], 0x80010060)
	}
	if align := jumpTableAlignment(image, m, f); align != 4 {
		t.Fatalf("a alignment = %d, want 4", align)
	}
	if align := jumpTableAlignment(image, m, g); align != 0 {
		t.Fatalf("b alignment = %d, want 0", align)
	}
	binary.LittleEndian.PutUint32(image[0x44:], 0x12345678)
	if align := jumpTableAlignment(image, m, f); align != -1 {
		t.Fatalf("non-table alignment = %d, want -1", align)
	}
	binary.LittleEndian.PutUint32(image[0x44:], 0x80010010)
	if conflict := unitJumpTableConflict(image, m, []*functionSpec{f, g}); conflict == "" {
		t.Fatal("expected a conflict between a and b")
	}
	if conflict := unitJumpTableConflict(image, m, []*functionSpec{f}); conflict != "" {
		t.Fatalf("unexpected conflict %q", conflict)
	}
}
