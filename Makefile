DOCKER ?= docker
# The tag follows the image inputs, so editing them rebuilds on the next call.
IMAGE_TAG := $(shell cat Dockerfile .dockerignore tools/go.mod tools/go.sum 2>/dev/null | cksum | cut -d' ' -f1)
IMAGE ?= fft-decomp-dev:$(IMAGE_TAG)
BEAR ?= bear
EDITOR_CC ?= clang
PCSX_REDUX ?= /Applications/PCSX-Redux.app/Contents/MacOS/PCSX-Redux

.DEFAULT_GOAL := help

HOST_USER := $(shell id -u):$(shell id -g)

# Optional tool overrides, passed only when given: JOBS (compile jobs, default
# one per CPU; permuter threads, default 4), CACHE=0 (recompile everything),
# WORK_DIR (per-function intermediates, default /tmp/fft-build in the
# container; empty keeps them under build/work), REPORT_ALL=1 (list every
# mismatching function) and DURATION (permuter seconds, default 300).
TOOL_ENV = $(if $(JOBS),--env TOOLS_JOBS=$(JOBS)) \
	$(if $(CACHE),--env TOOLS_CACHE=$(CACHE)) \
	$(if $(filter-out undefined,$(origin WORK_DIR)),--env TOOLS_WORK_DIR=$(WORK_DIR)) \
	$(if $(REPORT_ALL),--env TOOLS_REPORT_ALL_MISMATCHES=$(REPORT_ALL))

# Every command runs in a fresh container as the host user, so files written
# under the bind-mounted checkout keep the caller's ownership. --init forwards
# Ctrl-C and reaps the permuter's children.
DOCKER_RUN = $(DOCKER) run --rm --init --pull=never --user "$(HOST_USER)" \
	--volume "$(CURDIR):/work" $(strip $(TOOL_ENV))

# tools/ is rebuilt from the working tree on every call (incrementally, via
# build/cache/go), so edits apply on the next command. A container-local copy
# runs, since Docker Desktop can report a just-written bind-mounted binary busy.
TOOLS = $(DOCKER_RUN) $(IMAGE) sh -c 'go build -C tools -o ../build/bin/tools . && cp build/bin/tools /tmp/tools && exec /tmp/tools "$$@"' tools

# clang-format comes from the image, so every host formats identically.
FORMAT = $(DOCKER_RUN) $(IMAGE) bash -o pipefail -c 'find src include -type f \( -name "*.c" -o -name "*.h" \) -print0 | sort -z | xargs -0 -r clang-format --style=file "$$@"' clang-format

.PHONY: help
help: ## Show this help
	@printf 'Final Fantasy Tactics matching decompilation\n\n'
	@grep -E '^[a-zA-Z_-]+:.*?## ' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "  %-18s %s\n", $$1, $$2}'
	@printf '\nFUNC is looked up in target/*.yaml; MODULE=id picks one when several define it.\n'

.PHONY: _docker-build
_docker-build:
	$(DOCKER) build --tag $(IMAGE) .

# Build the image only when it is missing; `make bootstrap` always rebuilds.
.PHONY: image
image:
	@$(DOCKER) image inspect "$(IMAGE)" >/dev/null 2>&1 || $(MAKE) --no-print-directory _docker-build

.PHONY: image-name
image-name:
	@echo "$(IMAGE)"

.PHONY: bootstrap
bootstrap: _docker-build ## Rebuild the Docker image, check-config, and extract scus-94221.bin if present
	@$(TOOLS) check-config
	@if [ -f scus-94221.bin ]; then $(MAKE) --no-print-directory extract; \
	else echo 'No scus-94221.bin: skipped extraction. `make validate` works without it; `make build` needs it.'; fi

.PHONY: extract
extract: image ## Extract scus-94221.bin (automatic for build and checksums)
	@$(TOOLS) extract

.PHONY: build
build: extract ## Build and verify every module and the disc (MODULE=x for one module)
	@$(TOOLS) build $(if $(strip $(MODULE)),"$(MODULE)",disc)

.PHONY: validate
validate: image ## Compile and compare with target/ hashes, no BIN needed (MODULE=x)
	@$(TOOLS) validate $(if $(strip $(MODULE)),--module="$(MODULE)")

# FUNC names a configured function; MODULE is needed only when it is ambiguous.
function_args = $(if $(strip $(MODULE)),--module="$(MODULE)") "$(FUNC)"

.PHONY: diff
diff: image ## Compare FUNC=name with the original bytes (needs the BIN)
	@test -n "$(FUNC)" || { echo "FUNC is required: make diff FUNC=<function> [MODULE=<id>]"; exit 2; }
	@$(TOOLS) diff $(function_args)

.PHONY: permute
permute: image ## Run decomp-permuter on FUNC=name (DURATION=300 JOBS=4)
	@test -n "$(FUNC)" || { echo "FUNC is required: make permute FUNC=<function> [MODULE=<id>] [DURATION=300] [JOBS=4]"; exit 2; }
	@$(TOOLS) permute $(if $(DURATION),--duration="$(DURATION)") $(function_args)

.PHONY: checksums
checksums: extract ## Record function hashes in target/ from the original bytes
	@$(TOOLS) checksums

.PHONY: check-config
check-config: image ## Validate project metadata (target/, sources, declarations)
	@$(TOOLS) check-config

.PHONY: config-fmt
config-fmt: image ## Rewrite target/*.yaml in canonical form
	@$(TOOLS) config-fmt

.PHONY: declarations
declarations: image ## Report conflicting file-scope declarations (ARGS="...")
	@$(TOOLS) declarations $(ARGS)

.PHONY: symbols
symbols: image ## Rename a symbol: ACTION=rename-function|rename-global ARGS="--old A --new B"
	@test -n "$(ACTION)" || { echo "ACTION is required: rename-function or rename-global"; exit 2; }
	@$(TOOLS) symbols "$(ACTION)" $(ARGS)

.PHONY: test
test: image ## Vet and test the Go tooling
	@$(DOCKER_RUN) $(IMAGE) sh -c 'cd tools && go vet ./... && go test ./...'

# cc1 runs with -gcoff, so moving lines can change the bytes: `make validate` after.
.PHONY: fmt
fmt: image ## clang-format every C source and header in place
	@$(FORMAT) -i

.PHONY: fmt-check
fmt-check: image ## Fail if any C source or header is not clang-formatted
	@$(FORMAT) --dry-run --Werror

.PHONY: run
run: ## Build the disc and launch PCSX-Redux (PCSX_REDUX=path; default is the macOS app)
	$(MAKE) --no-print-directory build MODULE=
	test -x "$(PCSX_REDUX)"
	"$(PCSX_REDUX)" \
		-iso "$(CURDIR)/build/disc/output-scus-94221.cue" \
		-run

.PHONY: shell
shell: image ## Open an interactive shell in the Docker image
	@$(DOCKER_RUN) --interactive --tty $(IMAGE) bash

# Runs on the host so paths point into this checkout. Clang rejects a few
# redeclarations GCC 2.6.3 accepts; those files are still recorded.
.PHONY: compile-commands
compile-commands: ## Create editor compilation metadata (host bear and clang)
	@command -v "$(BEAR)" >/dev/null || { echo "Bear is required (set BEAR=/path/to/bear)"; exit 2; }
	@command -v "$(EDITOR_CC)" >/dev/null || { echo "Clang is required (set EDITOR_CC=/path/to/clang)"; exit 2; }
	$(BEAR) --output compile_commands.json -- /bin/sh -c 'set -eu; find src -type f -name "*.c" -print | sort | while IFS= read -r source; do "$(EDITOR_CC)" -target mipsel-none-elf -march=mips1 -mabi=32 -msoft-float -fsyntax-only -undef -nostdinc -Iinclude -Wno-everything "$$source" >/dev/null 2>&1 || :; done'
