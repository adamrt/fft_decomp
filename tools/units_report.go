// units_report.go renders translation-unit candidates as YAML (one document
// per module) and a Markdown summary.
package main

import (
	"bytes"
	"fmt"
	"sort"
	"strings"

	"gopkg.in/yaml.v3"
)

const unitsSchemaVersion = 1

type unitsReport struct {
	SchemaVersion int                   `yaml:"schema_version"`
	CandidateOnly bool                  `yaml:"candidate_only"`
	Module        string                `yaml:"module"`
	File          string                `yaml:"file"`
	Options       unitsReportOptions    `yaml:"options"`
	Totals        unitsReportTotals     `yaml:"totals"`
	Alignments    []unitsReportAlign    `yaml:"alignments"`
	Groups        []unitsReportGroup    `yaml:"groups"`
	Diagnostics   unitsReportDiagnostic `yaml:"diagnostics"`
}

type unitsReportOptions struct {
	Evidence      string `yaml:"evidence,omitempty"`
	MinConfidence string `yaml:"min_confidence"`
	MaxOwners     int    `yaml:"max_owners"`
}

type unitsReportTotals struct {
	Groups       int            `yaml:"groups"`
	Functions    int            `yaml:"functions"`
	DataRows     int            `yaml:"data_rows"`
	Units        int            `yaml:"units"`
	ByConfidence map[string]int `yaml:"by_confidence"`
}

type unitsReportAlign struct {
	Partner     string                  `yaml:"partner"`
	Matched     int                     `yaml:"matched"`
	Runs        int                     `yaml:"runs"`
	Keys        map[string]int          `yaml:"keys"`
	DataMatched int                     `yaml:"data_matched"`
	DataShifts  int                     `yaml:"data_shifts"`
	Divergences []unitsReportDivergence `yaml:"divergences"`
}

type unitsReportDivergence struct {
	Kind      string   `yaml:"kind"`
	Module    string   `yaml:"module"`
	Start     string   `yaml:"start"`
	End       string   `yaml:"end"`
	Functions []string `yaml:"functions"`
	Partner   []string `yaml:"partner_functions,omitempty"`
}

type unitsReportGroup struct {
	ID         int                   `yaml:"id"`
	Text       unitsReportSpan       `yaml:"text"`
	Rodata     *unitsReportSpan      `yaml:"rodata,omitempty"`
	Data       unitsReportSpan       `yaml:"data"`
	Sections   []unitsReportSection  `yaml:"sections"`
	Boundaries []unitsReportBoundary `yaml:"boundaries"`
	Units      []unitsReportUnit     `yaml:"units"`
}

type unitsReportSpan struct {
	Start     string `yaml:"start"`
	End       string `yaml:"end"`
	Functions int    `yaml:"functions,omitempty"`
	Rows      int    `yaml:"rows,omitempty"`
}

type unitsReportSection struct {
	ID       string `yaml:"id"`
	Start    string `yaml:"start"`
	End      string `yaml:"end"`
	Kind     string `yaml:"kind"`
	Rows     int    `yaml:"rows"`
	Monotone bool   `yaml:"monotone"`
	Resets   int    `yaml:"resets"`
}

type unitsReportBoundary struct {
	After      string                `yaml:"after"`
	Before     string                `yaml:"before"`
	Addr       string                `yaml:"addr"`
	Score      int                   `yaml:"score"`
	Confidence string                `yaml:"confidence"`
	Evidence   []unitsReportEvidence `yaml:"evidence"`
}

type unitsReportEvidence struct {
	Kind   string `yaml:"kind"`
	Weight int    `yaml:"weight"`
	Detail string `yaml:"detail"`
}

type unitsReportUnit struct {
	ID            string                   `yaml:"id"`
	Start         string                   `yaml:"start"`
	End           string                   `yaml:"end"`
	Size          int                      `yaml:"size"`
	Confidence    string                   `yaml:"confidence"`
	Profile       string                   `yaml:"profile"`
	Prefixes      map[string]int           `yaml:"prefixes"`
	SuggestedName string                   `yaml:"suggested_name"`
	Counterparts  []unitsReportCounterpart `yaml:"counterparts"`
	Functions     []unitsReportFunction    `yaml:"functions"`
	Data          []unitsReportData        `yaml:"data"`
}

type unitsReportCounterpart struct {
	Module  string `yaml:"module"`
	Unit    string `yaml:"unit"`
	Matched int    `yaml:"matched"`
}

type unitsReportFunction struct {
	Addr    string            `yaml:"addr"`
	Size    int               `yaml:"size"`
	Name    string            `yaml:"name"`
	Source  string            `yaml:"source"`
	Aligned map[string]string `yaml:"aligned,omitempty"`
}

type unitsReportData struct {
	Addr    string   `yaml:"addr"`
	Names   []string `yaml:"names"`
	Section string   `yaml:"section"`
	Owners  []string `yaml:"owners"`
}

type unitsReportDiagnostic struct {
	UnownedData          []string                  `yaml:"unowned_data"`
	WidelyReferenced     []unitsReportWide         `yaml:"widely_referenced"`
	RodataOrderViolation []unitsReportRodataOrder  `yaml:"rodata_order_violations"`
	UnreadableSources    []string                  `yaml:"unreadable_sources"`
	Straddlers           []unitsReportStraddlerRow `yaml:"local_rows_vetoing_cuts"`
}

type unitsReportWide struct {
	Name   string `yaml:"name"`
	Owners int    `yaml:"owners"`
}

type unitsReportRodataOrder struct {
	Function string `yaml:"function"`
	Rodata   string `yaml:"rodata"`
	Previous string `yaml:"previous"`
}

type unitsReportStraddlerRow struct {
	Name  string `yaml:"name"`
	First string `yaml:"first_owner"`
	Last  string `yaml:"last_owner"`
}

func hexAddr(addr uint32) string { return fmt.Sprintf("0x%08x", addr) }

// unitsModuleReport builds the YAML document for one analyzed module.
func unitsModuleReport(an *unitModuleAnalysis, options unitsOptions) unitsReport {
	report := unitsReport{
		SchemaVersion: unitsSchemaVersion, CandidateOnly: true, Module: an.m.ID, File: an.m.File,
		Options: unitsReportOptions{Evidence: options.evidence, MinConfidence: options.minConfidence, MaxOwners: options.maxOwners},
		Totals: unitsReportTotals{Groups: len(an.groups), Functions: len(an.functions), DataRows: len(an.rows),
			ByConfidence: map[string]int{"high": 0, "medium": 0, "low": 0}},
		Alignments: []unitsReportAlign{}, Groups: []unitsReportGroup{},
	}
	for _, alignment := range an.alignments {
		report.Alignments = append(report.Alignments, alignment)
	}
	for _, group := range an.groups {
		g := unitsReportGroup{ID: group.id,
			Text:     unitsReportSpan{Start: hexAddr(group.textStart), End: hexAddr(group.textEnd), Functions: group.last - group.first + 1},
			Data:     unitsReportSpan{Start: hexAddr(group.dataStart), End: hexAddr(group.dataEnd), Rows: len(group.rows)},
			Sections: []unitsReportSection{}, Boundaries: []unitsReportBoundary{}, Units: []unitsReportUnit{}}
		if group.rodataEnd > group.rodataStart {
			g.Rodata = &unitsReportSpan{Start: hexAddr(group.rodataStart), End: hexAddr(group.rodataEnd)}
		}
		for _, section := range group.sections {
			g.Sections = append(g.Sections, unitsReportSection{ID: section.id, Start: hexAddr(section.start), End: hexAddr(section.end),
				Kind: section.kind, Rows: len(section.rows), Monotone: section.monotone, Resets: section.resets})
		}
		for _, cut := range group.cuts {
			if cut.confidence == "none" && !cut.certain {
				continue
			}
			boundary := unitsReportBoundary{After: an.functions[cut.after].f.Name, Before: an.functions[cut.after+1].f.Name,
				Addr: hexAddr(an.functions[cut.after+1].f.Addr), Score: cut.score, Confidence: cut.confidence, Evidence: []unitsReportEvidence{}}
			for _, evidence := range cut.evidence {
				boundary.Evidence = append(boundary.Evidence, unitsReportEvidence(evidence))
			}
			g.Boundaries = append(g.Boundaries, boundary)
		}
		for _, unit := range group.units {
			report.Totals.Units++
			report.Totals.ByConfidence[unit.confidence]++
			u := unitsReportUnit{ID: unit.id, Start: hexAddr(unit.start), End: hexAddr(unit.end), Size: int(unit.end - unit.start),
				Confidence: unit.confidence, Profile: unit.profile, Prefixes: unit.prefixes, SuggestedName: unit.suggestedName,
				Counterparts: []unitsReportCounterpart{}, Functions: []unitsReportFunction{}, Data: []unitsReportData{}}
			for _, counterpart := range unit.counterparts {
				u.Counterparts = append(u.Counterparts, counterpart)
			}
			for _, index := range unit.functions {
				fn := an.functions[index]
				entry := unitsReportFunction{Addr: hexAddr(fn.f.Addr), Size: fn.f.Size, Name: fn.f.Name, Source: fn.source}
				if len(fn.aligned) > 0 {
					entry.Aligned = map[string]string{}
					for partner, name := range fn.aligned {
						if fn.positional[partner] {
							name += " (positional)"
						}
						entry.Aligned[partner] = name
					}
				}
				u.Functions = append(u.Functions, entry)
			}
			for _, rowIndex := range unit.rows {
				row := an.rows[rowIndex]
				data := unitsReportData{Addr: hexAddr(row.addr), Names: row.names, Section: group.sectionID(row.section), Owners: []string{}}
				for _, owner := range row.owners {
					data.Owners = append(data.Owners, an.functions[owner].f.Name)
				}
				u.Data = append(u.Data, data)
			}
			g.Units = append(g.Units, u)
		}
		report.Groups = append(report.Groups, g)
	}
	report.Diagnostics = an.diagnostics
	if report.Diagnostics.UnownedData == nil {
		report.Diagnostics.UnownedData = []string{}
	}
	if report.Diagnostics.WidelyReferenced == nil {
		report.Diagnostics.WidelyReferenced = []unitsReportWide{}
	}
	if report.Diagnostics.RodataOrderViolation == nil {
		report.Diagnostics.RodataOrderViolation = []unitsReportRodataOrder{}
	}
	if report.Diagnostics.UnreadableSources == nil {
		report.Diagnostics.UnreadableSources = []string{}
	}
	if report.Diagnostics.Straddlers == nil {
		report.Diagnostics.Straddlers = []unitsReportStraddlerRow{}
	}
	return report
}

func renderUnitsYAML(report unitsReport) ([]byte, error) {
	var out bytes.Buffer
	out.WriteString("# Translation-unit candidates: inferred from configured layout and current source references.\n")
	encoder := yaml.NewEncoder(&out)
	encoder.SetIndent(2)
	if err := encoder.Encode(report); err != nil {
		return nil, err
	}
	if err := encoder.Close(); err != nil {
		return nil, err
	}
	return out.Bytes(), nil
}

// renderUnitsMarkdown summarizes the selected modules for reading.
func renderUnitsMarkdown(reports []unitsReport) []byte {
	var out strings.Builder
	out.WriteString("# Translation-unit candidates\n\n")
	out.WriteString("Candidates only. The analysis models units as contiguous function runs and data ranges.\n")
	out.WriteString("It uses configured layout and compiler profiles, cross-module alignment by current\n")
	out.WriteString("source paths, hashes and names, and references in the current function bodies. Unmatched\n")
	out.WriteString("runs and data-distance changes can also come from source edits, conditional membership,\n")
	out.WriteString("resized tables or alignment. They do not prove original source-file boundaries.\n\n")
	out.WriteString("Confidence describes the weaker candidate edge, with group edges rated high. Required\n")
	out.WriteString("cuts (`certain` internally) follow configured library/assembly transitions, profile\n")
	out.WriteString("changes or jump-table alignment constraints; strong mode also takes alignment and\n")
	out.WriteString("data-distance signals. An unsplit span is not proof that its functions shared one file.\n\n")
	out.WriteString("Data section labels are estimates from address ranges and spacing; nearby references\n")
	out.WriteString("do not establish static linkage. Function-reference counts include callbacks. Name\n")
	out.WriteString("prefixes describe candidates but do not affect boundary scores. Review each candidate\n")
	out.WriteString("against its original-byte contracts before reorganizing source.\n\n")
	for _, report := range reports {
		fmt.Fprintf(&out, "## %s (%s)\n\n", report.Module, report.File)
		fmt.Fprintf(&out, "%d functions, %d data rows, %d groups, %d units (high %d, medium %d, low %d).\n\n",
			report.Totals.Functions, report.Totals.DataRows, report.Totals.Groups, report.Totals.Units,
			report.Totals.ByConfidence["high"], report.Totals.ByConfidence["medium"], report.Totals.ByConfidence["low"])
		if len(report.Alignments) > 0 {
			out.WriteString("| Partner | Matched functions | Runs | Divergences | Matched data | Data shifts |\n|---|---|---|---|---|---|\n")
			for _, alignment := range report.Alignments {
				fmt.Fprintf(&out, "| %s | %d | %d | %d | %d | %d |\n", alignment.Partner, alignment.Matched, alignment.Runs,
					len(alignment.Divergences), alignment.DataMatched, alignment.DataShifts)
			}
			out.WriteString("\n")
		}
		for _, group := range report.Groups {
			fmt.Fprintf(&out, "### Group %d: text %s to %s (%d functions), data %s to %s (%d rows)\n\n",
				group.ID, group.Text.Start, group.Text.End, group.Text.Functions, group.Data.Start, group.Data.End, group.Data.Rows)
			if len(group.Sections) > 0 {
				out.WriteString("Sections: ")
				var parts []string
				for _, section := range group.Sections {
					monotone := ""
					if section.Monotone {
						monotone = ", monotone"
					}
					parts = append(parts, fmt.Sprintf("%s %s-%s (%s, %d rows%s)", section.ID, section.Start, section.End, section.Kind, section.Rows, monotone))
				}
				out.WriteString(strings.Join(parts, "; "))
				out.WriteString("\n\n")
			}
			out.WriteString("| Unit | Range | Functions | Confidence | Suggested name | Counterparts |\n|---|---|---|---|---|---|\n")
			for _, unit := range group.Units {
				var counterparts []string
				for _, counterpart := range unit.Counterparts {
					counterparts = append(counterparts, fmt.Sprintf("%s (%d)", counterpart.Unit, counterpart.Matched))
				}
				fmt.Fprintf(&out, "| %s | %s-%s | %d | %s | %s | %s |\n", unit.ID, unit.Start, unit.End, len(unit.Functions),
					unit.Confidence, unit.SuggestedName, strings.Join(counterparts, ", "))
			}
			out.WriteString("\n")
		}
	}
	return []byte(out.String())
}

// sortedPrefixes lists prefix counts in a stable order for Markdown.
func sortedPrefixes(prefixes map[string]int) []string {
	var names []string
	for name := range prefixes {
		names = append(names, name)
	}
	sort.Slice(names, func(i, j int) bool {
		if prefixes[names[i]] != prefixes[names[j]] {
			return prefixes[names[i]] > prefixes[names[j]]
		}
		return names[i] < names[j]
	})
	return names
}
