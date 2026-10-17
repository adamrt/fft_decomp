package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/json"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
)

func shareAuditFixture(t *testing.T, left, right, header string) (project, *projectConfig) {
	t.Helper()
	p := testProject(t, map[string]string{
		"src/a/left.c": left, "src/b/right.c": right, "include/contracts.h": header,
	})
	main := &moduleSpec{ID: "main", SourceDir: "src/a", Profile: "gcc-2.6.3_O2_aspsx-2.34",
		Functions: []functionSpec{{Name: "left", Addr: 0x80010000, Size: 128, Hash: "1111111111111111"}},
		Data:      []symbolSpec{{Name: "left_callee", Addr: 0x80010100}, {Name: "left_state", Addr: 0x80010200}}}
	world := &moduleSpec{ID: "world", SourceDir: "src/b", Profile: "gcc-2.6.3_O2_aspsx-2.34",
		Functions: []functionSpec{{Name: "right", Addr: 0x80020000, Size: 132, Hash: "2222222222222222", Profile: "gcc-2.6.3_O1_aspsx-2.34"}},
		Data:      []symbolSpec{{Name: "right_callee", Addr: 0x80020100}, {Name: "right_state", Addr: 0x80020200}}}
	return p, &projectConfig{Modules: []*moduleSpec{world, main}, byID: map[string]*moduleSpec{"main": main, "world": world}}
}

const shareAuditHeader = `int left_callee(int n);
int right_callee(int n);
extern struct item left_state;
extern struct item right_state;
`
const shareAuditLeft = `int left(int n) { return left_callee(n) + left_state.value; }`
const shareAuditRight = `int right(int n) { return right_callee(n) + right_state.value; }`

func TestShareAuditGroupsExactRolesDeterministically(t *testing.T) {
	p, config := shareAuditFixture(t, shareAuditLeft, shareAuditRight, shareAuditHeader)
	first, err := p.shareAudit(config, shareAuditOptions{minBytes: 64})
	if err != nil {
		t.Fatal(err)
	}
	if !first.CandidateOnly || len(first.Groups) != 1 {
		t.Fatalf("report = %+v", first)
	}
	group := first.Groups[0]
	if len(group.Participants) != 2 || group.Participants[0].Module != "main" {
		t.Fatalf("group = %+v", group)
	}
	left, right := group.Participants[0], group.Participants[1]
	if len(left.Roles) != 2 || left.Roles[0].Symbol != "left_callee" || left.Roles[1].Symbol != "left_state" || right.Roles[0].Symbol != "right_callee" || right.Roles[1].Symbol != "right_state" {
		t.Fatalf("roles = %+v / %+v", left.Roles, right.Roles)
	}
	if left.Address != "0x80010000" || left.Size != 128 || left.Hash != "1111111111111111" || right.Profile != "gcc-2.6.3_O1_aspsx-2.34" || left.Roles[0].Address != "0x80010100" {
		t.Fatalf("contracts lost: %+v / %+v", left, right)
	}
	config.Modules[0], config.Modules[1] = config.Modules[1], config.Modules[0]
	second, err := p.shareAudit(config, shareAuditOptions{minBytes: 64})
	if err != nil {
		t.Fatal(err)
	}
	a, _ := json.Marshal(first)
	b, _ := json.Marshal(second)
	if !bytes.Equal(a, b) {
		t.Fatal("module iteration changes JSON")
	}
}

func TestShareAuditRetainsTypeAndTokenDifferences(t *testing.T) {
	cases := []struct{ name, right, header string }{
		{"return signedness", strings.Replace(shareAuditRight, "int right", "unsigned int right", 1), shareAuditHeader},
		{"parameter signedness", strings.Replace(shareAuditRight, "int n", "unsigned int n", 1), shareAuditHeader},
		{"callee signedness", shareAuditRight, strings.Replace(shareAuditHeader, "int right_callee", "unsigned int right_callee", 1)},
		{"member", strings.Replace(shareAuditRight, ".value", ".other", 1), shareAuditHeader},
		{"operator", strings.Replace(shareAuditRight, " + ", " - ", 1), shareAuditHeader},
		{"literal", strings.Replace(shareAuditRight, "(n)", "(n + 1)", 1), shareAuditHeader},
		{"local name", strings.Replace(strings.Replace(shareAuditRight, "int n", "int index", 1), "(n)", "(index)", 1), shareAuditHeader},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			p, config := shareAuditFixture(t, shareAuditLeft, tc.right, tc.header)
			report, err := p.shareAudit(config, shareAuditOptions{})
			if err != nil {
				t.Fatal(err)
			}
			if len(report.Groups) != 0 {
				t.Fatalf("different %s was hidden: %+v", tc.name, report.Groups)
			}
		})
	}
}

func TestShareAuditRetainsLiteralValues(t *testing.T) {
	p, config := shareAuditFixture(t,
		`int left(int n) { return left_callee(n) + left_state.value + 12; }`,
		`int right(int n) { return right_callee(n) + right_state.value + 13; }`, shareAuditHeader)
	report, err := p.shareAudit(config, shareAuditOptions{})
	if err != nil {
		t.Fatal(err)
	}
	if len(report.Groups) != 0 {
		t.Fatal("different literal values were erased")
	}
}

func TestShareAuditExactBindingPrecedenceAndSourceSelf(t *testing.T) {
	p := testProject(t, map[string]string{
		"src/a/body.c":    `int body(int n) { return body(n - 1) + own + linked + second + fallback + replaced; }`,
		"include/types.h": `extern int own, linked, second, fallback, replaced;`,
	})
	main := &moduleSpec{ID: "main", Data: []symbolSpec{{Name: "own", Addr: 0x100}, {Name: "fallback", Addr: 0x110}}}
	link1 := &moduleSpec{ID: "first", Data: []symbolSpec{{Name: "linked", Addr: 0x200}, {Name: "own", Addr: 0x210}}}
	link2 := &moduleSpec{ID: "second", Data: []symbolSpec{{Name: "linked", Addr: 0x300}, {Name: "second", Addr: 0x310}}}
	m := &moduleSpec{ID: "scene", Links: []string{"first", "second"}, SourceDir: "src/a", Profile: "profile",
		Functions: []functionSpec{{Name: "public", Source: "src/a/body.c", SourceSymbol: "body", Addr: 0x80010000, Size: 64}},
		Imports:   []symbolSpec{{Name: "own", Addr: 0x400}, {Name: "body", Addr: 0xBAD}},
		Data:      []symbolSpec{{Name: "replaced", Addr: 0x410}},
		Overrides: []overrideSpec{{Function: "public", Name: "replaced", Addr: 0x420}}}
	config := &projectConfig{Modules: []*moduleSpec{m, main, link1, link2}}
	scanner, err := newShareAuditScanner(p, config)
	if err != nil {
		t.Fatal(err)
	}
	entry, _, _, reason, err := scanner.entry(m, &m.Functions[0])
	if err != nil || reason != "" {
		t.Fatalf("entry: %s %v", reason, err)
	}
	got := map[string]string{}
	for _, role := range entry.Roles {
		got[role.Symbol] = role.Address
	}
	want := map[string]string{"body": "0x80010000", "own": "0x00000400", "linked": "0x00000200", "second": "0x00000310", "fallback": "0x00000110", "replaced": "0x00000420"}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("bindings = %v, want %v", got, want)
	}
	if entry.SourceSymbol != "body" || entry.Name != "public" || entry.DefinitionSymbol != "body" || entry.Roles[0].Role != "$self" {
		t.Fatalf("self contract = %+v", entry)
	}
}

func TestShareAuditUnknownContractsAndRawReviewLimits(t *testing.T) {
	p, config := shareAuditFixture(t, shareAuditLeft, shareAuditRight, "extern struct item left_state, right_state;")
	report, err := p.shareAudit(config, shareAuditOptions{})
	if err != nil {
		t.Fatal(err)
	}
	if len(report.Groups) != 0 {
		t.Fatal("different undeclared callees grouped as compatible")
	}
	header := shareAuditHeader + "unsigned int right_callee(int n);\n"
	p, config = shareAuditFixture(t, shareAuditLeft, shareAuditRight, header)
	report, err = p.shareAudit(config, shareAuditOptions{})
	if err != nil {
		t.Fatal(err)
	}
	if len(report.Groups) != 0 {
		t.Fatal("ambiguous callee contract was assumed")
	}
	scanner, err := newShareAuditScanner(p, config)
	if err != nil {
		t.Fatal(err)
	}
	if scanner.contract("right_callee").Status != "ambiguous" {
		t.Fatal("contract disagreement not flagged")
	}
	p, config = shareAuditFixture(t,
		`int left(int n) { return opaque(n) + left_state[n]; }`,
		`int right(int n) { return opaque(n) + right_state[n]; }`,
		`extern const int left_state[16]; extern const int right_state[32];`)
	for _, m := range config.Modules {
		m.Data = append(m.Data, symbolSpec{Name: "opaque", Addr: 0x500})
	}
	report, err = p.shareAudit(config, shareAuditOptions{})
	if err != nil {
		t.Fatal(err)
	}
	if len(report.Groups) != 1 {
		t.Fatalf("same unknown symbol should be flagged candidate: %+v", report)
	}
	roles := report.Groups[0].Participants[0].Roles
	if roles[0].Contract.Status != "unknown" || !strings.Contains(roles[1].Contract.Sites[0].Raw, "const") || !strings.Contains(roles[1].Contract.Sites[0].Raw, "[16]") {
		t.Fatalf("raw/unknown limits lost: %+v", roles)
	}
	if !strings.Contains(report.Groups[0].Participants[1].Roles[1].Contract.Sites[0].Raw, "[32]") {
		t.Fatal("same-line declaration excerpt selected the wrong symbol")
	}
	if !strings.Contains(strings.Join(report.ReviewLimits, " "), "array extents") {
		t.Fatal("lossy signature limit absent")
	}
}

func TestShareAuditMembersAndLocalShadowsAreNotRoles(t *testing.T) {
	p, config := shareAuditFixture(t,
		`int left(int n) { int left_callee; return left_callee + left_state.value; }`,
		`int right(int n) { int right_callee; return right_callee + right_state.value; }`, shareAuditHeader)
	for _, m := range config.Modules {
		m.Data = append(m.Data, symbolSpec{Name: "value", Addr: 0x600})
	}
	scanner, err := newShareAuditScanner(p, config)
	if err != nil {
		t.Fatal(err)
	}
	main, _ := config.module("main")
	entry, _, _, _, err := scanner.entry(main, &main.Functions[0])
	if err != nil {
		t.Fatal(err)
	}
	if len(entry.Roles) != 1 || entry.Roles[0].Symbol != "left_state" {
		t.Fatalf("member/shadow classified as global: %+v", entry.Roles)
	}
	report, err := p.shareAudit(config, shareAuditOptions{})
	if err != nil {
		t.Fatal(err)
	}
	if len(report.Groups) != 0 {
		t.Fatal("different local names were erased")
	}
}

func TestShareAuditExcludesSingleSourceAssemblyLibrariesAndFilters(t *testing.T) {
	p, config := shareAuditFixture(t, `int left(int n) { return n; }`, `int right(int n) { return n; }`, "")
	main, _ := config.module("main")
	world, _ := config.module("world")
	world.Functions[0].Source = "src/a/left.c"
	world.Functions[0].SourceSymbol = "left"
	report, err := p.shareAudit(config, shareAuditOptions{})
	if err != nil {
		t.Fatal(err)
	}
	if len(report.Groups) != 0 || report.Excluded["already_single_source_groups"] != 1 {
		t.Fatalf("already shared group = %+v", report)
	}
	world.Functions[0].Source = ""
	world.Functions[0].SourceSymbol = ""
	world.Functions[0].Asm = "existing architecture"
	report, err = p.shareAudit(config, shareAuditOptions{})
	if err != nil {
		t.Fatal(err)
	}
	if len(report.Groups) != 0 || report.Excluded["assembly"] != 1 {
		t.Fatal("assembly included")
	}
	world.Functions[0].Asm = ""
	world.Libraries = []librarySpec{{Addr: 0x80020000, End: 0x80020100}}
	report, err = p.shareAudit(config, shareAuditOptions{})
	if err != nil {
		t.Fatal(err)
	}
	if report.Excluded["library"] != 1 {
		t.Fatal("library range included")
	}
	world.Libraries = nil
	for _, options := range []shareAuditOptions{{minBytes: 133}, {prefix: "left"}, {modules: map[string]bool{"main": true}}} {
		report, err = p.shareAudit(config, options)
		if err != nil {
			t.Fatal(err)
		}
		if len(report.Groups) != 0 {
			t.Fatalf("filter ignored: %+v", options)
		}
	}
	main.Functions[0].Source = "src/platform/psyq/custom.c"
	report, err = p.shareAudit(config, shareAuditOptions{})
	if err != nil {
		t.Fatal(err)
	}
	if report.Excluded["library"] != 1 {
		t.Fatal("platform source included")
	}
}

func shareAuditTree(t *testing.T, root string) map[string][32]byte {
	t.Helper()
	result := map[string][32]byte{}
	err := filepath.WalkDir(root, func(path string, entry os.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if entry.IsDir() {
			return nil
		}
		data, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		relative, err := filepath.Rel(root, path)
		if err != nil {
			return err
		}
		result[relative] = sha256.Sum256(data)
		return nil
	})
	if err != nil {
		t.Fatal(err)
	}
	return result
}

func TestShareAuditCommandIsReadOnlyAndDeterministic(t *testing.T) {
	p := validTestProject(t)
	before := shareAuditTree(t, p.root)
	var first, second bytes.Buffer
	p.out = &first
	if err := p.shareAuditCommand([]string{"--min-bytes=1"}); err != nil {
		t.Fatal(err)
	}
	p.out = &second
	if err := p.shareAuditCommand([]string{"--min-bytes=1"}); err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(first.Bytes(), second.Bytes()) {
		t.Fatal("command output varies")
	}
	if !reflect.DeepEqual(before, shareAuditTree(t, p.root)) {
		t.Fatal("audit writes or deletes project files")
	}
	var report shareAuditReport
	if err := json.Unmarshal(first.Bytes(), &report); err != nil || !report.CandidateOnly {
		t.Fatalf("JSON report: %v", err)
	}
	for _, args := range [][]string{{"--min-bytes=-1"}, {"--module=unknown"}, {"unexpected"}} {
		if err := p.shareAuditCommand(args); err == nil {
			t.Fatalf("accepted invalid args: %v", args)
		}
	}
}
