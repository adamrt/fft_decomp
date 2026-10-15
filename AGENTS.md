# AGENTS.md

A byte-exact C decompilation of the USA PlayStation release of *Final Fantasy
Tactics* (`SCUS-94221`). Improve names, types, organization and readability while
preserving every original byte.

## Invariants

- Never weaken checks or change hashes to accept edited code. Only
  `make checksums` updates hashes, using original bytes.
- `target/*.yaml` owns each function's address, size, compiler profile, hash and
  source binding. Preserve these contracts; shared sources still compile at
  every original entrypoint with that entry's profile and bindings. A module's
  `units:` list names merged sources (original translation units) compiled
  once per module; a member row carries `unit:` instead of `source:`, members
  follow one another in address order, and the file defines exactly those
  functions in that order, as that module sees the file. A module's
  `defines:` map holds the `VARIANT_` constants (passed as `-D`) that let one
  source serve programs built with different values; only a trial-proven
  constant goes there, and headers never use them. Every compile also
  defines the program macro `FFT_<MODULE>` (`FFT_BATTLE`, `FFT_WORLD`,
  `FFT_EVENT_BUNIT`, plus `FFT_EVENT` and `FFT_EFFECT` for the overlay
  families). A shared source may keep a program-specific block under
  `#ifdef FFT_X` only as the minimal difference a trial proved in every
  module that compiles the file, with a one-line comment saying what the
  other program does. Sources test only `FFT_` and `VARIANT_` names in
  conditionals; headers test neither.
- Preserve retail bugs and ABI disagreements. Explain them briefly in source
  comments or `QUIRKS.md`.
- Keep drafts, experiments, extracted files and generated output under ignored
  `build/`. Never commit game media or copied reference material.
- Stage explicit paths and inspect the staged diff. Preserve pushed history;
  make coherent commits with short subject lines when the user authorizes them.

## Organization

- `src/<subsystem>/`: all code of a subsystem (`menu`, `gfx`, `sound`, `text`,
  `script`, `thread`, `camera`, `map`, `unit`, `item`, `job`, `card`, `file`,
  `window`, `bits`, `memory`, `status`), one file per function or one unit
  file, named after it. A bare subsystem prefix is a shared routine; a module
  prefix (`battle_gfx_`, `wldcore_menu_`) is that program's own routine of the
  subsystem. A `_variant` body that only one program links sits in that
  program's subfolder, `src/<subsystem>/<module>/`. Register explicit `source:`
  paths in each original module; preserve meaningful separate entries.
- `src/<module>/`: program-specific code outside any subsystem (battle actions
  and formulas, the world map opcodes, overlay screens). Each EVENT overlay
  has its own `src/event/<overlay>/`.
- `src/lib/psyq/<library>/` and `src/lib/suzuki/`: reconstructed linked libraries.
  Keep established public names, signatures and original module bindings.
- `include/fft/`: game module, subsystem and shared-type headers.
  `include/psx/`: library and PS1 hardware interfaces. Library code never
  includes game headers.
- `tools/`: Go build/check tooling. `tools/target.go` owns the original disc size
  and hash. `QUIRKS.md` records unusual behavior worth preserving.

Put declarations in their owning header. Sources include their own declaration
header and the focused headers for APIs they use. Group related declarations
under short subsystem comments: types, globals, then functions. Leave blank
lines between type blocks; subsystem headers include their own dependencies,
not the module umbrella.

## Code style

- Prefer ordinary C, named fields, direct assignments and conventional loops.
  Remove redundant locals where the clean spelling still matches.
- Trace casts, shifts, masks and unions through readers and writers before
  changing types. Keep genuine encoded data and format-dispatch boundaries;
  avoid invented overlays or splitting records without evidence.
- Do not introduce register pins, asm barriers, `volatile` or raw offsets to
  force a cleanup. Retain existing residue only when measured necessary, with
  a concise reason. Keep architectural register access isolated.
- Use established module/subsystem prefixes and concise, behavior-based names.
  A shared thing takes one subsystem name without a module prefix; a prefix
  stays only on things one module has. The `_variant` suffix is reserved for a
  function with more than one body under one shared name: bodies in several
  folders, or several conditional definitions in one shared file
  (check-config enforces it both ways); name other alternates by what
  distinguishes them. Preserve game terminology and evidence-based unknown
  names rather than guessing.
- Use snake_case, `_t` for struct typedefs, `_e` for enum typedefs, uppercase
  enumerators, and `_init_`. Use decimal for counts/ordinary enum values and
  hex for flags and encoded values. Header guards have no `FFT_` prefix.
- Unknown members are `_unknown_XX`; `_unused_XX` means no named access;
  `_padding_XX` means proven alignment/size filler. Offsets are relative to the
  member's own struct. Never access unused or padding members by name.
- Use `ONE` for fixed-point 1.0 (4096). Add `const` only for genuinely read-only
  inputs and verify all callers under their historical compiler profiles.
- Use American spelling.
- Keep comments factual and short: explain behavior or a necessary constraint.
  Reference library material validates our interfaces; do not copy
  implementations or comments into the repository.

## Workflow

Use the Makefile for compilation, containers and verification. Inspect callers,
callees and layouts before editing; measure uncertain candidates in `build/`.
Use `make symbols` for function/global renames and `make config-fmt` after target
changes. Shared-source consolidation must retain every original address and
call binding, without runtime wrappers or invented alias machinery.

```sh
make fmt                         # always before checking and committing C/headers
make check [MODULE=x]             # compile and compare original function hashes
make check-config                # sources, declarations and target metadata
make fmt-check                   # formatting drift
make test                        # Go tooling tests
make build                       # rebuild the byte-identical disc
make diff FUNC=f [MODULE=x]       # inspect one original-byte mismatch
make library-diff ARGS="..."      # measure a scratch library candidate
make compile-commands            # refresh editor metadata after source moves
make symbols ACTION=unit ARGS="--name N --source src/x/N.c M:E..."  # merge entries (or whole units) into a unit
make units ARGS="--evidence=required|strong"  # unit map from binary evidence only
```

`MODULE` accepts module IDs, overlay aliases, lists, `event` or `effect`.
`make check` needs no disc; `make build` and `make diff` use `scus-94221.bin`.
Run `make fmt` after C/header edits, then matching checks. Before handing off,
run full `check`, `check-config`, `fmt-check`, `test` and `build`. Formatting,
parameter widths, signedness and qualifiers can affect the original bytes.

## Agents

Split large work into disjoint files and give each worker its own ignored
`build/` scratch path. One lead owns shared headers/target integration and final
verification. Workers report affected entries, match evidence and proposed
header changes; only the lead commits when authorized.
