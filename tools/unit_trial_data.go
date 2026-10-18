package main

// Scratch data/rodata additions let ownership trials retain the existing unit without
// editing target metadata or copying the project. Expected hashes come only
// from the verified original image; acceptance still requires make checksums.

import (
	"fmt"
	"sort"
)

type unitTrialRangeYAML struct {
	Addr uint32 `yaml:"addr"`
	Size int    `yaml:"size"`
}

func (p project) addUnitTrialRanges(unit *unitInstance, data, rodata []unitTrialRangeYAML, bss ...[]unitTrialRangeYAML) error {
	image, err := p.readModuleImage(unit.module)
	if err != nil {
		return err
	}
	return extendUnitTrialRanges(unit, data, rodata, image, bss...)
}

func extendUnitTrialRanges(unit *unitInstance, data, rodata []unitTrialRangeYAML, image []byte, bss ...[]unitTrialRangeYAML) error {
	if len(image) != unit.module.Size {
		return fmt.Errorf("%s: trial range image has an unexpected size", unit.module.ID)
	}
	newData, err := extendUnitTrialSection(unit, "data", unit.data, append(append([]rodataSpec(nil), unit.rodata...), unit.bss...), data, image)
	if err != nil {
		return err
	}
	newRodata, err := extendUnitTrialSection(unit, "rodata", unit.rodata, append(append([]rodataSpec(nil), newData...), unit.bss...), rodata, image)
	if err != nil {
		return err
	}
	// Neither section changes if a later range or the other section is invalid.
	var additions []unitTrialRangeYAML
	for _, ranges := range bss {
		additions = append(additions, ranges...)
	}
	newBss, err := extendUnitTrialSection(unit, "bss", unit.bss, append(append([]rodataSpec(nil), newData...), newRodata...), additions, image)
	if err != nil {
		return err
	}
	if err := validateBSSOriginal(unit.module, newBss, image, true); err != nil {
		return err
	}
	if err := validateBSSLabels(unit.module, newBss); err != nil {
		return err
	}
	next := *unit
	next.data, next.rodata, next.bss = newData, newRodata, newBss
	if err := validateTrialBSSEnvelope(&next); err != nil {
		return err
	}
	unit.data, unit.rodata, unit.bss = newData, newRodata, newBss
	return nil
}

func extendUnitTrialSection(unit *unitInstance, section string, existing, other []rodataSpec, additions []unitTrialRangeYAML, image []byte) ([]rodataSpec, error) {
	m := unit.module
	combined := append([]rodataSpec(nil), existing...)
	for _, addition := range additions {
		if addition.Size <= 0 || !m.contains(addition.Addr, addition.Size) {
			return nil, fmt.Errorf("%s: trial %s range at 0x%08x is invalid or outside the module", m.ID, section, addition.Addr)
		}
		overlaps := func(addr uint32, size int) bool {
			return uint64(addition.Addr) < uint64(addr)+uint64(size) && uint64(addr) < uint64(addition.Addr)+uint64(addition.Size)
		}
		for _, function := range m.Functions {
			if overlaps(function.Addr, function.Size) {
				return nil, fmt.Errorf("%s: trial %s overlaps original function %s", m.ID, section, function.Name)
			}
			for _, span := range function.Rodata {
				if overlaps(span.Addr, span.Size) {
					return nil, fmt.Errorf("%s: trial %s overlaps original rodata of %s", m.ID, section, function.Name)
				}
			}
		}
		for _, owner := range m.Units {
			bssAddr, bssSize, err := bssEnvelope(m, owner.Bss)
			if err != nil {
				return nil, err
			}
			if bssSize > 0 && overlaps(bssAddr, bssSize) {
				return nil, fmt.Errorf("%s: trial %s overlaps bss envelope of unit %s", m.ID, section, owner.Name)
			}
			for _, spans := range [][]rodataSpec{owner.Data, owner.Rodata} {
				for _, span := range spans {
					if overlaps(span.Addr, span.Size) {
						return nil, fmt.Errorf("%s: trial %s overlaps original data/rodata of unit %s", m.ID, section, owner.Name)
					}
				}
			}
		}
		// This also covers the other scratch section and earlier additions, which are not
		// necessarily present in the original configuration.
		for _, spans := range [][]rodataSpec{other, combined} {
			for _, span := range spans {
				if overlaps(span.Addr, span.Size) {
					return nil, fmt.Errorf("%s: trial %s ranges overlap at 0x%08x", m.ID, section, addition.Addr)
				}
			}
		}
		start := m.offset(addition.Addr)
		combined = append(combined, rodataSpec{Addr: addition.Addr, Size: addition.Size, Hash: shortHash(image[start : start+addition.Size])})
	}
	sort.Slice(combined, func(i, j int) bool { return combined[i].Addr < combined[j].Addr })
	for i := 1; i < len(combined); i++ {
		end := uint64(combined[i-1].Addr) + uint64(combined[i-1].Size)
		if uint64(combined[i].Addr) < end || uint64(combined[i].Addr)-end >= 8 {
			return nil, fmt.Errorf("%s: trial %s ranges are not contiguous at 0x%08x", m.ID, section, combined[i].Addr)
		}
	}
	return combined, nil
}
