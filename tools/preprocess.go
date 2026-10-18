package main

import "path/filepath"

// Share preprocessing options between matching builds, declaration checks,
// ownership analysis and permuter input. Generated includes belong to the
// checkout even when source/header inputs use a container-local mirror.
func (p project) cppFlags() []string {
	return []string{"-P", "-undef", "-nostdinc", "-Iinclude", "-I" + filepath.Join(p.root, "build", "generated")}
}
