package main

import (
	"debug/elf"
	"errors"
	"fmt"
)

// Keep definition evidence separate from emitted bytes. Every consumer of
// Sections still sees only allocatable sections, including unexpected ones.
type historicalResult struct {
	Sections    map[string][]byte      `json:"sections"`
	Definitions []historicalDefinition `json:"definitions"`
}

type historicalDefinition struct {
	Name    string `json:"name"`
	Address uint64 `json:"address"`
	Section string `json:"section"`
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
		if symbol.Name == "" || symbol.Section == elf.SHN_UNDEF || symbol.Section == elf.SHN_ABS || symbol.Section == elf.SHN_COMMON {
			continue
		}
		if int(symbol.Section) >= len(linked.Sections) {
			return nil, fmt.Errorf("definition %s has invalid ELF section %d", symbol.Name, symbol.Section)
		}
		section := linked.Sections[symbol.Section]
		if section.Flags&elf.SHF_ALLOC != 0 {
			definitions = append(definitions, historicalDefinition{symbol.Name, symbol.Value, section.Name})
		}
	}
	return definitions, nil
}

// Only named definitions that actually exist are checked. Other target data
// rows can be aliases, unrecovered original objects or shared runtime views.
// Byte ranges remain governed by the owner's existing section contracts.
func (result *historicalResult) checkedSections(module *moduleSpec, owner string) (map[string][]byte, error) {
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
