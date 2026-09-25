# Target quirks

Non-obvious facts about the retail code. They explain source that looks wrong
and mark code that a cleanup must not "fix". Details live in the named file.

## Retail bugs the source reproduces

- `src/battle/battle_camera_store_state_to_script_variables.c`: all three zoom
  components are stored to script word `0x20`, so the first two are lost.
- `src/battle/battle_script_toggle_message_portrait_flip.c`,
  `src/world/world_script_toggle_message_portrait_flip.c`: the record index is never
  assigned; all six passes use whatever `$s0` held. Keep the local
  uninitialized.
- `src/world/world_menu_resize_parent_entry_to_digits.c`: calls
  `world_text_count_decimal_digits` with no argument; the callee reads a stale
  `$a0`.
- `src/wldcore/wldcore_proposition_determine_success.c`: the preferred-job
  check resolves the party unit once, before its loop, from the scoring loop's
  leftover (past-the-end) index, so it tests that one unit every pass.
- `src/battle/battle_move_animate_fall_to_target_tile.c`: the event-state
  (`0x34`) path writes map Y into `real_z` (not `real_y`); the height branch
  then overwrites it.
- `src/battle/battle_map_light_state_command.c`: several arms return an
  uninitialized pointer.
- `src/battle/battle_map_load_mesh_variant.c`: scales the mesh header's color
  palette byte offset by four, placing the palette read past most mesh files.
- `src/battle/battle_map_blend_ambient_light_color.c`,
  `src/battle/battle_map_blend_darkness_color.c`: modes `11` and above leave the
  target colour uninitialized (no default case).
- `src/battle/battle_ai_load_ability_entry.c`: the CT adjustments test the
  Monster Skill (`0x80`) and Defend (`0x40`) support bits; Short Charge and
  Non-Charge were presumably intended.
- `src/battle/battle_ai_evaluate_status_cancellation.c`: the Blood Suck branch
  tests decision bit `0x04`, which is built from Jump.
- `src/battle/battle_ai_build_monster_skill_tile_mask.c`: the `-1` neighbour
  offsets are loaded unsigned (+255), so only `+x`/`+y` survive, and the level
  loop runs for level 0 only.
- `src/battle/battle_ai_build_targetable_tile_mask.c`: radii 8–15 index past
  the 8-entry template table into the following status weights.
- `src/battle/battle_ai_evaluate_reflected_target_origins.c`: the resume entry
  skips the reflector pointer reload and depends on the caller's `$s2`.
- `src/battle/battle_ai_evaluate_math_targets.c`: the extra known-ability call
  in the special-ID loop discards its result.
- `src/battle/battle_camera_step_real_coords_toward_target.c`: in the positive
  direction the Y (`vz`) step adds the vector component twice; X and Z add it
  once.
- `world_menu_open_entry_window` receives requested x/y origins but always
  opens at its fixed default origin.
- `src/main/main_party_save_unit.c` calls `main_party_remove_unit` without its
  required roster index; the callee reads the current `$a0`.

## Calls that disagree with the callee

Some calls need an erased signature or a function-pointer cast to preserve
their register values. Others can declare ignored parameters and call directly
without changing the bytes.

- `g_battle_thread_call_target` is the main-stack dispatch slot for callees
  with different signatures; assignments erase their function types.
- `battle_target_set_panels_for_action` reads `$v0` after a void-returning
  panel builder; the value is the callee's leftover register contents.
- `src/event/equip_unit_load_selected_data.c` passes two arguments to
  `equip_unit_copy_data_to_compare_slot`, which takes none.
- `src/world/world_menu_resize_parent_entry_to_digits.c` passes none to
  `world_text_count_decimal_digits` (see above).
- `src/world/world_text_render_id_list_to_image_rows.c` passes a ninth stack
  argument that `world_text_render_ids_into_image` never reads. The BUNIT twin
  instead declares it as `unused_flags` in `include/fft/event_bunit.h`; unify the two.
- `src/battle/battle_move_animate_fall_to_target_tile.c` passes a coordinate
  buffer to the one-argument
  `battle_gfx_calculate_screen_z_from_misc_screen_data`, as it does to
  `_with_caller_data`; `battle_move_animate_jump_arc_to_own_tile.c` does not.

## Declaration leads

- `include/fft/menu.h`: `world_menu_entry_t.window_x` is `u16`, but
  `src/world/world_menu_open_entry_window.c` needs it signed.
- The unit status record (`battle_unit_status_record_t`, `include/fft/battle.h`)
  is redeclared per overlay: `status_panel_gauges_t` (ATTACK, REQUIRE), the head
  of `equip_unit_data_t`, `world_unit_status_billboard_t`, and HELPMENU's raw
  `g_helpmenu_active_banner`.
- `equip_gfx_context_t` (`include/fft/event_equip.h`), `jobstts_gfx_context_t`,
  `bunit_gfx_context_t` and `world_gfx_packet_buffer_t` share one 25-pointer
  pool layout (BUNIT and WORLD match through `0xec`); their pool names disagree.
- `g_wldcore_zodiac_start_dates[12][2]` (`include/fft/wldcore.h`) and OPEN's
  flat `g_open_birthday_zodiac_months[24]` are the same `{month, day}` table;
  the OPEN name covers both bytes, and `target/opening.yaml` still names `+1`
  `g_open_birthday_zodiac_days`.
- `battle_unit_misc_data_t.movement_value` (`+0x11c`, `include/fft/battle.h`)
  is a plain `u8` holding a packed step: direction in bits 6–7, layer in bit 5,
  length in bits 0–1 (`src/battle/battle_move_get_current_and_destination_tiles.c`).
- `g_world_gfx_full_texture_window` is a `RECT`, but
  `src/world/world_formation_build_view_primitives.c` reads its first 4 bytes
  as a screen point.
- `g_main_item_location_flags` (`0x80059414`): only the first 64 bytes (512
  Move-Find bits) are proven; the next 64-byte bank is saved with it but has no
  known meaning.
- `0x80165ef4` carries two names (`g_battle_text_substitution_values`,
  `g_dead_unit_roster_id`) because it holds several identifier kinds; keep
  its name generic.

## Duplicated code

Twins stay separate functions because each lives in its own module or
translation unit. Share their types and constants through headers.

- EFFECT: 32 groups of byte-identical routines across 110 native files. E005's
  first routine appears in 17 overlays, and E450/E480 are identical.
  E336/E461, E230/E481 and E015/E047 differ only in immediates.
- BATTLE/WORLD pairs: scenario conditionals
  (`battle_script_run_scenario_conditions.c` /
  `world_process_scenario_conditionals.c`), action-slot restrictions, the
  portrait-flip toggle (instruction `0x37`), staged status, selected-tile data,
  and the event interpreters.
- `src/wldcore/wldcore_menu_push_script_flag_01a4_detail_level.c` and
  `src/wldcore/wldcore_menu_push_story_event_text_level.c` are one routine at
  menu levels `0x2f` and `0x1e`; the second reserves 8 unused stack bytes.
- `battle_map_blend_ambient_light_color.c` and
  `battle_map_blend_darkness_color.c` are one 1,032-byte function over
  different records.
- `world_gfx_reset_record_texture_window{,_2,_3}.c` differ only in the `RECT`
  they install.
- Overlays share load addresses, so an address match is not an identity
  match. Compare bytes: REQUIRE and EQUIP both have code at `0x801c2dcc`, and
  the two routines differ.

## Other surprises

- JOBSTTS.OUT offset `0x0` and BUNIT.OUT offset `0x200` hold the literal `"%d"`
  (`g_jobstts_text_decimal_format`, `g_bunit_text_decimal_format`) used by `src/event/jobstts_menu_script_draw_formatted_number.c` and
  `src/event/bunit_cmd_draw_right_aligned_number_handler.c`.
- The game-options word packs its last five fields out of array order
  (`include/fft/menu.h`).
- CallFunction (`battle_script_execute_event.c`, `world_script_execute_event.c`)
  tests selectors in sequence against one operand that the arms modify. So
  `0x06` also runs the `0x0f` warp, and `0x0e` can fall into later arms.
- `src/battle/battle_action_dispatch_target_reaction_ability.c`: formula 7
  suppresses reactions. Counter Magic (`0x1b3`) is tested last, after byte
  `+0x8d`, so Counter (`0x1ba`) wins.
- `g_main_debug_display_enabled` is only ever cleared by retail code; the
  add-unit threads and unit-summary panels print debug output when it is set.
- `src/event/helpmenu_run_battle_help_menu.c`: an earlier close arm catches
  help mode 2, so its group is unreachable.
- `src/wldcore/wldcore_input_check_repeating_directional.c`: the arms test
  UP, DOWN, RIGHT, LEFT, R1, L1, but the counters sit in UP, DOWN, LEFT, RIGHT,
  R1, L1 address order. The early-out clears only the four direction counters.
- `src/wldcore/wldcore_map_roll_random_encounter.c`: encounter masks count from
  the high bit, so the chosen ENTD is `entds[7 - bit]`.
- `src/battle/battle_action_finalize_draw_out_katana_result.c`: when the
  katana breaks, the strike work's `can_earn_experience` is copied onto itself.
