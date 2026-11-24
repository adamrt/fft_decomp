# AGENTS.md

A byte-exact C decompilation of the USA PlayStation release of *Final Fantasy
Tactics* (`SCUS-94221`). Improve names, types, organization and readability while
preserving every original byte.

## Invariants

- Never weaken checks or change hashes to accept edited code. Only
  `make checksums` updates hashes, using original bytes.
- `target/*.yaml` owns each function's address, size, compiler profile, hash
  and source binding. Shared sources still compile at every original
  entrypoint with that entry's profile and bindings.
- A module's `units:` list names merged sources (reconstructed translation
  units) compiled once per module. A member row carries `unit:` instead of
  `source:`; members follow one another in address order, and the file
  defines exactly those functions in that order, as that module sees the file.
- Every compile defines the program macro `FFT_<MODULE>` (`FFT_BATTLE`,
  `FFT_WORLD`, `FFT_EVENT_BUNIT`, plus `FFT_EVENT` and `FFT_EFFECT` for the
  overlay families). A module's `defines:` map holds the trial-proven
  `VARIANT_` constants (passed as `-D`) that let one source serve programs
  built with different values. Sources test only `FFT_` and `VARIANT_` names;
  headers may test `FFT_` names, never `VARIANT_` names.
- Preserve retail bugs and ABI disagreements; explain them briefly in source
  comments or `QUIRKS.md`.
- Keep drafts, experiments, extracted files and generated output under ignored
  `build/`. Never commit game assets (graphics, sound, movies, text and script
  dumps) or text copied from outside references (wikis, guides, SDK sources).
  Data tables recovered while decompiling belong in source; write a table as
  the rule that produces it when one reproduces every byte. A zero-filled
  buffer the original kept in `.data` needs an explicit `= {0}`.
- Stage explicit paths and inspect the staged diff. Preserve pushed history;
  make coherent commits with short subject lines when the user authorizes them.

## Where things live

- `src/<program>/` (`main`, `battle`, `world`, `wldcore`, `open`): each
  program's own code, in feature folders where they help. BATTLE's folders
  follow its six link images, each with its own code and data (`core/`,
  `map/`, `event/`, `calc/`, `ai/`, `effects/`). Keep a feature's callbacks,
  private helpers, tables and working records together; file size and
  function count are not reasons to split it.
- `src/lib/`: shared game components by concept: `event/` (the script
  interpreter, dialog and scene services of BATTLE and WORLD), `menu/` (shared
  menus, windows, text and drawing), `thread/`, `sound/`, `file/`,
  `memcard.c`, and `suzuki/` (the linked sound engine; keep its established
  names, signatures and module bindings).
- `src/event/<overlay>/`: each EVENT overlay's own code. `src/effect/`: the
  native callbacks linked into EFFECT overlays; the resident effect runtime
  stays in BATTLE. `src/platform/psyq/<library>/`: reconstructed PS1 SDK
  libraries, keeping their public interfaces and module bindings; platform
  code never includes game headers, and analysis tools skip it.
- `include/fft/<folder>/`: game headers in layer folders, from the bottom:
  `sys/` (system loop, threads, memory, files, overlays, input, sound, music,
  math, Suzuki), then `gfx/`, `text/` and `menu/`, then `game/` (records every
  mode shares), then the modes `battle/`, `map/` (the battle map), `effect/`,
  `script/` (the event interpreter), `world/` (WLDCORE's world map), `party/`
  (WORLD's roster screens) and `front/` (title, prologue, ending). A header
  includes only its own layer or lower. `include/psx/`: PsyQ and PS1
  hardware interfaces.
- `tools/`: Go build and check tooling; `tools/target.go` owns the original
  disc size and hash. `QUIRKS.md` records unusual behavior worth preserving;
  `ARCHAEOLOGY.md` records what the binary shows about the original build.
- Put a program's entry-owning unit in `entry.c`, or `entry_<role>.c` when
  separate units own independent entrances; keep the whole unit together.
  README's entrance index lists shared entrances, resident copies and
  service-only programs. Identify entrances from their callers; native EFFECT
  callbacks keep their behavior files.
- A file's leading comment explains its role in the game for a programmer
  familiar with FFT, in a short paragraph when useful. Avoid generic
  summaries, function inventories and filler.

## Headers

- Put declarations in the header of their concept. A header is named for the
  owner prefix it declares, in full (`map/map_mesh.h` declares `map_mesh_`).
  A large library (`menu_`, `gfx_`, `effect_`, `sprite_`, `map_`, `script_`)
  splits by sub-topic across headers in its folder; a small owner that belongs
  to a larger concept shares its header (`rumor_` in `bar.h`). The header tree
  is the index of owners: look there before inventing one.
- A header starts with its guard (no `FFT_` prefix), a one-line description,
  then its includes. Group declarations under short lowercase section
  comments, each section's types first, then globals, then functions, with
  sections ordered so a type comes before its first use. Plain declarations
  run together; a size assert follows its type directly; blank lines separate
  type blocks and come before a commented declaration.
- Headers compile alone. A header that only passes a record around names it
  by its tag (`struct map_unit*`) instead of including its header.
- Headers declare only what other files use. A source includes the headers of
  the concepts it uses; a source several programs compile includes what any of
  them needs.
- A record laid out per program, or a same-role function whose signature
  differs by program, may be declared under `#ifdef FFT_X` blocks.

## Sharing and file boundaries

- Sharing is recorded in `target/*.yaml` (which modules compile a file), never
  by folder. Identical bodies stay in one shared file, even where the binary
  shows each program compiled them inside a different object; record those
  object facts in `ARCHAEOLOGY.md` instead of duplicating code.
- A shared file keeps small program-specific variations under `#ifdef FFT_X`
  blocks proven in every module that compiles it, with a short comment; shared
  statements stay outside the blocks. When implementations differ
  substantially, keep separate files per program rather than whole alternate
  bodies or a thicket of conditionals. Functions that keep one order in every
  program can share a file; functions that change order between programs were
  copied and stay apart (`make order` reports both).
- File boundaries come from binary evidence. A boundary is proven by a
  hand-written or library object, a compiler-profile change, or jump-table
  alignment (gcc 2.6.3 aligns jump tables within an object). Two files whose
  variables interleave in one `.data` or `.bss` block were one object, where
  the linker keeps each object's data together (WORLD, the EVENT overlays;
  not BATTLE). Different function orders and name prefixes are not evidence.
- Where the binary is silent (most of BATTLE's core image), prefer a coherent
  owner and split only for an independent responsibility. Existing
  single-function files prove nothing about original boundaries.

## Naming

### Shape
- Public functions are `<owner>_<verb>_<object>`: the owner is the game
  concept, then the operation as it reads in English (`battle_unit_refresh_stats`,
  `camera_get_zoom`, `map_palette_set_colors`). An operation with no object
  ends at its verb.
- The owner is usually the module and its noun at once (`at_list_advance_cursor`).
  A sub-noun joins the owner only for a family of its own with several
  operations and usually its own record (`effect_slot_free`), a mode the
  player picks (`shop_buy_`, `shop_soldier_hire_`) or a storefront of a shared
  screen (`shop_fur_`). A parent subsystem leads only when the noun would be
  ambiguous without it (`map_unit_get_tile`, but `map_mesh_`).
- `map_` is the battle map as a level (terrain, mesh, tiles, files, palettes,
  lighting); `world_map_` is the world map. A function on a unit record takes
  the record as its owner (`battle_unit_`, `map_unit_`, `party_unit_`);
  `unit_panel_`, `unit_list_` and `unit_view_` are screens.
- `gfx_` is the shared GPU layer; `<owner>_gfx_` is an owner's own primitive
  pools or draw contexts (`menu_gfx_`, `save_gfx_`); an owner that draws
  through others' pools uses `<owner>_draw_`.
- Use the game's own US term where the player sees one (rumor, proposition,
  Brave Story, formation, reaction, AT List), shortened when the short word is
  unambiguous (`novel_`). Code with no player concept takes a mechanism topic
  (`script_`, `thread_`, `menu_`, `fade_`, `file_`).
- Decide owners family by family; never reorder words mechanically or make a
  status-returning operation sound like a boolean predicate. Keep readable
  predicates and phrases (`map_unit_find_at_tile`, `menu_thread_is_running`,
  `calendar_date_to_day_index`).

### Lookups
- A lookup by another record's id names its key last as `by_<key>`, spelled
  like the field (`map_unit_get_by_battle_unit_id` returns the record,
  `map_unit_get_id_by_battle_unit_id` the id). A lookup by the record's own id
  needs no key (`map_unit_get`), unless a pointer version has the same name:
  the pointer version keeps it and the id version adds `_by_id`.
- When the noun after a subsystem is a record, the function takes that
  record's id (`animation_map_unit_set_walk(map_unit_id)` beside
  `animation_set_walk(map_unit_t*)`).

### Roles
- `_init` sets up state or resources, `_run` completes a loop or operation,
  `_update` advances one step, `_start` launches scheduled work, `_dispatch`
  selects a behavior. The role word is the verb, before its object
  (`shop_run_screen`); when that would mislead, name what is stepped
  (`item_list_update_sort_menu`) rather than adding `_frame`.
- Thread bodies end in `_thread`. A public state-machine handler names its
  state before its role (`battle_state_free_cursor_update`).
- A bytecode machine's handler is `<machine>_opcode_<instruction>`
  (`script_opcode_walk_to`), and the machine's `<machine>_opcode_e` enum
  names each opcode as `<MACHINE>_OPCODE_<INSTRUCTION>`; the handler's
  comment lists the opcodes it handles as `MENU_OPCODE_QUAD (0x04)`.
  "Opcode" is only for an instruction stream a dispatcher decodes; request
  codes and single records are commands.
- Program entrypoints are `<program>_entrypoint` or
  `<program>_<role>_entrypoint` for independent entrances to one overlay
  (`open_title_entrypoint`). Identify them from the callers that enter the
  loaded program; their comments say whether the entry runs a loop, advances
  a frame or initializes resources. Same-role resident copies keep the
  entrypoint name; keep `main` and established runtime and library names.
  Ordinary exported helpers and effect callbacks keep behavior names.

### Statics, globals and locals
- A function used only inside its own unit is `static`, in no header; when
  the file uses it before its definition, a static prototype follows the
  includes. A static function drops the owner and starts with its verb
  (`read_pad`, `enter_free_cursor`), with no marker or `_cb`/`_func` suffix.
  A private body whose role another program declares in a shared header drops
  `static` and takes that declaration.
- File-scope `static` variables use `s_`; `g_` marks a variable other files or
  programs reach, through a C definition with a header declaration or an
  address label (`make static` converts one its file alone uses). Globals take
  owners like functions (`g_shop_`, `g_menu_stack_`). A name shared code binds
  to a different definition in each program says which programs define it.
- Locals and private types use their enclosing context.

### One role, one name
- One role takes one name in every program: bodies that differ by program
  share it, and every name is unique across programs except same-role bodies,
  which never share a link. Names carry no program token except entrypoints;
  the folder says which program a body belongs to. Keep enough qualification
  to tell roles apart and to avoid target-symbol collisions.
- Name an unknown by what the code shows it doing; mark thin evidence
  `Provisional:` in its comment; keep an address in a name only when nothing
  else is known. Use American spelling.

## Records and types

- A unit has three records. `party_unit_t` is the roster member the player
  keeps and saves. `battle_unit_t` is a unit in the current battle (computed
  stats, gear, statuses, CT, team, AI, current action), indexed by
  `battle_unit_id`. `map_unit_t` is its piece on the battle map (position,
  path, sprites, animation, facing), indexed by `map_unit_id`; an event ghost
  has a map unit and no battle unit. Comments may still say "unit" where the
  player would.
- Use snake_case; `_t` for struct typedefs and `_e` for enum typedefs (the tag
  repeats the typedef name without its suffix); uppercase enumerators. A size
  assert is named after its type (`unit_size_must_be_N`). Decimal for counts
  and screen or VRAM coordinates and sizes; hex for flags and encoded values;
  enum values in whichever base reads better (hex for id ranges and codes).
- Unknown members are `_unknown_XX`; `_unread_XX` means written by name but never
  read (by C, hand-written code or DMA); `_unused_XX` means no named access;
  `_padding_XX` means proven alignment or size filler. Offsets are relative to
  the member's own struct. Never access unused or padding members by name.
- Prefer `s8`/`u8`, `s16`/`u16`, `s32`/`u32` for explicit widths; PS1 SDK
  `long` values are `s32`/`u32` everywhere instead of pointer casts. Use `ONE`
  for fixed-point 1.0 (4096); a name that states a format uses `qN` for N
  fraction bits (`effect_lerp_q8`). Add `const` only for genuinely read-only
  inputs, verified under every caller's historical compiler profile.
- Materially different record layouts take distinct types; similar roles do
  not justify one conditional typedef. Prefer concrete records and named
  fields over raw views; keep genuine encoded data and alternate layouts, and
  never move a union's uncertainty into casts or byte arithmetic.

## Code

- Represent only what the original game did: no abstractions, helper layers
  or scaffolding it did not have. A macro, header or helper exists only when
  it expresses something the original contained.
- Prefer ordinary C, named fields, direct assignments and conventional loops.
  Remove redundant locals unless a named temporary reads better.
- Trace casts, shifts, masks and unions through readers and writers before
  changing types.
- Do not introduce register pins, asm barriers, `volatile`, stack padding or
  raw offsets to force a cleanup. Keep existing residue only when measured
  necessary, with a one-line reason; when the bytes force a repeated
  expression or an odd form, say so in one line. Keep architectural register
  access isolated.
- Keep comments factual: what a function or block does when the code does not
  make it obvious, and why a non-obvious step is there (a game rule, a retail
  quirk, a byte-matching constraint). Do not restate the code. Reference
  material validates our interfaces; never copy implementations or comments.

## Workflow

Use the Makefile for compilation, containers and verification. Inspect callers,
callees and layouts before editing; measure uncertain candidates in `build/`.
Use `make symbols` for renames and unit merges and `make config-fmt` after
target changes. Shared-source consolidation keeps every original address and
call binding, without runtime wrappers or alias machinery.

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
make symbols ACTION=rename-function ARGS="--pairs f.tsv [--module M] [--join]"  # renames (also rename-global, unit, rename-unit)
make static ARGS="[--dry-run] NAME"  # make a file-private global static (or --own src/x.c to define it there)
make units ARGS="--evidence=strong"  # unit map from binary evidence only
make order                       # where programs order shared functions differently
make naming                      # public functions whose owner is not their header
```

`MODULE` accepts module IDs, overlay aliases, lists, `event` or `effect`.
`make check` needs no disc; `make build` and `make diff` use `scus-94221.bin`.
Before handing off, run full `check`, `check-config`, `decl-check`,
`fmt-check`, `test` and `build`. Formatting, parameter widths, signedness and
qualifiers can affect the original bytes.

## Agents

- Split large work into disjoint files; one lead owns shared headers, target
  integration, final verification and the commits on the user's branches.
- Give each worker an exact base commit; a worker in its own git worktree
  checks `git log -1` first, since worktrees can start elsewhere. Workers keep
  results that must survive the worktree (patches, tables) on a worker branch
  or outside the worktree, and report affected entries, match evidence and
  proposed header changes.
