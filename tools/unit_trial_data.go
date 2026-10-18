package main

// Scratch data additions let ownership trials retain the existing unit without
// editing target metadata or copying the project. Expected hashes come only
// from the verified original image; acceptance still requires make checksums.

import (
	"fmt"
	"sort"
)

type unitTrialDataYAML struct {
	Addr uint32 `yaml:"addr"`
	Size int    `yaml:"size"`
}

func (p project) addUnitTrialData(unit *unitInstance, additions []unitTrialDataYAML) error {
	image, err := p.readModuleImage(unit.module)
	if err != nil {
		return err
	}
	return extendUnitTrialData(unit, additions, image)
}

func extendUnitTrialData(unit *unitInstance, additions []unitTrialDataYAML, image []byte) error {
	m := unit.module
	if len(image) != m.Size {
		return fmt.Errorf("%s: trial data image has an unexpected size", m.ID)
	}
	combined := append([]rodataSpec(nil), unit.data...)
	for _, addition := range additions {
		if addition.Size <= 0 || !m.contains(addition.Addr, addition.Size) {
			return fmt.Errorf("%s: trial data range at 0x%08x is invalid or outside the module", m.ID, addition.Addr)
		}
		overlaps := func(addr uint32, size int) bool {
			return uint64(addition.Addr) < uint64(addr)+uint64(size) && uint64(addr) < uint64(addition.Addr)+uint64(addition.Size)
		}
		for _, function := range m.Functions {
			if overlaps(function.Addr, function.Size) {
				return fmt.Errorf("%s: trial data overlaps original function %s", m.ID, function.Name)
			}
			for _, span := range function.Rodata {
				if overlaps(span.Addr, span.Size) {
					return fmt.Errorf("%s: trial data overlaps original rodata of %s", m.ID, function.Name)
				}
			}
		}
		for _, owner := range m.Units {
			for _, spans := range [][]rodataSpec{owner.Data, owner.Rodata} {
				for _, span := range spans {
					if overlaps(span.Addr, span.Size) {
						return fmt.Errorf("%s: trial data overlaps original data/rodata of unit %s", m.ID, owner.Name)
					}
				}
			}
		}
		// This also covers scratch rodata and earlier additions, which are not
		// necessarily present in the original configuration.
		for _, spans := range [][]rodataSpec{unit.rodata, combined} {
			for _, span := range spans {
				if overlaps(span.Addr, span.Size) {
					return fmt.Errorf("%s: trial data ranges overlap at 0x%08x", m.ID, addition.Addr)
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
			return fmt.Errorf("%s: trial data ranges are not contiguous at 0x%08x", m.ID, combined[i].Addr)
		}
	}
	unit.data = combined
	return nil
}
