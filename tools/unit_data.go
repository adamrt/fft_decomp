// unit_data.go carries checked unit-owned tables into the rebuilt module.
package main

import "fmt"

type compiledDataSpan struct {
	section string
	addr    uint32
	data    []byte
}

// sliceUnitOwnedData runs after compareUnitSections has checked the section
// extents and hashes. Read-only tables share a section with function literals;
// neither those literals nor alignment gaps belong to the returned spans.
func sliceUnitOwnedData(unit *unitInstance, sections map[string][]byte) []compiledDataSpan {
	var result []compiledDataSpan
	for _, section := range []struct {
		name  string
		owned []rodataSpec
		all   []rodataSpec
	}{{".data", unit.data, unit.data}, {".rodata", unit.spec.rodata, unit.rodata}, {".bss", unit.bss, unit.bss}} {
		for _, span := range section.owned {
			start := int(span.Addr - section.all[0].Addr)
			result = append(result, compiledDataSpan{section.name, span.Addr, sections[section.name][start : start+span.Size]})
		}
	}
	return result
}

// writeUnitOwnedData compares compiled tables directly with the verified
// original, then writes exactly their declared ranges into the output image.
func writeUnitOwnedData(m *moduleSpec, unit *unitDef, compiled []compiledDataSpan, original, output []byte) error {
	if len(original) != m.Size || len(output) != m.Size {
		return fmt.Errorf("%s: unit %s data output requires complete module images", m.ID, unit.Name)
	}
	if len(compiled) != len(unit.Data)+len(unit.Rodata)+len(unit.Bss) {
		return fmt.Errorf("%s: unit %s emitted %d owned data spans, expected %d", m.ID, unit.Name, len(compiled), len(unit.Data)+len(unit.Rodata)+len(unit.Bss))
	}
	if err := validateBSSOriginal(m, unit.Bss, original, true); err != nil {
		return err
	}
	index := 0
	for _, section := range []struct {
		name  string
		spans []rodataSpec
	}{{".data", unit.Data}, {".rodata", unit.Rodata}, {".bss", unit.Bss}} {
		for _, span := range section.spans {
			if span.Size <= 0 || !m.contains(span.Addr, span.Size) {
				return fmt.Errorf("%s: unit %s owned %s at 0x%08x lies outside the module image", m.ID, unit.Name, section.name, span.Addr)
			}
			got := compiled[index]
			index++
			if got.section != section.name || got.addr != span.Addr {
				return fmt.Errorf("%s: unit %s did not emit owned %s at 0x%08x", m.ID, unit.Name, section.name, span.Addr)
			}
			start := m.offset(span.Addr)
			expected := original[start : start+span.Size]
			if shortHash(expected) != span.Hash {
				return fmt.Errorf("%s: unit %s owned %s at 0x%08x hash does not match the original bytes; run `make checksums`", m.ID, unit.Name, section.name, span.Addr)
			}
			if offset, differs := mismatch(expected, got.data); differs {
				return fmt.Errorf("%s: unit %s %s at 0x%08x mismatch at span offset 0x%x (file offset 0x%x)", m.ID, unit.Name, section.name, span.Addr, offset, start+offset)
			}
		}
	}
	// Validate every span before changing the destination.
	for _, span := range compiled {
		start := m.offset(span.addr)
		copy(output[start:start+len(span.data)], span.data)
	}
	return nil
}

// validateUnitOwnedLayout prevents table writes from claiming function text,
// writable/read-only aliases or another unit's tables. Function literals may
// overlap owned read-only tables: BATTLE links such shared literals in two units,
// and the build compares both compiled copies against the same original bytes.
func validateUnitOwnedLayout(m *moduleSpec) error {
	type claimedRange struct {
		start, end uint64
		owner      string
	}
	var owned []claimedRange
	for _, unit := range m.Units {
		for _, section := range []struct {
			name  string
			spans []rodataSpec
		}{{".data", unit.Data}, {".rodata", unit.Rodata}, {".bss", unit.Bss}} {
			spans := section.spans
			if section.name == ".bss" && len(spans) > 0 {
				addr, size, err := bssEnvelope(m, spans)
				if err != nil {
					return err
				}
				spans = []rodataSpec{{Addr: addr, Size: size}}
			}
			for _, span := range spans {
				owner := fmt.Sprintf("unit %s owned %s at 0x%08x", unit.Name, section.name, span.Addr)
				if span.Size <= 0 || !m.contains(span.Addr, span.Size) {
					return fmt.Errorf("%s: %s lies outside the module image", m.ID, owner)
				}
				start, end := uint64(span.Addr), uint64(span.Addr)+uint64(span.Size)
				for _, previous := range owned {
					if start < previous.end && previous.start < end {
						return fmt.Errorf("%s: %s overlaps %s", m.ID, owner, previous.owner)
					}
				}
				for _, f := range m.Functions {
					if start < uint64(f.Addr)+uint64(f.Size) && uint64(f.Addr) < end {
						return fmt.Errorf("%s: %s overlaps function %s", m.ID, owner, f.Name)
					}
					if section.name != ".rodata" {
						for _, literal := range f.Rodata {
							if start < uint64(literal.Addr)+uint64(literal.Size) && uint64(literal.Addr) < end {
								return fmt.Errorf("%s: %s overlaps function %s rodata", m.ID, owner, f.Name)
							}
						}
					}
				}
				owned = append(owned, claimedRange{start, end, owner})
			}
		}
	}
	return nil
}
