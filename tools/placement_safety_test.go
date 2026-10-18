package main

import (
	"strings"
	"testing"
)

func placementFamilyFixture(modules ...*moduleSpec) (*projectConfig, map[string]*unitModuleAnalysis, map[string]*placementReport) {
	analyses := map[string]*unitModuleAnalysis{}
	reports := map[string]*placementReport{}
	for _, m := range modules {
		an := &unitModuleAnalysis{m: m}
		for i := range m.Functions {
			an.functions = append(an.functions, unitFunction{f: &m.Functions[i], source: m.Functions[i].Source, idents: map[string]struct{}{}})
		}
		analyses[m.ID] = an
		reports[m.ID] = &placementReport{}
	}
	return &projectConfig{Modules: modules}, analyses, reports
}

func TestPlacementTransitivePositionDoesNotConflateCoexistingFunctions(t *testing.T) {
	bunit := &moduleSpec{ID: "event-bunit", Functions: []functionSpec{
		{Name: "job_calculate_and_store_unlocks", Size: 72, Source: "src/event/bunit/job_abilities.c"},
		{Name: "unit_list_build_job_list", Size: 352, Source: "src/event/bunit/job_abilities.c"},
	}}
	jobstts := &moduleSpec{ID: "event-jobstts", Functions: []functionSpec{
		{Name: "job_find_first_for_skillset", Size: 88, Source: "src/event/jobstts/job_abilities.c"},
		{Name: "job_list_build", Size: 352, Source: "src/event/jobstts/job_abilities.c"},
	}}
	world := &moduleSpec{ID: "world", Functions: []functionSpec{
		{Name: "job_calculate_and_store_unlocks", Size: 72, Source: "src/lib/menu/job_calculate_and_store_unlocks.c"},
		{Name: "job_list_build", Size: 496, Source: "src/world/job_list_build.c"},
	}}
	config, analyses, reports := placementFamilyFixture(bunit, jobstts, world)
	analyses[bunit.ID].functions[0].aligned = map[string]string{jobstts.ID: "job_find_first_for_skillset", world.ID: "job_calculate_and_store_unlocks"}
	analyses[bunit.ID].functions[0].positional = map[string]bool{jobstts.ID: true}
	analyses[bunit.ID].functions[1].aligned = map[string]string{world.ID: "job_list_build"}
	analyses[jobstts.ID].functions[0].aligned = map[string]string{world.ID: "job_list_build"}
	analyses[jobstts.ID].functions[0].positional = map[string]bool{world.ID: true}
	reports[bunit.ID].Moves = []placementRow{{Module: bunit.ID, Function: bunit.Functions[0].Name, NewName: bunit.Functions[0].Name, Subsystem: "lib/menu", Apply: true}}
	reports[world.ID].Review = []placementRow{{Module: world.ID, Function: world.Functions[1].Name, NewName: world.Functions[1].Name, Subsystem: "lib/menu", Apply: true}}
	twins := map[string]string{bunit.ID + ":" + bunit.Functions[0].Name: "calculator", world.ID + ":" + world.Functions[0].Name: "calculator"}
	families := placementFamilies(config, analyses, reports, twins)
	if len(families) != 1 || len(families[0].Members) != 6 {
		t.Fatalf("retain the full uncertain family for inspection: %+v", families)
	}
	family := families[0]
	if family.Apply || !strings.Contains(family.Note, "coexisting") {
		t.Errorf("uncertain coexistence remains actionable: %+v", family)
	}
	seen := map[string]map[string]bool{}
	for _, member := range family.Members {
		if seen[member.Module] == nil {
			seen[member.Module] = map[string]bool{}
		}
		if seen[member.Module][member.Body] {
			t.Errorf("distinct functions in %s share body %s", member.Module, member.Body)
		}
		seen[member.Module][member.Body] = true
	}
	for _, report := range reports {
		for _, row := range append(report.Moves, report.Review...) {
			if row.Apply || row.Family != family.ID || !strings.Contains(row.Note, "coexisting") {
				t.Errorf("family veto did not reach row: %+v", row)
			}
		}
	}
}

func TestPlacementSameFileDoesNotProveSameActiveFunctionBody(t *testing.T) {
	world := &moduleSpec{ID: "world", Functions: []functionSpec{{Name: "menu_draw", Size: 32, Source: "src/lib/menu/menu_draw.c"}}}
	battle := &moduleSpec{ID: "battle", Functions: []functionSpec{{Name: "menu_draw", Size: 64, Source: "src/lib/menu/menu_draw.c"}}}
	config, analyses, reports := placementFamilyFixture(world, battle)
	reports[world.ID].Review = []placementRow{{Module: world.ID, Function: "menu_draw", NewName: "menu_draw", Subsystem: "lib/menu", Apply: true}}
	families := placementFamilies(config, analyses, reports, nil)
	if len(families) != 1 || len(families[0].Members) != 2 || families[0].Members[0].Body == families[0].Members[1].Body {
		t.Fatalf("different active program views collapsed by filename: %+v", families)
	}
}

func TestPlacementTwinsDoNotPropagateToAnotherFunctionInSameSource(t *testing.T) {
	main := &moduleSpec{ID: "main", Functions: []functionSpec{{Name: "menu_init", Size: 32, Source: "src/lib/menu/feature.c"}}}
	world := &moduleSpec{ID: "world", Functions: []functionSpec{{Name: "menu_update", Size: 64, Source: "src/lib/menu/feature.c"}}}
	battle := &moduleSpec{ID: "battle", Functions: []functionSpec{{Name: "menu_init", Size: 32, Source: "src/battle/feature.c"}}}
	config, analyses, reports := placementFamilyFixture(main, world, battle)
	analyses[main.ID].functions[0].aligned = map[string]string{world.ID: "menu_update"}
	analyses[main.ID].functions[0].positional = map[string]bool{world.ID: true}
	reports[battle.ID].Review = []placementRow{{Module: battle.ID, Function: "menu_init", NewName: "menu_init", Subsystem: "lib/menu", Apply: true}}
	twins := map[string]string{"main:menu_init": "init", "battle:menu_init": "init"}
	families := placementFamilies(config, analyses, reports, twins)
	if len(families) != 1 {
		t.Fatalf("families = %+v", families)
	}
	bodies := map[string]string{}
	for _, member := range families[0].Members {
		bodies[member.Module] = member.Body
	}
	if bodies[main.ID] != bodies[battle.ID] || bodies[world.ID] == bodies[main.ID] {
		t.Fatalf("twins membership leaked through a shared filename: %v", bodies)
	}
}

func TestPlacementCoexistingDuplicateEvidence(t *testing.T) {
	for _, test := range []struct {
		name         string
		symbol       string
		hash         string
		profile      string
		missingFacts string
		sharedSource bool
		wantApply    bool
	}{
		{name: "equal original bytes", hash: "0123456789abcdef", wantApply: true},
		{name: "equal original bytes with different profiles", hash: "0123456789abcdef", profile: "gcc-2.6.3_O1_aspsx-2.34", wantApply: true},
		{name: "same C definition", symbol: "menu_init", wantApply: true},
		{name: "missing both active fact maps", symbol: "menu_init", missingFacts: "both"},
		{name: "missing first active fact map", symbol: "menu_init", missingFacts: "first"},
		{name: "missing second active fact map", symbol: "menu_init", missingFacts: "second"},
		{name: "equal bytes without active facts", hash: "0123456789abcdef", missingFacts: "both", wantApply: true},
		{name: "symbolic twins only", wantApply: false},
		{name: "symbolic twins in one source", sharedSource: true, wantApply: false},
	} {
		t.Run(test.name, func(t *testing.T) {
			main := &moduleSpec{ID: "main", Profile: "gcc-2.6.3_O2_aspsx-2.34", Functions: []functionSpec{
				{Name: "menu_init", Size: 32, Hash: test.hash, Source: "src/main/menu.c"},
				{Name: "menu_init_copy", Size: 32, SourceSymbol: test.symbol, Hash: test.hash, Source: "src/main/menu.c", Profile: test.profile},
			}}
			world := &moduleSpec{ID: "world", Functions: []functionSpec{{Name: "menu_init", Size: 32, Source: "src/world/menu.c"}}}
			if test.sharedSource {
				world.Functions[0].Source = main.Functions[0].Source
			}
			config, analyses, reports := placementFamilyFixture(main, world)
			if test.missingFacts == "both" || test.missingFacts == "first" {
				analyses[main.ID].functions[0].idents = nil
			}
			if test.missingFacts == "both" || test.missingFacts == "second" {
				analyses[main.ID].functions[1].idents = nil
			}
			for i := range analyses[main.ID].functions {
				analyses[main.ID].functions[i].aligned = map[string]string{world.ID: "menu_init"}
			}
			reports[world.ID].Moves = []placementRow{{Module: world.ID, Function: "menu_init", NewName: "menu_init", Subsystem: "lib/menu", Apply: true}}
			twins := map[string]string{"main:menu_init": "init", "main:menu_init_copy": "init", "world:menu_init": "init"}
			families := placementFamilies(config, analyses, reports, twins)
			if len(families) != 1 || families[0].Apply != test.wantApply {
				t.Fatalf("apply must follow direct duplicate evidence, not coexistence alone: %+v", families)
			}
			if reports[world.ID].Moves[0].Apply != test.wantApply {
				t.Fatalf("row applicability disagrees: %+v", reports[world.ID].Moves[0])
			}
		})
	}
}

func TestPlacementSameDefinitionRequiresMatchingBindingsAndProfile(t *testing.T) {
	const symbol = "menu_init_scaled_draw_area_packets"
	const alias = "option_init_scaled_draw_area_packets"
	for _, test := range []struct {
		name          string
		firstOverride uint32
		aliasOverride uint32
		aliasProfile  string
		unitProfile   string
		wantApply     bool
	}{
		{name: "different template bindings", firstOverride: 0x801531b0, aliasOverride: 0x80156c8c},
		{name: "one different template override", aliasOverride: 0x80156c8c},
		{name: "equal template overrides", firstOverride: 0x80156c8c, aliasOverride: 0x80156c8c, wantApply: true},
		{name: "override matches default binding", aliasOverride: 0x801531b0, wantApply: true},
		{name: "different function profiles", aliasProfile: "gcc-2.6.3_O1_aspsx-2.34"},
		{name: "different unit profiles", unitProfile: "gcc-2.6.3_O1_aspsx-2.34"},
		{name: "equal effective profiles", unitProfile: "gcc-2.6.3_O2_aspsx-2.34", wantApply: true},
	} {
		t.Run(test.name, func(t *testing.T) {
			world := &moduleSpec{ID: "world", Profile: "gcc-2.6.3_O2_aspsx-2.34", Functions: []functionSpec{
				{Name: symbol, Addr: 0x800ecb24, Size: 52, Hash: "7d7062d484ec19c4", Source: "src/lib/menu/menu_init_scaled_draw_area_packets.c"},
				{Name: alias, SourceSymbol: symbol, Addr: 0x8010170c, Size: 52, Hash: "babbe0634130c090", Source: "src/lib/menu/menu_init_scaled_draw_area_packets.c", Profile: test.aliasProfile},
			}, Data: []symbolSpec{{Name: "g_menu_draw_area_template", Addr: 0x801531b0}}}
			if test.firstOverride != 0 {
				world.Overrides = append(world.Overrides, overrideSpec{Function: symbol, Name: "g_menu_draw_area_template", Addr: test.firstOverride})
			}
			if test.aliasOverride != 0 {
				world.Overrides = append(world.Overrides, overrideSpec{Function: alias, Name: "g_menu_draw_area_template", Addr: test.aliasOverride})
			}
			if test.unitProfile != "" {
				world.Functions[1].Unit = "packets"
				world.Units = []unitDef{{Name: "packets", Profile: test.unitProfile}}
			}
			battle := &moduleSpec{ID: "battle", Functions: []functionSpec{{Name: symbol, Size: 52, Source: "src/battle/packets.c"}}}
			config, analyses, reports := placementFamilyFixture(world, battle)
			for i := range analyses[world.ID].functions {
				analyses[world.ID].functions[i].idents = map[string]struct{}{symbol: {}, "g_menu_draw_area_template": {}}
			}
			reports[battle.ID].Moves = []placementRow{{Module: battle.ID, Function: symbol, NewName: symbol, Subsystem: "lib/menu", Apply: true}}
			twins := map[string]string{"world:" + symbol: "init", "world:" + alias: "init", "battle:" + symbol: "init"}
			families := placementFamilies(config, analyses, reports, twins)
			if len(families) != 1 || families[0].Apply != test.wantApply {
				t.Fatalf("definition exception ignored bound behavior/profile: %+v", families)
			}
			if reports[battle.ID].Moves[0].Apply != test.wantApply {
				t.Fatalf("row applicability disagrees: %+v", reports[battle.ID].Moves[0])
			}
		})
	}
}
