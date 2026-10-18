package main

import (
	"debug/elf"
	"errors"
	"fmt"
)

// Keep definition evidence separate from emitted bytes. Every consumer of
// Sections still sees only allocatable sections, including unexpected ones.
type historicalResult struct {
	Sections    map[string][]byte               `json:"sections"`
	Definitions []historicalDefinition          `json:"definitions"`
	Allocations map[string]historicalAllocation `json:"allocations"`
	NoCommon    bool                            `json:"no_common"`
}

type historicalAllocation struct {
	Address uint64          `json:"address"`
	Size    uint64          `json:"size"`
	Type    elf.SectionType `json:"type"`
	Flags   elf.SectionFlag `json:"flags"`
}

type historicalDefinition struct {
	Name    string `json:"name"`
	Address uint64 `json:"address"`
	Section string `json:"section"`
	Size    uint64 `json:"size"`
}

// Fixed-address externs are absolute linker aliases, not source-owned storage.
// Historical C definitions can be local, NOTYPE and zero-sized, so their
// regular allocated section and address provide the useful evidence here.
func historicalDefinitions(linked *elf.File) ([]historicalDefinition, error) {
	definitions := []historicalDefinition{}
	symbols, err := linked.Symbols()
	if errors.Is(err, elf.ErrNoSymbols) {
		return definitions, nil
	}
	if err != nil {
		return nil, err
	}
	for _, symbol := range symbols {
		if symbol.Section == elf.SHN_COMMON {
			return nil, fmt.Errorf("unsupported ELF COMMON symbol %s", symbol.Name)
		}
		if symbol.Name == "" || symbol.Section == elf.SHN_UNDEF || symbol.Section == elf.SHN_ABS || elf.ST_TYPE(symbol.Info) == elf.STT_SECTION {
			continue
		}
		if int(symbol.Section) >= len(linked.Sections) {
			return nil, fmt.Errorf("definition %s has invalid ELF section %d", symbol.Name, symbol.Section)
		}
		section := linked.Sections[symbol.Section]
		if section.Flags&elf.SHF_ALLOC != 0 {
			definitions = append(definitions, historicalDefinition{Name: symbol.Name, Address: symbol.Value, Section: section.Name, Size: symbol.Size})
		}
	}
	return definitions, nil
}

// Both the input object and linked result must prove absence of COMMON; the
// linker may otherwise silently materialize an unclaimed COMMON allocation.
func rejectHistoricalCommon(path string) error {
	object, err := elf.Open(path)
	if err != nil {
		return err
	}
	defer object.Close()
	_, err = historicalDefinitions(object)
	return err
}

// Version-four cache entries retain the real ELF allocation evidence rather
// than treating a nil payload as a zero-length NOBITS allocation. Invalid or
// older entries are misses, never accepted without recompilation.
func (result *historicalResult) validateMetadata() error {
	if !result.NoCommon || result.Definitions == nil || result.Allocations == nil || result.Sections == nil {
		return errors.New("incomplete historical allocation metadata")
	}
	if len(result.Allocations) != len(result.Sections) {
		return errors.New("historical allocation/payload key mismatch")
	}
	for name, data := range result.Sections {
		allocation, ok := result.Allocations[name]
		if !ok || allocation.Flags&elf.SHF_ALLOC == 0 || allocation.Address > 0xffffffff || allocation.Size > 0x100000000-allocation.Address {
			return fmt.Errorf("invalid historical allocation metadata for %s", name)
		}
		if allocation.Type != elf.SHT_PROGBITS && allocation.Type != elf.SHT_NOBITS {
			return fmt.Errorf("unsupported allocation type for %s", name)
		}
		if allocation.Type == elf.SHT_NOBITS {
			if data != nil || name == ".text" {
				return fmt.Errorf("invalid NOBITS payload for %s", name)
			}
		} else if allocation.Size != uint64(len(data)) {
			return fmt.Errorf("historical allocation/payload size mismatch for %s", name)
		}
	}
	for _, definition := range result.Definitions {
		allocation, ok := result.Allocations[definition.Section]
		if !ok || definition.Address < allocation.Address || definition.Address > allocation.Address+allocation.Size || definition.Size > allocation.Address+allocation.Size-definition.Address {
			return fmt.Errorf("definition %s lies outside its allocated section", definition.Name)
		}
	}
	return nil
}

// Only named definitions that actually exist are checked. Other target data
// rows can be aliases, unrecovered original objects or shared runtime views.
// Byte ranges remain governed by the owner's existing section contracts.
func (result *historicalResult) checkedSections(module *moduleSpec, owner string) (map[string][]byte, error) {
	if err := result.validateMetadata(); err != nil {
		return nil, err
	}
	expected := make(map[string]uint32, len(module.Data))
	for _, symbol := range module.Data {
		expected[symbol.Name] = symbol.Addr
	}
	for _, definition := range result.Definitions {
		if definition.Section == ".text" {
			continue
		}
		if address, ok := expected[definition.Name]; ok && definition.Address != uint64(address) {
			return nil, fmt.Errorf("%s: %s defines %s at 0x%08x, but target binds it at 0x%08x",
				owner, definition.Section, definition.Name, definition.Address, address)
		}
	}
	return result.Sections, nil
}

func (result *historicalResult) checkedUnitSections(unit *unitInstance) (map[string][]byte, error) {
	sections, err := result.checkedSections(unit.module, unit.spec.name)
	if err != nil {
		return nil, err
	}
	if len(unit.bss) == 0 {
		return sections, nil
	}
	base, size, err := bssEnvelope(unit.module, unit.bss)
	if err != nil {
		return nil, err
	}
	allocation, ok := result.Allocations[".bss"]
	if !ok || allocation.Address != uint64(base) || allocation.Size != uint64(size) || allocation.Type != elf.SHT_NOBITS || allocation.Flags != elf.SHF_ALLOC|elf.SHF_WRITE {
		return nil, fmt.Errorf("%s: .bss must be an exact writable NOBITS allocation at 0x%08x+%d", unit.spec.name, base, size)
	}
	claimed := make([]bool, len(unit.bss))
	for _, definition := range result.Definitions {
		if definition.Section != ".bss" {
			continue
		}
		bound := false
		for _, symbol := range unit.module.Data {
			if symbol.Name == definition.Name && uint64(symbol.Addr) == definition.Address {
				bound = true
				break
			}
		}
		if !bound {
			return nil, fmt.Errorf("%s: unbound BSS definition %s", unit.spec.name, definition.Name)
		}
		inside := false
		for i, span := range unit.bss {
			start, end := uint64(span.Addr), uint64(span.Addr)+uint64(span.Size)
			if definition.Address >= start && definition.Address < end && definition.Size <= end-definition.Address {
				inside = true
				if definition.Address == start {
					claimed[i] = true
				}
			}
		}
		if !inside {
			return nil, fmt.Errorf("%s: BSS definition %s is outside claimed spans", unit.spec.name, definition.Name)
		}
	}
	for i, found := range claimed {
		if !found {
			return nil, fmt.Errorf("%s: .bss at 0x%08x lacks a real named definition", unit.spec.name, unit.bss[i].Addr)
		}
	}
	// Only this validated NOBITS allocation becomes deterministic output zeros;
	// undeclared or unexpected allocations remain visible to section rejection.
	normalized := make(map[string][]byte, len(sections))
	for name, data := range sections {
		normalized[name] = data
	}
	normalized[".bss"] = make([]byte, size)
	return normalized, nil
}
