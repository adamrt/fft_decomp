# Final Fantasy Tactics decompilation

> Status: 100% decompiled and byte-exact ✅

A matching decompilation of the North American PlayStation release of *Final
Fantasy Tactics* (`SCUS-94221` [redump](http://redump.org/disc/55/)). Every game
function is C that compiles to the original bytes, and the rebuilt disc is a
byte-for-byte match.

Game files and proprietary Sony tools are not included.

After semantic changes and cleanup, `make check` will ensure the changes still
match byte-for-byte.

## Getting started

Requirements
- Docker or Podman
- `SCUS-94221` BIN at `./scus-94221.bin` (optional)

Then run:

```sh
# Prepare environment
make bootstrap

# Then use one of the following (make help for more):
make check  # verify all code against checksums (without BIN)
make build  # build every module and a byte-matching disc (needs the BIN)
```

`make build` writes `build/disc/output-scus-94221.bin` and `.cue`.
With Podman, pass `DOCKER=podman` to the same targets, for example
`make DOCKER=podman JOBS=6 check`.

## Finding your way around

`src/main`, `src/battle`, `src/world`, `src/wldcore` and `src/open` follow the
game's program boundaries. Feature folders group complete workflows inside
those programs. `src/event/<overlay>` holds the menu and scene overlays;
`src/effect` holds native effect callbacks, while their resident runtime is
in BATTLE.

`src/lib` contains shared game components: event scripts and dialog, menus,
threads, files, sound, memory cards and other common rules. Suzuki is the
linked sound engine under `src/lib/suzuki`. The original SDK and runtime
interfaces are separate under `src/platform/psyq`.

Game headers in `include/fft` describe concepts, and PS1 interfaces live in
`include/psx`. A source's folder describes its responsibility; `target/*.yaml`
records every program that compiles it, with the original addresses, compiler
profiles, data bindings and byte hashes. A shared source can therefore have
several independently checked program instances.

Files keep related state, private helpers and callbacks together. Reconstructed
translation units follow binary evidence; a large file or a repeated name
prefix alone does not justify splitting one.

### Program entrances

Start with `entry.c`, or `entry_<role>.c` when separate units own independent
entrances. An entry may run an entire loop, advance one frame or initialize
resources; the function comment identifies which. The entry files include
their original helpers and state.

| Program | Source | Entrance |
| --- | --- | --- |
| MAIN | [main/entry.c](src/main/entry.c) | `main` |
| BATTLE | [battle/entry.c](src/battle/entry.c) | `battle_entrypoint` |
| WLDCORE | [wldcore/entry.c](src/wldcore/entry.c) | `wldcore_entrypoint` |
| OPEN | [open/entry.c](src/open/entry.c) | `open_title_entrypoint`, `open_ending_entrypoint` |
| ATTACK | [attack/entry_deployment.c](src/event/attack/entry_deployment.c) | `attack_deployment_entrypoint` |
| ATTACK | [attack/entry_map_title.c](src/event/attack/entry_map_title.c) | `attack_map_title_entrypoint` |
| BUNIT | [bunit/entry.c](src/event/bunit/entry.c) | `bunit_entrypoint` |
| CARD | [card/entry.c](src/event/card/entry.c) | `card_entrypoint` |
| DEBUGCHR | [debugchr/entry.c](src/event/debugchr/entry.c) | `debugchr_editor_entrypoint`, `debugchr_mounted_unit_entrypoint` |
| EQUIP | [equip/entry.c](src/event/equip/entry.c) | `equip_entrypoint` |
| ETC | [etc/entry.c](src/event/etc/entry.c) | `etc_entrypoint` |
| HELPMENU | [lib/menu/help_overlay.c](src/lib/menu/help_overlay.c) | `helpmenu_battle_entrypoint`, `helpmenu_after_battle_entrypoint` |
| JOBSTTS | [jobstts/entry.c](src/event/jobstts/entry.c) | `jobstts_entrypoint` |
| OPTION | [option/entry.c](src/event/option/entry.c) | `option_entrypoint` |
| REQUIRE | [require/entry_conditions.c](src/event/require/entry_conditions.c) | `require_conditions_entrypoint` |
| REQUIRE | [require/entry_dark_screen.c](src/event/require/entry_dark_screen.c) | `require_dark_screen_entrypoint` |
| SMALL | [small/entry.c](src/event/small/entry.c) | `small_entrypoint` |

WORLD supplies callable screen services rather than one outer program loop;
start with [world/screen/runtime.c](src/world/screen/runtime.c), including
`screen_world_resources_init` and `screen_menu_frame_update`. It also contains
a resident `option_entrypoint` in [world/options.c](src/world/options.c) and
links the shared HELPMENU entrances.

EFFECT overlays expose behavior callbacks selected by the effect runtime;
their bindings are in [target/effect.yaml](target/effect.yaml). MAIN's CRT
entrance, `__SN_ENTRY_POINT`, remains in
[platform/psyq/crt](src/platform/psyq/crt/__SN_ENTRY_POINT.c).

### Names

Public names start with the owning game concept, then describe the operation.
Prefer `formation_roster_sort` when the roster is the stable topic. Preserve
natural operation phrases such as `unit_find_at_tile`,
`menu_stack_dispatch_resume` and `effect_vector_add_random_offsets`.
Drawing families may use `owner_draw_thing`; predicates use `is` or `has`
when they actually return a truth value. Consistent ownership matters more
than forcing every verb to the end.

`init` sets up state, `run` completes an operation or loop, `update` advances
one step, `start` schedules work, and `dispatch` selects a behavior.
Private names use the file's context, with enough qualification to keep
distinct roles clear. The full conventions and matching workflow are in
[AGENTS.md](AGENTS.md).

## Thanks

This project is an ode to the [FFHacktics](https://ffhacktics.com/wiki/)
community. ❤️
