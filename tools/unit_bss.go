package main

import "fmt"

func validateBSSLabels(m *moduleSpec, spans []rodataSpec) error {
	for _, span := range spans {
		if m.dataAt(span.Addr) == nil {
			return fmt.Errorf("%s: bss at 0x%08x names no data row", m.ID, span.Addr)
		}
	}
	return nil
}

// BSS contracts describe reconstructed zero-backed ELF allocations, not the
// historical section provenance of bytes in the flat original image. Every
// consumer uses this same envelope, with every byte covered by adjacent contracts.
func bssEnvelope(m *moduleSpec, spans []rodataSpec) (uint32, int, error) {
	if len(spans) == 0 {
		return 0, 0, nil
	}
	var end uint64
	for i, span := range spans {
		if span.Size <= 0 || !m.contains(span.Addr, span.Size) {
			return 0, 0, fmt.Errorf("%s: bss at 0x%08x lies outside the module", m.ID, span.Addr)
		}
		if i > 0 && uint64(span.Addr) != end {
			return 0, 0, fmt.Errorf("%s: bss ranges are not contiguous at 0x%08x", m.ID, span.Addr)
		}
		end = uint64(span.Addr) + uint64(span.Size)
	}
	base := spans[0].Addr
	size := int(end - uint64(base))
	if !m.contains(base, size) {
		return 0, 0, fmt.Errorf("%s: bss envelope lies outside the module", m.ID)
	}
	return base, size, nil
}

func validateBSSOriginal(m *moduleSpec, spans []rodataSpec, image []byte, hashes bool) error {
	if len(spans) == 0 {
		return nil
	}
	base, size, err := bssEnvelope(m, spans)
	if err != nil {
		return err
	}
	if len(image) != m.Size {
		return fmt.Errorf("%s: bss requires the complete original image", m.ID)
	}
	start := m.offset(base)
	for i, value := range image[start : start+size] {
		if value != 0 {
			return fmt.Errorf("%s: original bss envelope is nonzero at 0x%08x", m.ID, uint64(base)+uint64(i))
		}
	}
	if hashes {
		for _, span := range spans {
			start := m.offset(span.Addr)
			if shortHash(image[start:start+span.Size]) != span.Hash {
				return fmt.Errorf("%s: bss original hash differs at 0x%08x; run `make checksums`", m.ID, span.Addr)
			}
		}
	}
	return nil
}

// Reuse the configured-layout checker for scratch envelope collisions, while
// removing only inherited BSS spans from their original owner. No existing
// initialized table or literal is removed from that check.
func validateTrialBSSEnvelope(unit *unitInstance) error {
	m := *unit.module
	m.Units = append([]unitDef(nil), m.Units...)
	for i := range m.Units {
		if len(unit.members) == 0 || m.Units[i].Name != unit.members[0].Unit {
			continue
		}
		var keep []rodataSpec
		for _, span := range m.Units[i].Bss {
			inherited := false
			for _, current := range unit.bss {
				if current == span {
					inherited = true
				}
			}
			if !inherited {
				keep = append(keep, span)
			}
		}
		m.Units[i].Bss = keep
	}
	// Scratch data/rodata have already been checked individually. Include their
	// spans here to prevent a BSS allocation from claiming one of them.
	// Existing ranges may be duplicated, so compare this envelope directly.
	base, size, err := bssEnvelope(&m, unit.bss)
	if err != nil || size == 0 {
		return err
	}
	for _, spans := range [][]rodataSpec{unit.data, unit.rodata} {
		for _, span := range spans {
			if uint64(base) < uint64(span.Addr)+uint64(span.Size) && uint64(span.Addr) < uint64(base)+uint64(size) {
				return fmt.Errorf("%s: bss envelope overlaps scratch data/rodata", m.ID)
			}
		}
	}
	m.Units = append(m.Units, unitDef{Name: "scratch_bss", Bss: unit.bss})
	return validateUnitOwnedLayout(&m)
}
