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
  `units:` list names merged sources (reconstructed translation units) compiled
  once per module; a member row carries `unit:` instead of `source:`, members
  follow one another in address order, and the file defines exactly those
  functions in that order, as that module sees the file. A module's
  `defines:` map holds the `VARIANT_` constants (passed as `-D`) that let one
  source serve programs built with different values; only a trial-proven
  constant goes there, and headers never use them. Every compile also
  defines the program macro `FFT_<MODULE>` (`FFT_BATTLE`, `FFT_WORLD`,
  `FFT_EVENT_BUNIT`, plus `FFT_EVENT` and `FFT_EFFECT` for the overlay
  families). A shared source keeps small program-specific variations under
  `#ifdef FFT_X` blocks proven in every module that compiles the file, with
  a short comment explaining the difference. Statements the programs share
  stay outside the block. When substantial implementations differ, prefer
  coherent feature files in the respective programs; do not force sharing
  through whole alternate bodies or a thicket of conditionals. Small helpers
  may remain duplicated with their owning feature. Sources test only `FFT_` and `VARIANT_` names
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
- `src/lib/`: shared game components, grouped by concept. `event/` holds
  the script interpreter, dialog and related BATTLE/WORLD services; `menu/`
  holds shared in-game menus, windows, text and drawing support, grouped by
  feature while preserving each program's function order and profiles. `thread/`,
  `sound/` and `file/` hold their standalone shared units; related functions
  inside mixed original units stay with those units. `memcard.c` holds the
  shared memory-card unit. `suzuki/` is the linked sound engine; keep its
  established names, signatures and original module bindings.
- `src/event/<overlay>/`: each EVENT overlay's own code. `src/effect/`:
  the native callbacks linked into EFFECT overlays; the resident effect
  runtime stays in BATTLE. `src/platform/psyq/<library>/`: reconstructed
  PS1 SDK and runtime libraries, preserving their public interfaces and
  module bindings. Analysis tools exclude platform code and target-declared
  library ranges, not the shared game components under `src/lib/`.
- Sharing is recorded in `target/*.yaml` (which modules compile a file),
  never by folder. A shared file keeps its per-program differences under
  program macros; a standalone function with a separate body per program
  lives in each program's folder.
- File boundaries come from evidence first: hand-written and library
  objects, a change of compiler profile, jump-table alignment, read-only
  data that is not contiguous. Where the binary is silent, cut by topic to
  about 20 to 60 functions. A name prefix is never boundary evidence.
  Existing single-function files are not evidence of original boundaries;
  different function orders can reflect separate objects or edited source copies.
- `include/fft/`: one flat folder of game headers named by concept
  (`unit.h`, `shop.h`, `proposition.h`, `battle_map_load.h`, `menu_stack.h`),
  each declaring one part of the game for every program that uses it. A
  large concept splits by sub-topic (`unit.h` and `unit_runtime.h`,
  `effect.h` and `effect_geometry.h`); add folders only when size calls for
  them. Suzuki's interface belongs here too, separate from game sound and
  music services. `include/psx/`: PsyQ and PS1 hardware interfaces. Platform
  library code never includes game headers.
- `tools/`: Go build/check tooling. `tools/target.go` owns the original disc size
  and hash. `QUIRKS.md` records unusual behavior worth preserving.

Put declarations in the header of their concept. Sources include the headers
of the concepts they use; a source several programs compile includes what any
of them needs. A header starts with its guard, then a one-line description,
then its includes. Group declarations by sub-topic under short lowercase
section comments, with each section's types first, then its globals, then its
functions; order sections so a type comes before its first use. Plain
declarations run together without blank lines, a size assert follows its type
directly, and blank lines separate type blocks and come before a commented
declaration. Headers include their own dependencies and compile alone; a
header that only passes a record around names it by its tag (`struct unit*`)
instead of including the record's header.

## Code style

- Represent only what the original game did. Add no abstractions, helper
  layers or scaffolding for code the game did not have; a macro, header or
  helper exists only when it expresses something the original contained.
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
- Name a function by concept, optional sub-topic, then behavior
  (`shop_buy_`, `proposition_open_returned_list`, `battle_map_load_data`,
  `unit_panel_`). The concept is the game's own US term where the player
  sees one (bar, rumor, proposition, Brave Story, sound novel, formation,
  reaction, AT List); code with no player concept takes a mechanism topic
  (`script_`, `thread_`, `text_`, `menu_`, `screen_`, `window_`, `gfx_`,
  `memcard_`, `bits_`, `system_`, `file_`, `heap_`, `overlay_`, `input_`,
  `render_`, `controller_`). A sub-topic leads when the player picks it as a
  mode (`shop_buy_`, `formation_job_`). Except for program entrypoints below,
  names carry no program token; the folder says which program a body belongs
  to. One role takes one name in every program, so bodies that differ by
  program share it (CARD's and WORLD's `save_build_image`); different roles
  take different names, and every name is unique across programs except
  same-role bodies, which never share a link.
- Name execution roles consistently: `_init` sets up state or resources,
  `_run` completes a loop or operation, `_update` advances one step,
  `_start` launches scheduled work, and `_dispatch` selects a behavior.
- Program entrypoints are the naming exception: use `<program>_entrypoint`,
  or `<program>_<role>_entrypoint` for independent entrances to one overlay
  (`small_entrypoint`, `open_title_entrypoint`, `open_ending_entrypoint`).
  Identify them from the callers that enter the loaded program, not function
  order or export status. Comments describe whether the entry runs a loop,
  advances a frame or initializes resources. Same-role resident copies keep
  the entrypoint name. Preserve `main` and established runtime/library names;
  ordinary exported helpers and effect callbacks keep behavior names.
- A bytecode machine's handler for one opcode is named
  `<machine>_opcode_<nn>_<instruction>` with the opcode in two hex digits:
  `script_opcode_28_walk_to` (event scripts), `sound_novel_opcode_*`,
  `location_script_opcode_*` (world map scripts), `ending_opcode_*`,
  `menu_opcode_*` (menu display scripts), `tutorial_opcode_*`,
  `effect_script_opcode_*`. A
  handler shared by several opcodes takes the lowest. Use "opcode" only for
  an instruction stream that a dispatcher decodes; request codes and
  single records are commands.
- Globals follow the same rule (`g_shop_`, `g_unit_`, `g_menu_stack_`); a
  name that shared code binds to a different definition in each program
  carries a comment saying which programs define it. Materially different
  record layouts take distinct types; similar roles do not justify hiding
  different representations behind one conditional typedef.
- Preserve game terminology. Name an unknown by what the code shows it
  doing, and mark a name that rests on thin evidence `Provisional:` in its
  comment; keep an address in a name only when nothing else is known.
- A function used only inside its own unit is `static` and appears in no
  header; when the file uses it before its definition, a static prototype
  follows the includes. Headers declare only what other files use. A
  private body whose role another program declares in a shared header drops
  `static` and takes that declaration.
- Use snake_case, `_t` for struct typedefs, `_e` for enum typedefs (the tag
  repeats the typedef's name without its suffix), uppercase enumerators, and
  `_init_`. A size assert is named after its type (`unit_size_must_be_N`). Use decimal for counts/ordinary enum values and
  hex for flags and encoded values. Header guards have no `FFT_` prefix.
- Unknown members are `_unknown_XX`; `_unused_XX` means no named access;
  `_padding_XX` means proven alignment/size filler. Offsets are relative to the
  member's own struct. Never access unused or padding members by name.
- Use `ONE` for fixed-point 1.0 (4096). A name that states a fixed-point
  format uses `qN` for N fraction bits (`battle_lerp_q8`, `menu_lerp_q12`).
  Add `const` only for genuinely read-only inputs and verify all callers
  under their historical compiler profiles.
- Use American spelling.
- Prefer `s8`/`u8`, `s16`/`u16` and `s32`/`u32` for explicit-width values.
  PS1 SDK `long` values are 32-bit; express them as `s32`/`u32` consistently
  in declarations, definitions and callers instead of adding pointer casts.
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
