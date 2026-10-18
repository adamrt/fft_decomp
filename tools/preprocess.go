package main

// Share preprocessing options between matching builds, declaration checks,
// ownership analysis and permuter input.
func (p project) cppFlags() []string {
	return []string{"-P", "-undef", "-nostdinc", "-Iinclude"}
}
