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
  families). A shared source keeps program-specific code under `#ifdef
  FFT_X` blocks proven in every module that compiles the file, with a short
  comment saying what the other programs do. Statements the programs share
  stay outside the block; a function whose body differs throughout is
  written out once per program, each under its own block, rather than
  interleaved line by line. Sources test only `FFT_` and `VARIANT_` names
  in conditionals; a header may test `FFT_` names (a record laid out per
  program) but never `VARIANT_` names.
- Preserve retail bugs and ABI disagreements. Explain them briefly in source
  comments or `QUIRKS.md`.
- Keep drafts, experiments, extracted files and generated output under ignored
  `build/`. Never commit game assets (graphics, sound, movies, text and
  script dumps) or text copied from outside references (wikis, guides, SDK
  sources). Data tables recovered while decompiling the code belong in
  source; write a table as the rule that produces it when one reproduces
  every byte. A zero-filled buffer the original kept in `.data` needs an
  explicit `= {0}`; without it the compiler moves it to `.bss`.
- Stage explicit paths and inspect the staged diff. Preserve pushed history;
  make coherent commits with short subject lines when the user authorizes them.

## Organization

- `src/<program>/` (`main`, `battle`, `world`, `wldcore`, `open`): each
  program's own code as feature files of a few to a few dozen functions,
  named after what they hold. A file's leading comment briefly describes
  what the file holds.
- `src/event/`: the event library (script interpreter, threads, dialog and
  text, windows, sound queue) compiled into BATTLE and WORLD. `src/event/menu/`:
  the menu library compiled into WORLD and the EVENT overlays, as the small
  objects the overlays link in two different orders. `src/event/<overlay>/`:
  each overlay's own code. `src/effect/`: the 26 routines the effect
  overlays link. `src/lib/psyq/<library>/` and `src/lib/suzuki/`:
  reconstructed linked libraries; keep established public names, signatures
  and original module bindings.
- Sharing is recorded in `target/*.yaml` (which modules compile a file),
  never by folder. A shared file keeps its per-program differences under
  program macros; a standalone function with a separate body per program
  lives in each program's folder.
- File boundaries come from evidence first: hand-written and library
  objects, a change of compiler profile, jump-table alignment, read-only
  data that is not contiguous. Where the binary is silent, cut by topic to
  about 20 to 60 functions. A name prefix is never boundary evidence.
- `include/fft/`: game program, library and shared-type headers. A program's
  declarations live in topic headers (`world_shop.h`, `wldcore_map.h`,
  `main_sound.h`, `menu_panel.h`); the base header (`world.h`) keeps the
  types and declarations several topics share. `include/psx/`: library and
  PS1 hardware interfaces. Library code never includes game headers.
- `tools/`: Go build/check tooling. `tools/target.go` owns the original disc size
  and hash. `QUIRKS.md` records unusual behavior worth preserving.

Put declarations in their owning header. Sources include their own declaration
header and the focused headers for the APIs they use; a source several
programs compile includes what any of them needs. A header starts with its
guard, then a one-line description, then its includes. Group declarations by
topic under short section comments, with each topic's types first, then its
globals, then its functions; order topics so a type comes before its first
use. Leave blank lines between type blocks; headers include their own
dependencies.

## Code style

- Prefer ordinary C, named fields, direct assignments and conventional loops.
  Remove redundant locals unless a named temporary reads better: a long call
  or index expression, a value used more than once, or a step that deserves
  a name.
- Trace casts, shifts, masks and unions through readers and writers before
  changing types. Keep genuine encoded data and format-dispatch boundaries;
  avoid invented overlays or splitting records without evidence.
- Do not introduce register pins, asm barriers, `volatile` or raw offsets to
  force a cleanup. Retain existing residue only when measured necessary, with
  a concise reason. Keep architectural register access isolated.
- Name functions by program, then topic, then behavior. A program's own
  function carries its program token first (`battle_gfx_`, `battle_formula_`,
  `world_shop_`, `wldcore_bar_`, `attack_deploy_`, `main_file_`); a library
  function compiled into several programs carries only its topic (`script_`,
  `thread_`, `text_`, `menu_`, `gfx_`, `memcard_`, `bits_`, `window_`), so a
  bare prefix at a call site means shared code. Files carry no program
  token; their folder says it. A body compiled into one program carries
  that program's token, and a body several programs compile carries only
  its topic. When code several programs compile calls a function whose body
  differs by program, every body takes that one topic-only name, since the
  call site can name only one.
- A bytecode machine's handler for one opcode is named
  `<machine>_opcode_<nn>_<instruction>` with the opcode in two hex digits:
  `script_opcode_28_walk_to` (event scripts), `wldcore_sound_novel_opcode_*`,
  `wldcore_script_opcode_*` (world map scripts), `open_opcode_*`,
  `menu_opcode_*` (menu display scripts), `world_tutorial_opcode_*`. A
  handler shared by several opcodes takes the lowest. Use "opcode" only for
  an instruction stream that a dispatcher decodes; request codes and
  single records are commands.
- Globals follow the same rule: a global with one definition carries the
  defining program's token (`g_main_`, `g_battle_`, `g_bunit_`), and a name
  that shared code binds to a different definition in each program carries
  only its topic, with a comment saying which programs define it.
- Preserve game terminology. Name an unknown by what the code shows it
  doing, and mark a name that rests on thin evidence `Provisional:` in its
  comment; keep an address in a name only when nothing else is known.
- A function used only inside its own unit is `static` and appears in no
  header; when the file uses it before its definition, a static prototype
  follows the includes. Headers declare only what other files use.
- Use snake_case, `_t` for struct typedefs, `_e` for enum typedefs, uppercase
  enumerators, and `_init_`. Use decimal for counts/ordinary enum values and
  hex for flags and encoded values. Header guards have no `FFT_` prefix.
- Unknown members are `_unknown_XX`; `_unused_XX` means no named access;
  `_padding_XX` means proven alignment/size filler. Offsets are relative to the
  member's own struct. Never access unused or padding members by name.
- Use `ONE` for fixed-point 1.0 (4096). A name that states a fixed-point
  format uses `qN` for N fraction bits (`battle_lerp_q8`, `menu_lerp_q12`).
  Add `const` only for genuinely read-only inputs and verify all callers
  under their historical compiler profiles.
- Use American spelling.
- Keep comments factual. Say what a function or block does when the code
  does not make it obvious, and why a non-obvious step is there (a game
  rule, a retail quirk, a byte-matching constraint). Skip comments that
  restate the code. Reference library material validates our interfaces; do
  not copy implementations or comments into the repository.

## Workflow

Use the Makefile for compilation, containers and verification. Inspect callers,
callees and layouts before editing; measure uncertain candidates in `build/`.
Use `make symbols` for function/global renames and `make config-fmt` after target
changes. Shared-source consolidation must retain every original address and
call binding, without runtime wrappers or invented alias machinery.

```sh
make fmt [FILES="a.c b.h"]       # always before checking and committing C/headers
make check [MODULE=x]             # compile and compare original function hashes
make check-config                # sources, declarations and target metadata
make decl-check                  # calls to undeclared functions, per program view
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
run full `check`, `check-config`, `decl-check`, `fmt-check`, `test` and `build`. Formatting,
parameter widths, signedness and qualifiers can affect the original bytes.

## Agents

Split large work into disjoint files and give each worker its own ignored
`build/` scratch path. One lead owns shared headers/target integration and final
verification. Workers report affected entries, match evidence and proposed
header changes; only the lead commits when authorized.
