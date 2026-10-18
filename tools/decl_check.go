// decl_check.go compiles every C source once per program view with the
// historical cc1 in syntax-only mode and reports calls to undeclared
// functions. The matching compile runs with -w, so an implicit declaration
// (which still produces the right bytes for an int-returning call) would
// otherwise go unnoticed.
package main

import (
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
	"sync"
)

// declCheckView is one source as one module preprocesses it.
type declCheckView struct {
	source  string
	module  string
	defines []string
}

var implicitDeclaration = regexp.MustCompile("implicit declaration of function `([A-Za-z_][A-Za-z0-9_]*)'")

// declCheckViews lists each C source once per distinct set of preprocessor
// flags among the modules that compile it.
func declCheckViews(config *projectConfig) []declCheckView {
	seen := map[string]bool{}
	var views []declCheckView
	for _, m := range config.Modules {
		flags := m.defineFlags()
		for index := range m.Functions {
			f := &m.Functions[index]
			if f.Asm != "" {
				continue
			}
			source := m.sourceOf(f)
			if !strings.HasSuffix(source, ".c") {
				continue
			}
			key := source + "\x00" + strings.Join(flags, " ")
			if seen[key] {
				continue
			}
			seen[key] = true
			views = append(views, declCheckView{source: source, module: m.ID, defines: flags})
		}
	}
	sort.Slice(views, func(i, j int) bool {
		if views[i].source != views[j].source {
			return views[i].source < views[j].source
		}
		return views[i].module < views[j].module
	})
	return views
}

func (p project) declCheckCommand() error {
	config, err := p.loadProjectConfig()
	if err != nil {
		return err
	}
	profile, ok := compilerProfileNamed(config.byID["main"].Profile)
	if !ok {
		return fmt.Errorf("unknown compiler profile %q", config.byID["main"].Profile)
	}
	workDir := filepath.Join(p.root, "build", "decl-check")
	if err := os.MkdirAll(workDir, 0o755); err != nil {
		return err
	}
	views := declCheckViews(config)
	jobs, err := toolJobs()
	if err != nil {
		return err
	}
	var (
		mutex    sync.Mutex
		problems []string
		failures []error
		checked  = map[string]bool{} // preprocessed text hash: identical views are checked once
		wait     sync.WaitGroup
		queue    = make(chan declCheckView)
	)
	for worker := 0; worker < jobs; worker++ {
		wait.Add(1)
		go func(worker int) {
			defer wait.Done()
			preprocessed := filepath.Join(workDir, fmt.Sprintf("view-%d.i", worker))
			for view := range queue {
				cppDir, cppSource := p.preprocessInput(filepath.Join(p.root, view.source))
				cpp := append([]string{"mipsel-linux-gnu-cpp"}, p.cppFlags()...)
				cpp = append(cpp, view.defines...)
				cpp = append(cpp, cppSource, "-o", preprocessed)
				command := exec.Command(cpp[0], cpp[1:]...)
				command.Dir = cppDir
				if output, err := command.CombinedOutput(); err != nil {
					mutex.Lock()
					failures = append(failures, fmt.Errorf("%s [%s]: preprocess: %w\n%s", view.source, view.module, err, strings.TrimSpace(string(output))))
					mutex.Unlock()
					continue
				}
				data, err := os.ReadFile(preprocessed)
				if err != nil {
					mutex.Lock()
					failures = append(failures, err)
					mutex.Unlock()
					continue
				}
				sum := sha256.Sum256(data)
				hash := hex.EncodeToString(sum[:])
				mutex.Lock()
				duplicate := checked[hash]
				checked[hash] = true
				mutex.Unlock()
				if duplicate {
					continue
				}
				cc1 := append(cc1Command(profile.compilerPath), "-quiet", "-fsyntax-only", "-Wimplicit", preprocessed, "-o", os.DevNull)
				command = exec.Command(cc1[0], cc1[1:]...)
				command.Dir = p.root
				output, err := command.CombinedOutput()
				var found []string
				for _, match := range implicitDeclaration.FindAllStringSubmatch(string(output), -1) {
					found = append(found, match[1])
				}
				mutex.Lock()
				if err != nil {
					failures = append(failures, fmt.Errorf("%s [%s]: cc1: %w\n%s", view.source, view.module, err, strings.TrimSpace(string(output))))
				}
				for _, name := range found {
					problems = append(problems, fmt.Sprintf("%s [%s]: implicit declaration of %s", view.source, view.module, name))
				}
				mutex.Unlock()
			}
		}(worker)
	}
	for _, view := range views {
		queue <- view
	}
	close(queue)
	wait.Wait()
	sort.Strings(problems)
	for _, problem := range problems {
		fmt.Println(problem)
	}
	if len(failures) > 0 {
		return errors.Join(failures...)
	}
	if len(problems) > 0 {
		return fmt.Errorf("decl-check: %d implicit declarations", len(problems))
	}
	fmt.Printf("decl-check: %d program views of %d sources declare every function they call\n", len(checked), countSources(views))
	return nil
}

func countSources(views []declCheckView) int {
	sources := map[string]bool{}
	for _, view := range views {
		sources[view.source] = true
	}
	return len(sources)
}
