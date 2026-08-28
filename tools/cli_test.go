package main

import (
	"strings"
	"testing"
)

func TestCommandDispatchRejectsUnknownSelections(t *testing.T) {
	p := validTestProject(t)
	tests := []struct {
		name string
		run  func() error
		want string
	}{
		{name: "build", run: func() error { return p.buildCommand([]string{"unknown"}) }, want: "unknown module"},
		{name: "validate", run: func() error { return p.validateCommand([]string{"--module=unknown"}) }, want: "unknown module"},
		{name: "diff", run: func() error { return p.diffCommand([]string{"unknown"}) }, want: "no function"},
		{name: "permute", run: func() error { return p.permuteCommand([]string{"--module=unknown", "battle_helper"}) }, want: "unknown module"},
		{name: "symbols", run: func() error { return p.symbolsCommand([]string{"sort"}) }, want: "unknown symbols action"},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			err := test.run()
			if err == nil || !strings.Contains(err.Error(), test.want) {
				t.Fatalf("error = %v, want text %q", err, test.want)
			}
		})
	}
}

func TestResolveModuleSelection(t *testing.T) {
	p := validTestProject(t)
	config, err := p.loadProjectConfig()
	if err != nil {
		t.Fatal(err)
	}
	for _, test := range []struct {
		selection string
		want      []string
	}{
		{"battle", []string{"battle"}},
		{"event", []string{"event-attack", "event-card"}},
		{"main, attack battle attack", []string{"battle", "event-attack", "main"}},
	} {
		modules, err := resolveModuleSelection(config, test.selection)
		if err != nil {
			t.Fatalf("%q: %v", test.selection, err)
		}
		var got []string
		for _, m := range modules {
			got = append(got, m.ID)
		}
		if strings.Join(got, " ") != strings.Join(test.want, " ") {
			t.Errorf("%q = %v, want %v", test.selection, got, test.want)
		}
	}
	for _, selection := range []string{"", " , ", "nowhere"} {
		if _, err := resolveModuleSelection(config, selection); err == nil {
			t.Errorf("%q: expected an error", selection)
		}
	}
}
