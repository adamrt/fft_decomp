package main

import "fmt"

// compileUnit packs a member's declared literal ranges into one blob in
// declaration order, omitting section gaps and other members' literals.
// Standalone functions have one range and use the same representation.
func splitFunctionRodata(f *functionSpec, compiled []byte) ([][]byte, error) {
	var spans [][]byte
	remaining := compiled
	for _, span := range f.Rodata {
		if span.Size <= 0 {
			return nil, fmt.Errorf(".rodata 0x%08x has invalid declared size %d", span.Addr, span.Size)
		}
		if span.Size > len(remaining) {
			return nil, fmt.Errorf(".rodata 0x%08x requires %d bytes, only %d compiled bytes remain", span.Addr, span.Size, len(remaining))
		}
		spans = append(spans, remaining[:span.Size])
		remaining = remaining[span.Size:]
	}
	if len(remaining) != 0 {
		return nil, fmt.Errorf(".rodata has %d undeclared trailing bytes", len(remaining))
	}
	return spans, nil
}
