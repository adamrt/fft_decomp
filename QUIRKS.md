# Target quirks

Non-obvious facts about the retail code. They explain source that looks wrong
and mark code that a cleanup must not "fix". Details live in the named file.

## Retail bugs the source reproduces

- `battle_unit_calculate_height_data`: the seven-byte result preserves an
  uninitialized elevation high bit; an unavailable unit leaves the other result
  fields unset. A non-Chocobo that shares its tile (a rider) takes its
  statuses, movement abilities and terrain from the other unit, so it is
  unavailable when its mount is Dead and never for its own Dead status.
- `DrawOTagEnv`: the diagnostic string uses literal `&08x` for its second item, so it does not format the environment pointer.
- `StCdInterrupt`: without `CdlModeSize1`, it skips the local disc-position read but still copies the uninitialized four bytes into the ring descriptor.
- `sprintf`: unchecked precision and zero padding can overrun its 512-byte temporary segment buffer.
- `FntPrint`: unsupported conversions leave the field length undefined; the count limit is checked after writing, and oversized hexadecimal widths can overrun the 512-byte temporary buffer.
- `DecDCTvlc`: the pause path saves its Y predictor at `0x80073f34`, the last word of `g_psyq_press_vlc_state`, which overlaps the first four bytes of OPEN's `g_calendar_month_lengths`. Nothing calls `DecDCTvlcSize`, so the limit keeps its initial 0xffffff halfwords and the pause path never runs.
- EFFECT native renderers: `advance_particle_track` sets phase 3 when a callback's keyframe window has two frames left, even if init never ran; the destroy path of `quad_grid.c`, `quad_grid_secondary.c` and `wave_mesh.c` then reads its latch through the null work slot (kernel RAM).
- `camera_store_state_to_script_variables` (BATTLE): all three zoom
  components are stored to 32-bit script variable `0x20`, so the first two are lost.
- `math_mul_div_s64` increments the dividend's low word when negating a negative
  divisor; a carry while negating a negative product increments the division
  remainder instead of the product's high word. For `(6264, -400920, -2359)`
  it returns 1065039 rather than signed division's 1064587. Its range flag
  only checks negative-result high words; `math_cross_product_q12` checks
  `high >> 12` for 0 or -1. Neither test covers every signed 32-bit overflow.
- `script_opcode_focus_speed` accumulates `rotation_sum` in an uninitialized
  `$s6`: the event dispatcher leaves `operand_3` there, while camera fusion
  leaves its key count. Those caller values affect the camera duration.
- `script_opcode_flip_message_portrait` (BATTLE and WORLD): `record_index` is
  never assigned; all six passes use whatever `$s0` held. Keep the local
  uninitialized.
- `proposition_determine_success`: the preferred-job
  check resolves the party unit once, before its loop, from the scoring loop's
  leftover (past-the-end) index, so it tests that one unit every pass.
- `map_calculate_surface_height`,
  `calculate_coords_height`: an off-map point
  (a unit staged outside the map; `map_unit_calculate_height_from_world_coords`
  loads its world coordinates unsigned) gets no `map_get_tile` tile, and the
  null tile is read from RAM address 0. The unit shadow, palette
  modulation and per-frame rotation code (`sprite_draw_shadow`,
  `sprite_build_shadow_quad`,
  `apply_unit_palette_modulation`,
  `sprite_project_units`) do the same for units
  that chapter-end cutscenes walk off the map.
- `distortion_fall_to_target`: the event-state
  (`BATTLE_STATE_EVENT`, 0x34) path writes the tile-center depth into `real.vy` (not `real.vz`).
  The first frame adds the fall speed to it, which puts the unit below the
  ground, and the next frame snaps it to the ground: an event fall ends on its
  second frame.
- `map_light_dispatch_command`: several arms return an
  uninitialized pointer.
- `unit_view_attack_caster_stats_thread`: for an item the
  secondary-data pointer stays uninitialized, yet its `flags_3` is read before
  the item test.
- `dispatch_gns_resource` passes the packed lighting colors
  (`map_light_resource_t`) as a `MATRIX` to `set_light_matrices`, which also
  copies its 32-bit translation values from the following 16-bit direction
  components, and the second matrix's uninitialized `direction.t`.
- `map_mesh_load_palette`: scales the mesh header's color palette byte
  offset by four, placing the palette read past most mesh files. Its wait step
  tests `file_get_load_state() != 0` (still reading), the reverse of the other
  loaders' completion tests: it applies the palette and frees the buffer while
  the read runs, or, once the read has finished, never leaves the wait. Its
  caller runs it only while `g_map_refresh_pending` is set, and nothing sets it.
- `map_queue_textured_triangles`, `map_queue_textured_quads`
  and their untextured twins pack SXY with the full signed 32-bit MAC1 value; a
  negative X therefore forces the 16-bit screen Y to `0xffff`.
- `blend_ambient_color`,
  `blend_darkness_color`: modes `11` and above leave the
  target color uninitialized (no default case).
- `ai_load_ability_entry`: the CT adjustments test the
  Monster Skill (`0x80`) and Defend (`0x40`) support bits; Short Charge and
  Non-Charge were presumably intended.
- `evaluate_status_cancellation`: the Blood Suck branch
  tests `AI_DECISION_TRANSIENT_JUMP` (0x04), which is built from the Jump
  status.
- `build_monster_skill_mask`: the `-1` neighbor
  offsets are loaded unsigned (+255), so only `+x`/`+y` survive, and the level
  loop runs for level 0 only.
- `build_range_diamond_mask`: radii 8–15 index past
  the 8-entry template table into the following status weights. Jump Level 8
  can reach this path with a non-arcing weapon.
- `evaluate_self_target`: suspension during the
  first origin's target search marks the second-origin stage, so resuming skips
  the moved-origin pass.
- `evaluate_reflected_origins`: the resume entry
  skips the reflector pointer reload and depends on the caller's `$s2`.
- `evaluate_math_targets`: the extra known-ability call
  in the special-ID loop discards its result.
- `step_unit_focus`: in the positive
  direction the map-depth (`vz`) step adds the vector component twice; `vx`
  and `vy` add it once.
- `menu_screen_open_entry_window` receives requested x/y origins but always
  opens at its fixed default origin (-112, -80).
- `build_next_result_display`: the CT-zero
  display case reads `y_shift` without setting it; only the HP/MP and
  stat-change cases assign it, so the sprite row depends on the leftover value.
- `sprite_update_queued_load` replaces sheet ids from 0xa0
  with Ramza's chapter 1 sheet, but the table has 159 sheets (0 to 0x9e), so
  id 0x9f reads one entry past it. `sprite_queue_unit_graphics_load` tests
  against the count correctly.
- `effect_secondary_spell_charge_handler` saves the inertia threshold
  and sets the secondary-effect value but never restores it, unlike the other
  secondary handlers.
- `battle_unit_generate_random_equipment`: the Shuriken and Ball item
  types (32, 33) index past the four `equipment_categories` bytes into the
  first innate status byte; that adjacent byte controls their eligibility.
- `menu_text_render_encoded_ids_to_image`: the encoded-text branch tests
  its glyph counter (`col`) against `max_chars` but never assigns it (only the
  0x2000 glyph branch leaves it at 8); the stale register stays below the
  width, so text never wraps.
- `init_map_unit`: tints the new unit's palette
  before it clears `mount_state`, so a map unit slot last held by a rider looks up
  a mount that is gone and reads its world coordinates through NULL (kernel RAM).
- `build_action_preview`: a skipped
  preview leaves `s_unit_view_preview_target_action` null, and its
  `attack_accuracy` is read (from kernel RAM) before the null test.
- The Formation screen's `rebuild_lists` (`src/world/formation/screen.c`):
  the Soldier Office (context 0x65) lists only monsters, yet it stages unit 0
  (`formation_grid_init`, then `formation_stage_selected_unit`) even when that
  list is empty; with no earlier list since WORLD loaded, the pointer is 0, so
  the "restore HP/MP" stores copy kernel RAM 0x10/0x16 to 0x0c/0x12.
- `ai_choose_wait_facing`: when the target shares the
  acting unit's tile, `ai_find_target_facing` returns
  `AI_DIRECTION_OVERLAP` (4), which indexes `viability.by_direction[4]`: the
  never-written `past_viable_directions` byte after the array holds whatever
  the stack held and decides whether 4 is returned as the facing.
- Both copies of `ability_list_scrolling_thread`: `SetSemiTrans(&frame, 1)` passes the
  address of the `frame` pointer, so the semi-transparency bit lands in a stack
  byte past the pointer and the sprite is unchanged.
- Unchecked divisions whose divisor can be zero; the R3000 `div` does not trap
  and leaves quotient -1 (1 for a negative dividend) and the dividend as the
  remainder: `100 / ct` in `ability_list_get_ct` (CT 0 displays -1 + 1 = 0);
  `0x20000 / effect_lerp_q8(...)` for the wave steps in
  `effect_wave_mesh_update` (E033, E035, E073, E079, E080, E230, E453, E456);
  and the direction-times-spread products over `scale` in
  `effect_slot_init_motion`.
- Locals read before any assignment, so the value is whatever the caller left
  in a register: `menu_icon_timed_thread` (BATTLE and WORLD) tests `unset`
  (`$s6`) and, when it is nonnegative, lowers `g_thread_current_id` around
  `menu_select_icon_cluts`, as `menu_icon_linked_entry_thread` does for its
  linked entry; `effect_update_file_load` tests `result` (`$a0`) for a
  load state outside 0-3, which nothing sets;
  `cursor_get_input_direction` tests `input` (`$a1`) when `source` or
  `g_cursor_input_mode` is outside 0-2; `status_set_action_state` leaves
  `status` unset for an action state other than 0 and 5-8 (its `switch` has no
  default; no caller passes one); `dispatch_gns_resource` returns a
  local no path assigns (`$s8`); `menu_get_cursor_bob_offset` (BATTLE and
  WORLD) walks an unset table pointer for a mode other than 0 or 1;
  `effect_secondary_summon_charge_handler` returns `result` (`$a1`)
  unset when the phase is not 1-3, as does
  `effect_secondary_linear_projectile_handler`;
  BUNIT's `get_grid_stat` and `get_grid_stat_max` and WORLD's
  `get_grid_stat` return `value` (`$a1`) for a stat mode outside 0-2; `start_walk` leaves `facing` (`$a2`, the
  destination tile its callers pass) unset for a direction outside 0-3;
  `load_mesh_animation_record` returns `next` (`$a1`) for record
  type 0; `update_set_menu` indexes the entry ids with
  `cursor` when the list is empty; `world_map_destination_list_thread` uses
  `selected` when no visible location is the current one;
  `helpmenu_battle_entrypoint` reads `node`, `x` and `y` in the cursor loop
  when the text thread is already running on the first frame, and `navigation`
  when the unit view mode is not 1-4; `command_dispatch_request` passes
  `action` on unset for SCENARIO_CHECK and CLOSE; `sprite_display_init_numeric`
  leaves `display` unset for a section outside 0-2 (its caller passes 0-2);
  `update_mesh_animations` indexes the instructions with
  `next`, assigned only when a keyframe expires in the same pass, for a part
  whose animation state is set and whose keyframe is 0;
  `map_light_dispatch_back_color_command` returns `result` unset for DISABLE
  and RESTORE (every caller issues SET); `memcard_info_retry`
  returns `result` unset for a retry limit below 1 (its caller passes 2);
  `poll_card_info_until_stable` has the same empty-loop result for a
  nonpositive retry limit (its callers pass 3);
  `select_sound_id` returns `sound` (`$a2`) for a channel mode
  above 4; `select_reporter` returns `selected` for
  a speaker mode outside 1-4 when more than one unit went;
  `stop_weather_sfx` returns an unset `result` no caller reads.
- `save_read_file` sets `result` to -1 when the last read was
  short and then returns `total`; the test has no effect.
- `helpmenu_after_battle_entrypoint`: the vertical-cursor branch reaches
  its shadow quad as `&quad[2]`, past the end of the local `cursor[2]`,
  relying on `shadow[2]` following it on the stack; one four-entry array does
  not reproduce the two stack addresses. `entry`, `x` and `y` are assigned
  only on frames where the text thread is idle and are unset if it is already
  running on the first frame.
- `render_decimal_value` assigns 1 on every rung
  of the minimum-width ladder, so the value is always placed as one digit, and
  it reads the digit count as the low nibble for alignment and as the low
  byte for the digit loop.
- `build_game_over_polygons`: `texture_mode` is the constant 1,
  so the 4-bit CLUT arm never runs.
- `init_mdec_state` stores the slice width (24) into the
  global `g_movie_mdec_stream_state.output_rect.w`, not through its
  `state` parameter; the one caller passes that global.
- `init_strike_data` tests
  `ability_id >= 0 || ability_id < ABILITY_ID_ITEM_FIRST`, which is always
  true.
- EQUIP never fills the physical accessory evade slot of `equipment_unit_preview_t`
  (0x48, `_unused_48`); its loadout copy stores the accessory's magic evade
  to `magical_accessory_evade` (0x52) twice, the first where the physical one
  belongs.
- Event script, both interpreters:
  - Ten instruction values (0x14, 0x20, 0x34-0x36, 0x52, 0x67, 0xe1, 0xe2,
    0xe4) have operand lengths in the size table and no handler; the
    scanners step over them and the interpreter ends the script on them. No
    retail script uses them.
  - In BATTLE, AddGhostUnit scans IDs 0..20 although the runtime allocator has
    16 slots. With all slots occupied, it queues absent ID 16; installation
    later fails without a retry. The immediate `continue` requires all 21 IDs
    to exist, which ordinary allocation cannot produce. The empty four-pass
    countdown before the search remains.
  - BlockStart, AddUnitStart or CameraFusionStart with no matching End gets
    offset 0 from the finder and the script resumes at byte 1.
  - FocusSpeed and RelativeCamera patch the next instruction's operands
    without checking that it is Camera; Focus scans for the next Camera with
    no End check.
  - UnitAnimRotate tests the animation id's high byte as its flag, never
    reads its last operand byte, and has no absent-unit check, so map unit ID
    2000 indexes the rotation table.
  - WaitUnitReady waits on `map_unit_is_ready`, so it never waits.
  - Effect stores its x and y operands to two globals nothing reads.
  - DebugPause prints "PAUSE", yields one frame and sets a 16-bit value nothing reads
    (`script_opcode_debug_pause`); instruction 0xdc stores 13 to
    another. Neither changes the script's course.
  - In WORLD, WalkTo is an empty arm outside a block, Arrow resolves both
    units and never sets the trajectory, and both ChangeMap instructions call
    an empty `map_light_reset_script_light` where BATTLE resets the script
    light transform. WORLD's interpreter never runs (see Other surprises).
  - FadeSound builds its request as `byte 2 << (byte 1 + 16)`: the low half,
    the target volume, is always 0, so it only fades the music out, over
    byte 2's frames doubled for each unit of byte 1.
- `update_location_menu` opens the outfitter (`run_location_shop`) with shop
  id 0 for every town; `shop_run_screen` then takes the id from the
  location variable. The fur shop and soldier office pass their own ids.
- OPEN's runtime flags `SYSTEM_RUNTIME_FLAG_RENDER_RECORDS_HIDDEN` (8),
  `SYSTEM_RUNTIME_FLAG_CLEAR_FRAME` (0x20000) and
  `SYSTEM_RUNTIME_FLAG_SOFT_RESET_DISABLED` (0x80000) are tested and never set.
- Suzuki driver: music status bits `SUZUKI_MUSIC_STATUS_NO_VOICE_ATTRS` (0x20)
  and `SUZUKI_MUSIC_STATUS_IMMEDIATE_INSTRUMENT` (0x2000), channel flag
  `SUZUKI_CHANNEL_NO_RETRIGGER` (0x10) and driver flag
  `SUZUKI_SOUND_DRIVER_PUT_TYPE` (0x2000; only `suzuki_sound_init`'s flags
  argument, which the game passes as 0) are tested and never set; channel flags
  `SUZUKI_CHANNEL_NOTE_READ` (0x80) and `SUZUKI_CHANNEL_STREAMS_SET` (0x4000)
  and voice-attribute bits 0x7000 are set and never tested;
  `suzuki_smd_set_flags_all_channels` and `suzuki_smd_dispatch_snapshot` have no
  caller.
- CARD save menu state `SAVE_MENU_LEAVE_ALT` (9) is compared and never stored.
  EQUIP refuses command row 1, so its screen state 3 is never entered.
  `g_fullscreen_graphics` entries 0, 5 and 6 repeat entry 1's CHAPTER1.BIN.
- `debug_battle_setup_thread` does `g_world_map_flags += 3`
  on a 32-bit flags value. World-map flags `WORLD_MAP_FLAG_SKIP_MENU_FRAME`
  (0x80), `WORLD_MAP_FLAG_UNLOCK_ALL` (0x400000) and
  `WORLD_MAP_FLAG_TITLE_BRAVE_STORY` (0x20000000) are tested and never set.
- WORLD's `thread_resolve_slot` searches slots 1-16 where BATTLE's searches
  1-15 (`VARIANT_THREAD_TABLE_SLOT_COUNT`); WORLD's slot 16 (tutorial text)
  lies beyond the sixteen the reset clears.
- `card_entrypoint` and `save_run_screen` end with
  `thread_set_parameters(THREAD_BORROWED_SLOT_CARD_BACKDROP, 0, -1, 0)`: the
  backdrop thread exits only when its third parameter is set, so that call
  does not stop it. CARD also tests and stops slot 14 and suspends slot 15,
  which nothing in the overlay starts.
- `update_path_level` plays sound id 0 (`SOUND_SFX_NONE`) when the path
  animation ends; that entry has no streams, so it only stops the channel
  pair.
- `check_condition`: conditions 18 and 19 (casualties) compare
  with the record's 16-bit `id`, not `value` as the war-funds pair does.
- `scenario_load_setup` fills deployment squad ids 0x35/0x36 and
  clears 0x37/0x38, although the deployment screen handles four squads.
- 32-bit words 0xab-0xff of the script variable block are reachable by no
  variable id (the packed nibbles end at word 0xaa), yet MAIN's
  `overlay_call_battle_return_zero` gates its BATTLE call on word 0xff, and
  it has no caller.
- Formation menu states `FORMATION_MENU_ABILITY_ENTER_SPRITE_SLIDE` (22) and
  `FORMATION_MENU_ITEM_ENTER_NO_SLIDE` (23) have entries in
  `g_formation_menu_handlers` and code paths, and no code stores them,
  so `formation_sprite_update_slide` never runs. `g_menu_input_mode`
  `MENU_INPUT_BLOCKED` (1) is tested in four places and never stored.
- `command_wait_thread` stores -2 over the
  Wait entry's row action (7, `COMMAND_MENU_WAIT_YES_NO`) before confirming,
  so the Yes/No entry of the menu table never opens. No code, row action or
  system function opens entries 0x09, 0x0d, 0x16, 0x17 (the only way to 0x18)
  or the four no-act turn menus (0x20, 0x21, 0x32, 0x33), yet the confirm-move
  test rejects Move for 0x21 and 0x33.
- `battle_unit_learn_job_abilities` sets the learned bit twice when the JP
  purchase succeeds: once in the purchase branch and again under
  `if (learned)`.
- `battle_unit_apply_level_growth` calls `rand()` and discards the result; the
  call only advances the generator.
- `sprite_evtchr_map_unit_load_attack_image` searches
  `g_sprite_evtchr_slots[0..1]` for a free load marker and decompresses into
  that slot's image data, but writes the owner, marker and state of the
  caller's slot.
- `after_battle_update_party` searches `g_party_message_identity_groups`
  for a joining unit's group, remaps no match (0x49) to 0 and group 0x47 to
  0x48, and never uses the result.
- `helpmenu_battle_entrypoint`: the
  `UNIT_VIEW_MODE_HOVERED_UNIT` arm and its navigation table cannot be reached
  (the first arm, which closes the help, takes modes 1 and 2); request
  kinds 4, 0x11, 0x12 and 0x17 call the close routine for an empty id and
  then start the text thread anyway, relying on close not returning.
- `heap_free`: the clearing loop stops at the first page byte that differs
  from the allocation id, not at the end of the 64-entry
  `s_heap_game_allocator_table`; an allocation that reaches the last page
  reads on into `g_gfx_draw_envs` and clears its bytes while they match.
- `map_update_load`: the four texture quarter files load at
  `buffer + 0x8000 * (n + 1)`, while `dispatch_gns_resource` uploads
  quarter n from `buffer + 0x8000 * n`; quarter 3 is loaded past the end of
  the 0x20000-byte buffer.
- `toggle_tilt`: from tilt target 3 it starts a decrease toward
  target 4, which `camera_update_tilt` has no branch to finish.
- `action_calculate_target_result`: both tests bound the default
  ability class at `ABILITY_ID_ITEM_FIRST + 1`, so the Potion ability indexes
  `g_ability_range_data` one record past its end, which is the bytes of
  `g_item_ability_item_ids`. The other callers bound it at
  `ABILITY_ID_ITEM_FIRST`.
- `reaction_prepare_next`: Counter Tackle performs Dash under the
  Black Magic skillset, the skillset Counter Magic passes.
- `formula_element_apply`: Oil on a fire-element action doubles XA
  after every caller has stored the HP damage, and nothing reads XA again: Oil
  is removed and the damage is unchanged. The weapon-element path
  (`formula_element_modify_weapon_damage`) does not test Oil at all.
- `proposition_determine_success`: the level score is
  read from the bravery scores (proposition data record 5, three rows of
  five) with a ten-entry stride, so it runs on into the faith scores (record
  6). Record 7, three rows of ten level scores, is fetched by nothing: with
  job list 1 a level 91-99 unit scores 5 where record 7 holds 50.
- `add_usable_abilities_from_skillset`: the Faith test that
  skips abilities for a unit with innate Innocent reads the AI workspace's
  considered ability (whichever was loaded last), not the ability being
  added. The bare Jump entry for a unit with no level-jump ability is written
  without being counted, so the caller's next entry overwrites it.
- `target_set_area_by_trajectory`: the weapon-range flag
  is read from `g_ability_range_data[(u8)ability_id]`, so abilities
  0x100 and up use the record of their id's low byte.
- `apply_level_change`: a level-1 target of a level-down
  (the Degenerator trap; formula 0x59 misses at level 1) is clamped back to
  level 1 and still loses raw / (growth + 1) of every raw stat.
- `suzuki_sound_set_reverb_mode`: a negative mode is meant to keep the current
  mode, but the stored mode still carries the 0x100 work-area flag that LIBSPU
  strips from its own copy only. It never equals the SPU's mode, so there is
  no early return, and the reloaded value is treated as a mode of 10 or more.
  The 100 music files all hold mode 4.
- `s_suzuki_smd_opcode_lengths`: the 37 unassigned opcodes have length 0, so
  the look-ahead of `suzuki_smd_read_instructions` would never step past one,
  and 0x9D has three parameter bytes there while `suzuki_smd_stop_sfx` consumes
  two. No retail sequence uses any of them.
- `suzuki_smd_set_instrument`: the waveset kept on the Suzuki heap is the
  0xb30-byte header of WAVESET.WD (176 instruments), and most music channels
  end with Instrument 0xFF before End Bar, so entry 255 is read 0x4f0 bytes
  past the copy.
- WORLD and EQUIP's copies of `item_list_sort`: the evade keys read
  `g_item_shield_data` for ids 0x7a-0x8f and `g_item_accessory_data` for
  0x90-0xef by `secondary_data_id`, so thrown weapons (122-127) read far past
  the 16 shield records, and headgear and body armor (0-63) read the 32
  accessory records and on past them.
- `shop_cash_out_excess_items` walks ids below `ITEM_ID_END - 1`, so
  Phoenix Downs (0xfd, the last item) over 99 are never cashed out; every
  other item walk runs to `ITEM_ID_END`.
- `calculate_path` copies 256 terrain costs from row 0 of
  `g_terrain_movement_cost_tables` (64 a row) to the scratchpad cost
  block at 0x1f800100, on over the spread state at 0x1f800180. Its flag-0x100
  test (a WalkTo with terrain costs disabled) compares the cost-table index,
  not the cost, with 0xff, so blocked surfaces cost 1 too.
  `init_pathfind_work` copies 64 and keeps 0xff.
- `formula_healing_staff` (formula 0x07, the Healing Staff) applies
  zodiac compatibility twice, once itself and once inside
  `formula_apply_physical_modifiers`, whose Defense
  Up and Protect steps also cut the healing to two thirds.
- `formula_please_eat` (formula 0x57,
  Please Eat) stores a full HP and MP heal (max-HP damage for an undead
  target) and then overwrites `attack_type` with the level change's:
  `action_apply_target_result` still applies the
  amounts, but `result_prepare_display` shows no number.
- `music_unload_scenario_slot` releases a slot's music record and SMD
  without clearing `g_music.state` when that slot is the playing
  one. The shop, formation and both save screens unload their own music this
  way, so the next `music_stop` hands the released record
  to `suzuki_smd_stop_music`.
- `map_palette_stop_clut_cycle` clears the active byte of slot `handle`,
  but `map_palette_start_clut_cycle` returns slot + 1: it stops the slot
  after the handle's own, and handle 8 clears the byte after the eight slots,
  the first of `g_cursor_tile_glow_cycled_palettes`.
- `sprite_queue_unit_graphics_load` writes its record before it
  tests the count, which stops at 16: a seventeenth append lands one past the
  16-entry queue, on the head of `g_zodiac_primitives`.
- `effect_secondary_remove`: removing a record from the middle of the
  active chain leaves the record before it with `next_slot_id` naming the freed
  record. When that one is removed while a newer effect is still active, it
  patches the freed record's link (by then a free-chain link) and its real
  successor keeps naming it as the previous record.
- `heap_alloc` tags a new block one above the highest tag met below it, so
  it can repeat the tag of the block directly above (free pages 0-3 under a
  block tagged 3: allocating 1, 1 and 2 pages gives tags 1, 2, 3).
  `heap_free` tells blocks apart by tag alone: freeing the lower block
  frees both, and freeing the upper one returns 0 and frees nothing.
  `heap_reserve_at` picks an unused tag but bounds-checks only the starting
  address, allowing the requested page range to extend beyond the heap.
- `set_encounter_equipment`: the random body-armor roll tests
  `UNIT_SUPPORT_SET_1_EQUIP_SHIELD` before asking for `ITEM_TYPE_ARMOR`;
  Equip Armor is tested only for the helmet.
- `clear_untargetable_lower_panels` (ability ranges) visits
  only the 256 lower-level panels, so an untargetable upper-level tile keeps
  its range; the weapon-range twin
  `clear_untargetable_tiles` visits all 512.
- `settle_on_tile`, `restore_facing_after_attack` and
  `map_unit_facing_turn_to_target`: for a mounted pair the second
  `battle_unit_set_tile_position` call names the unit's own battle record again
  (the first function finds its "mount" by the unit's own `map_unit_id`), so
  the partner's battle stats take no tile or facing here. The other two also
  write the partner record's facing with no null test.
- `sprite_evtchr_clear_attack_image_slot` tests owner bit 0x80
  (`SPRITE_EVTCHR_SLOT_OWNER_ATTACK_IMAGE`), which the EVTCHR (0xfe) and free
  (0xff) markers also carry, so the event instruction releases a saved EVTCHR
  slot too.
- `effect_secondary_reflect_handler` leaves the shield spin's Y angle unset.
  It clears X and Z, then writes the frame angle to Z again before the model
  renderer passes the vector to `RotMatrix`, which reads all three angles.
- `effect_model_draw`, the Reflect shield: the depth is
  assigned from each corner in turn where the other models sum them, so only
  the last corner counts, and every triangle is linked at ordering-table entry
  0 whatever its depth.
- `map_update_load`: the fourth selection pass does not exclude filler
  rows (type 0x31; 1,533 in the retail GNS files, all with variable 0x22, state
  1 and comparison equal). One whose comparison holds indexes entry 49 of the
  49-entry `g_map_loaded_gns_records` and
  `s_map_pending_gns_records`: it reads
  `g_map_saved_light_matrix` and can overwrite the first 20 bytes of
  `s_map_local_light_matrix`.
- All six copies of `zodiac_build_primitives` (MAIN, WORLD, ATTACK, CARD,
  DEBUGCHR, REQUIRE): the round-to-nearest step of the sprite scale adds 1,
  not `ONE`, to the 4.12 product before dividing, so it only rounds up a
  remainder of `ONE` - 1.
- `battle_state_open_sp2_files_update` frees its 0x8000-byte buffer when
  `sprite_request_sp2_read` returns 0, which means the read has started, and
  keeps it on 1 (loader busy). The next frame allocates again and has its
  second request refused, so the file arrives in the kept block only because
  the heap hands the freed pages back.
- `brave_story_update_treasure_detail` and
  `brave_story_update_unexplored_land_detail` wait, in their LOAD phase, on
  the render record named by the level's 32-bit `saved_record_index` (offset
  0x08). Their pushes never set it: it holds what the last level at that
  stack depth left.
- `start_unit_secondary` (fall dust, splash, level up, trap,
  zodiac poof, death smoke, teleport) builds its init record in a local of
  which `effect_secondary_build_self_context` sets five fields;
  `effect_secondary_init_animation` copies the rest of the two ten-byte blocks and
  `used_weapon_id` as the stack left them.
- `sprite_draw_shadow` sets bit 0 of `shadow_dirty` before testing it,
  so the shadow quad is recalculated on every call and the dirty bit the
  movement code sets gates nothing.
- `formula_holy_sword` (formula 0x2D, Holy Sword)
  runs Strengthen and the target's affinities on the weapon's elements only
  (`formula_element_apply_weapon_strengthen`,
  `formula_element_modify_weapon_damage`): the Holy element in the range data
  of the five Holy Sword abilities (0x9b-0x9f; 0x9e adds Lightning) is never
  applied.
- Half of MP: `skillset_build_ability_lists` lists the halved
  cost rounded up and `build_ability_list_rows` grays an ability with a nonzero
  listed CT whose listed cost exceeds the unit's MP, while
  `check_action_and_spend_mp` and the AI (`ai_load_ability_entry`,
  `add_usable_abilities_from_skillset`) round down. A unit holding exactly the
  rounded-down half sees an odd-cost ability grayed out although it could pay
  for it.
- `calendar_advance_days` (the scripted multi-day advance) disagrees
  with the daily `calendar_advance_day`: an open proposition that does
  not finish within the advance gains no elapsed days, a birthday reached only
  after the advance passes December 31 is missed, and script variable 0x67
  counts the year when the date passes December 31 instead of on March 21,
  with no cap at 99.
- `jobs_thread` reserves 0x1f000 bytes at the high overlay
  address (0x801df000), but the companion loader reads the 0x20000 bytes that
  `g_overlay_companion_executable_sizes` gives JOBSTTS.OUT, so the top two heap
  pages (0x801fe000-0x801ff000) are overwritten without being reserved.
  BATTLE's own `status_screen_jobs_thread` reserves 0x20000.
- `update_walk_to_edge_phase` adds the destination tile's
  water lift to the current edge height rather than to the destination's, so a
  step up onto deep water can pass the height test as a plain walk.
- `battle_state_change_map_jumping_in_update` sends event result 9
  (`SCRIPT_EVENT_FRAME_TO_WORLD`) to BATTLE and 10 (`_NEXT_EVENT`) to WORLD,
  the reverse of `battle_state_event_update`, and ignores results 11 (reset)
  and 19 (ending).
- `mark_unit_panels` is meant to mark the panels
  under allied or enemy units, but its loop over the 21 slots never advances
  the unit pointer, so it marks only the caster's own panel. None of the 368
  ability range records sets either flag that calls it.
- `build_usable_abilities` ORs the formula kinds of both hands'
  weapons and then switches on single values, so a unit holding two kinds (an
  HP-absorbing weapon beside a damaging one) gives its Attack entry no target
  team.
- `menu_screen_clamped_icon_list_thread`: Up steps the cursor up, but the Down
  test is inverted, so the cursor moves down on every frame Down is not held
  until it reaches `max_row_index`. Nothing refers to the thread.
- `menu_build_window_frame_image`: the banded upload (mode
  `MENU_WINDOW_FRAME_UPLOAD`, an image of 0x3000 bytes or more) starts a band
  when `(i & 0x5f) == 0`, at rows 0x20, 0x80 and 0xa0 rather than every 0x60
  rows, and its last-band test `i - height >= 0x60` is never true. After the
  band at row 0x20 the rows are written before the start of the buffer. No
  retail window is known to be that large.
- `battle_unit_generate_random_equipment` restarts its candidate list when an
  item with a higher required level appears, but items scanned after it with a
  lower level still join the list, so the pick is not limited to the best
  level reached.
- `status_queue_display_flag_change`: removing Crystal shares
  the Chicken case, so it queues the Chicken display bit for removal and leaves
  the Crystal bit set.
- `formula_apply_zodiac_compatibility` returns early when either unit is
  Serpentarius, skipping the floor of 1 at its end: an XA already cut to 0 (a
  Frog's XA of 1 against Defense Up or Protect) stays 0, and the hit deals 0
  damage where any other sign pair deals 1 * YA.
- DEBUGCHR's unit editor (`debugchr_editor_entrypoint`, started by
  `debug_unit_editor_thread`) is battle menu entry 44
  (`COMMAND_MENU_DEBUG_UNIT_EDITOR`), which no C caller or retail menu row
  selects; DEBUGCHR's reachable use is the mounted-unit Status screen
  (`debugchr_mounted_unit_entrypoint`), which keeps
  `g_debug_unit_editor_active` clear. The editor lets the job, the five
  equipment slots (Triangle empties one) and the five skillset and ability
  slots be changed, but `apply_values` writes back only HP, MP, CT, level,
  EXP, Brave, Faith, zodiac sign, Move, Speed and Jump, and
  `unit_panel_load_selected` reloads the unit after every change, so the
  other edits are lost at once.
- `novel_calculate_read_percent` (no caller) counts
  0x1000 bits from `g_novel_read_section_bits`, which holds 1,280 (40 words), so it
  also counts `g_item_location_flags` and the first 224 bytes of
  `g_item_poached_quantities`.
- `update_stream_frame` restarts a stream whose ring has fewer than
  16 free sectors through `start_cd_read`, which reads with
  mode 0x1a0; the first read in `movie_start_stream` uses 0x1e0, so the
  restart drops `CdlModeRT` (XA-ADPCM playback).
- `skillset_build_ability_list` bounds the support class with
  `<= ABILITY_ID_MOVEMENT_FIRST`, so Move+1 (486) is filed as a support ability;
  every caller asks for action abilities or for all four classes, so nothing
  sees the difference.
- Tests that cannot succeed: `init_map_unit` runs `run_block_seq` for the
  new unit's blocks only for a sheet id below `SPRITE_SHEET_ID_VELIUS` (60)
  and at least `SPRITE_SHEET_ID_BOY` (74); `proposition_advance_day` caps
  waiting days above 32, but the only new counts, stored by
  `proposition_update_result`, are 32 and 16; `battle_submit_frame` zeroes a
  `g_system_min_vsync_interval` of 1 under
  `wait < g_system_min_vsync_interval` with `wait` at least 2;
  `run_map_load` loops until `deploy_update_input` returns -1, and it always
  returns 1; the first `switch` of `apply_velocity` repeats the layer test
  that the statement before it has just made false;
  `turn_calculate_status_flags` cannot report Sleep, whose
  `g_status_effect_data` record carries the freeze-CT flag tested first;
  `animation_set_stand` tests a depth masked to a byte for `< 0` and a
  nonzero animation for 0; BATTLE's `menu_thread_get_activity` tests
  `i != 3` inside its scan of threads 4-9;
  `camera_rotate_for_unit_visibility` adjusts a negative `direction` after
  masking it to 0-0xfff; selector 15 of `map_set_tile_value` retries
  `tile_selector_15_stub`, which returns 0 at once, up to 1000 times before
  load exception 0x80. `run_unit_seq`, `run_block_seq` and
  `sprite_load_unit_frame` test sequence and frame pointers for -1, which the
  unpackers replace with the start of the data, and the last tests a
  three-bit part count plus one for `> 8`; `run_block_seq`'s own bounds
  (`sequence_id > 0x100`, `frame > 0x200`) admit the index one past each
  table. `select_autobattle_action` falls back from the recovery policy to
  retreat on a -2 result, which `select_protect_allies_action` never returns
  (only 0 or -1); `take_next_turn_event` has a case for turn event 0x400
  (`TURN_EVENT_UNKNOWN_0400`), which its only source, `turn_run_clock`, never
  returns; `menu_icon_selection_thread` clears both icon records'
  `bob_active` when one holds 0 and the other 2, but their only writer,
  `menu_build_window_sprites`, stores 0.
- `queue_unit_post_effect_messages` tests the 16-record
  capacity of `s_post_effect_messages` once, before it walks the
  unit's 11 reportable outcomes: a unit reached with 15 records queued can
  append ten more past the array, over `s_sprite_compressed_nibble_phase`
  and the globals after it.
- `evaluate_self_target` and
  `evaluate_reflected_origins` run
  `ai_origin_is_allowed` (deep water, Geomancy terrain, an
  adjacent Monster Skill holder) on the unit's current tile in both passes, so
  the starting tile decides the move-then-act pass, which casts from the
  destination; `ai_evaluate_act_move_orders` tests the destination there.
- `select_random_fire_tile` has no test for
  an empty candidate list, and `set_targeted_tiles` calls
  it for any result but -1, including 1 (no tile marked); it then targets the
  upper-level tile named by the stale `g_target_panels[0].mark`.
  Whether a retail action gets there is unknown.
- `free_script_instance` follows each freed work record's `next`,
  which `effect_particle_free_record` has just set to the old free-chain
  head. Only an empty list ends cleanly: with no free record the walk stops
  after the first and leaves the rest attached; otherwise it frees free-chain
  records again and then loops between two records forever.
- `map_light_dispatch_matrix_command`: the blend and delta steps (0x67, 0x69)
  start each translation from the start matrix's `m[i][3]` (the next row's
  first element, or the padding after `m[2][2]`) instead of `t[i]`. Nothing
  issues the start commands 0x66 and 0x68 that queue them, so they never run.
- `effect_camera_execute_position_command`, source
  `EFFECT_CAMERA_SOURCE_TARGET_AVERAGE`, sums the world coordinates of the
  unit targets only but divides by the whole target count, so tile targets
  pull the average toward the map origin, and no targets divides by zero. The
  average is stored only when an offset is given; every caller passes one.
- `map_unit_place_in_new_location` (`script_opcode_warp_unit` and
  `script_warp_unit_display_to_paired_unit`) passes `unit->facing`, the
  quadrant shifted left 10, as the `u8` facing of
  `battle_unit_set_tile_position`; its other callers divide by
  `UNIT_FACING_QUARTER_TURN`. The low byte is 0, so a warped unit's battle
  stats face south while its sprite faces as asked; facing evade reads the
  stats.
- `distortion_jump_takeoff` and `distortion_jump_rise`
  size the rise as `0x120 - ground y`, the altitude above world height 0 of a
  point 0x120 over the ground, but cover it from the unit's current height:
  a unit on ground h units up ends 0x120 + h above it. The second function's
  early exit treats the same value as an absolute altitude.
- `battle_state_confirm_action_update` stores answer 7
  (`TURN_COMMAND_PROMPT_YES`) with a unit on the cursor tile as the action's
  `targeting_type`, which otherwise holds `TARGET_TYPE_TILE` (5) or
  `TARGET_TYPE_UNIT` (6): readers that test for UNIT then aim at the stored
  tile, the two that test for TILE at the unit. The confirm prompts' command
  maps give only 5, 6 and 8; 7 comes from `command_resolve_selection`'s
  fallback for a menu outside its table, and whether that happens here is
  unknown.
- `calculate_path` (WalkTo): the destination filter repeats the
  water tests of `mark_move_destinations`, but refuses lava only
  when `move_type` is `MOVE_CLASS_FLY`, where that function refuses it unless
  `can_pass_lava` is set. The walker always uses `MOVE_CLASS_NORMAL`, so lava
  is never refused as a WalkTo destination.
- Both copies of `script_opcode_block_start_thread`: the block's WalkToAnim
  clears `g_unit_was_moving_latch[map_unit_id]` outside its absent-unit test,
  so an absent unit (2000) zeroes the halfword 4000 bytes on, the `w` of
  `g_menu_window_buffers[5].sprites[3]` in BATTLE and WORLD. The interpreters'
  own WalkToAnim keeps the store inside the test.
- `save_write_file` (CARD and WORLD) and
  `save_read_file` seek before each of their 40 attempts by the
  bytes transferred so far from the current position (origin 1), not from the
  start: after a short first transfer of n bytes, the next attempt works at
  file offset 2n with the data from offset n.
- `item_list_get_ranking_value` (EQUIP and WORLD): item 0 is a weapon to
  `item_list_get_category`, but the weapon branch excludes it, so it falls
  into the armor branch with index -144 and reads the range byte of weapon
  record 96 as its HP bonus. Both sort callers drop item 0 first.
- Every copy of `zodiac_build_primitives` (MAIN, WORLD, ATTACK, CARD,
  DEBUGCHR, REQUIRE) tags the backdrop `POLY_F4` but stores its corners at the
  `POLY_FT4` offsets, so the GPU takes the second and fourth corners from
  words at 0x0c and 0x14 that nothing here writes; only (0,0) and (255,0) are
  set. Its unlinked path, which builds every sprite into the backdrop's
  packet, never runs: no code stores `link_primitives`, and the CARD, WORLD
  and ATTACK contexts start at 1.
- `unit_list_build_index_map` maps a mount to its rider's list index
  (`unit_list_find_by_battle_unit_id`), not the rider's grid cell; they
  differ by the mounts sorted before the rider. Browsing onto such a mount
  with L1/R1 puts the grid cursor on a later unit's cell (or past the last),
  and ending the browse there selects that cell's unit.
- The right-aligned number instructions of the WORLD, BUNIT and EQUIP menu
  scripts (`menu_opcode_draw_right_aligned_number`) count their blank cells
  down to -1 in an `s16` with no lower bound: a value with more digits than
  the field starts below -1 and queues a blank quad about 65,535 times.
  Whether any retail field is too narrow is unknown.
- `fade_menu_update_in` does not clamp its level when the fade-in ends,
  unlike `fade_menu_update_out`. WORLD's copy starts at 240 and steps 16 per
  unit of event speed, so at speed 2 the level goes from 16 to -16 and the
  last subtractive tile takes the low byte, 240, darkening the screen for one
  frame; the CARD and BUNIT copies step 8 a unit and overshoot the same way
  at speed 4. Whether a fade-in runs at such a speed is unknown.
- `formation_get_party_unit_palette_index` treats every portrait index
  from 0x3f up as a monster's: the 28 story sprite sets that map to 0x78-0x93
  (Zalbag, Dycedarg, Wiegraf, the Lucavi and others) index the 20-entry
  `g_formation_monster_sprite_table` 37 to 64 bytes past its end. For example,
  Delita's Chapter 2/3 identity (`sprite_set` 5) selects portrait 120 and
  reads remap byte 57 in the following window data. Their normal-game
  reachability in Formation is unverified.
- `novel_opcode_unload_music` clears `tracks[slot]`,
  but `change_reading_music` and the audio queue keep slot n's
  track in `tracks[n - 1]`. The novel always uses slot 2, so the store hits
  `current_slot` (zeroed next) and `tracks[1]` keeps the unloaded id: a later
  queued play of that track switches to the empty slot without loading it.
- `display_object_draw_animated`: a part with its own palette gets
  CLUT x `(clut_x + (word & 0xff0000)) >> 12`, shifting the cel's CLUT x
  origin out with the palette field; OPEN's `draw_animated` (static in
  src/open/display_objects.c) adds the origin after the shift. All 141
  WLDCORE cels have CLUT x 0, so nothing shows.
- `find_ramza_slot` returns 16 (`PARTY_MEMBER_SLOT_COUNT`) when
  no party member slot holds Ramza, and the four MCBrave/MCFaith opcodes
  (`location_opcode_check_ramza_brave_at_least` through
  `location_opcode_check_ramza_faith_at_most`) use it unchecked, so
  they test roster slot 16, the first guest slot.
- `music_start_load_into_free_slot` allocates the SMD block before
  it requests the read; when `file_request_read_bytes` refuses (loader
  busy) it reports `SYSTEM_ERROR_SMD_LOAD` and returns 0 without freeing
  the block. `music_start_load_into_slot` (no caller) does the same.
- `menu_alloc_memory` checks the end of BATTLE's 53-page or WORLD's
  112-page arena before accepting a free run, so no positive allocation
  includes the last page. It marks pages before claiming one of 16 records;
  when all records are occupied, retries leave marked pages without an
  owner. Normal-game exhaustion of those records has not been established.

## Calls that disagree with the callee

Where callers pass arguments a callee never reads, the callee declares them as
`unused_` parameters and the calls are plain. The calls below cannot be
written that way.

- `g_thread_call_target` is the main-stack dispatch slot for callees
  with different signatures; assignments erase their function types.
- `menu_screen_fit_child_entry_to_digits` (WORLD): passes
  `text_count_decimal_digits` a local it never sets, so the callee counts
  the digits of a stale `$a0`. Nothing calls the function itself.
- `distortion_fall_to_target` passes a coordinate
  buffer to the one-argument
  `map_unit_calculate_height_from_world_coords`, as it does to
  `map_unit_calculate_height_with_caller_coords`;
  `distortion_jump_to_own_tile` does not.
- `s_suzuki_smd_modulator_waveforms` entries 8-15 call the void
  `suzuki_smd_modulator_deactivate`; `suzuki_smd_update_modulators` adds the
  leftover `$v0` (the cleared modulator flags) to the channel output.
- `music_update_tunes` passes `current_music` to `suzuki_smd_is_music_playing`
  even when no scenario music is loaded; the null record reads the status
  16-bit value at address `0x10`.
- `menu_window_animate_crop` and WORLD's twin `animate_box_crop` pass an
  uninitialized source (a stale `$s5`) to `menu_zoom_sprite`, which never
  reads it.
- `map_update_deployment_init_sequence` and
  `map_update_init_sequence` call `sound_stop_sfx` without its sound
  id: step 0 passes `map_id`, still in `$a0`; step 11 passes whatever
  `music_unload_current_scenario` left there.
- `map_unit_get_world_coords` returns NULL for a missing unit, but
  `map_unit_copy_world_coords`, `map_unit_copy_world_coords_static` and
  `map_unit_project_screen_position` test for -1 and the effect callers test
  nothing, so all of them read through it. A teleport-out with
  removal (Wiegraf at the Fovoham windmill) fades the unit out while the
  teleport particles still read its world coordinates, from kernel RAM at address 0.
- `script_process_status_commands` skips a battle unit id of 2000, which
  `map_unit_get_battle_unit_id` never returns: a map unit with
  no battle stats gives -1, `battle_unit_get` then returns
  NULL, and the Float and Jump tests read kernel RAM.
- `map_unit_get_tile_by_battle_unit_id` and `map_unit_get_tile` read the
  unit's map position without checking the lookup, so a unit with no runtime
  record (an arc-trajectory target in a Chapter 2 battle) is read from kernel
  RAM at address 0.
- `portrait_build_unit_poly` reads the speaker's battle record
  through `battle_unit_get`, which returns NULL for a
  speaker who is not on the field; the portrait test then reads kernel RAM.
  `fill_billboard` (BATTLE and DEBUGCHR) does the same while
  counting units, for a map unit record with no battle unit (the final battle).
- WORLD's `item_list_sort` counts each formation unit's five equipment ids into a
  256-byte local array, but the ids are 16-bit values and an item tried on in the
  shop's fitting room carries flag bits above the low byte, so sorting the
  list then increments a byte far past the array.
- `script_resolve_map_unit_id`: for the active-turn selector 0x69 with no unit
  holding the turn, it falls back to ENTD units 1 and 2 but tests the finder's
  battle unit id, not the loop counter, against 3, so a unit found in battle slot 3
  counts as missing; with neither on the field the Jump test reads the
  finder's record (NULL when there is none: kernel RAM at 0x58), and if it
  passes the lookup goes on with ENTD unit 3. In the Ramza game-over event
  (0x194) with no Ramza in the 21 records it returns -1, not
  `SCRIPT_MAP_UNIT_ID_NONE`, which is what its callers test for.
- `init_map_unit`: stores the spritesheet-slot
  claim (0xffff when all nine slots are taken) in the u8
  `spritesheet_vram_slot` and tests the stored 0xff against 0xffff, so the
  failure path never runs and a tenth sheet loads into slot 255.
- `ai_load_ability_entry`: a Jump ability's CT estimate
  for the acting unit reads `acting_unit`, which is NULL during the AI
  workspace setup at battle start (`acting_unit_id` is 0), so slot 0's Jump
  divides kernel-RAM bytes.
- `bunit_entrypoint` passes a second argument, 0, to the
  one-parameter `unit_grid_draw_background`; its other caller passes
  one.
- `command_dispatch_request` returns `$v0` as the void
  `announcement_select` left it.
- `menu_screen_text_bind_section` has no return statement;
  `build_slot_description` uses the address its last store
  left in `$v0`.
- `effect_emitter_spawn_particles` has an `s32` result it never sets
  and no caller reads.
- All 18 `s_distortion_handlers` (`distortion_none` through
  `distortion_jump_rise`) are `s32` and return nothing; the animation
  dispatcher ignores their results.
- `thread_resolve_slot` and `thread_resolve_next_slot` (BATTLE and WORLD) fall
  off their end after `thread_current_exit()`.
- `ai_calculate_unit_death_counter_delay` is defined
  with three parameters and both callers pass one; the other two are scratch
  it assigns before reading.
- `map_calculate_surface_height` masks its terrain layer to a byte; two callers
  pass the 16-bit `action_command.target_position.vy` unmasked.
- `start_next_post_move_event` passes x/layer/y to `ai_store_triggered_trap_coords`,
  whose byte stores expect x/y/layer. The argument-order disagreement is preserved.
- WLDCORE passes its 8-byte records by value: `RECT` to
  `window_build_render_record_image`, `window_init_text_record`,
  `display_object_init_image` and `get_image_vram_position`, and
  `point32_t` to `window_build_yes_no_panel` and as the CLUT of
  `display_object_init_image`.
- `filter_unit_id` (BATTLE and WORLD) reads `in_id` as a 16-bit value; most
  callers pass the address of an `int` loop counter through a `(u16*)` cast,
  so the id is that counter's low half.
- `start_gns_load` falls off its end for a map with no
  GNS sector and returns what `bzero` left in `$v0`. `map_update_load`
  compares the result with the GNS table and, when it is not the table (also
  when the read could not start), returns 0 with its 0x20000-byte load buffer
  still allocated.
- `g_effect_secondary_handlers` entries 0x07, 0x0a, 0x0b, 0x0e and 0x10
  set no result, although `effect_secondary_update`
  ends an effect on a handler result of 0. No record of
  `g_effect_secondary_animations` names them.
- JOBSTTS's `ability_list_is_non_action_enabled` passes one 32-bit pointer slot (0x801f0050, named
  both `g_ability_list_selected_ability_data` and
  `g_ability_list_selected_ability_secondary_data`) for both outputs of
  `ability_get_records`, so the ability data pointer is
  overwritten by the secondary data pointer. The ability-list callbacks never read
  the primary pointer: MP and CT use the surviving secondary pointer, and JP
  reads `g_ability_data` directly.
- CARD's `menu_input_update_event_state` passes six 32-bit values starting at
  `g_input_event` to `script_load_event_input_state`; CARD's controller pointer
  at +4 is copied as held buttons, and the remaining 32-bit input values are
  shifted by one field.
- `menu_screen_update_dialog_input` and `script_update_event_frame` pass six 32-bit values
  starting at `g_script_event_input` to the event input loader. The
  next five 32-bit values include unrelated status-label data; the corresponding held,
  previous and repeat globals reside elsewhere.
- `skillset_collect_monster_abilities` returns 0 from its early exits
  but falls off its end after building a list, and writes no terminator after
  it; its one caller, `build_ability_list_rows`, ignores the result and has
  filled the id column with 0xffff beforehand.
- `effect_get_unit_world_coords` passes the unit's map unit id to
  `effect_resolve_target_world_coords`, which takes an index into the
  effect's target list. Nothing calls it.

## Declaration leads

- `g_suzuki_sound_driver_flags` (`0x80032a54`): `spu_transfer_wait.c`
  declares it `volatile`, while `driver.c`, `sfx.c` and `smd_voices.c` declare
  it plain. Uniform qualification and local volatile-pointer rewrites change
  the original code, so each file keeps its own declaration.
- The heads of `equipment_unit_preview_t` and `unit_list_record_t` overlap
  `unit_panel_summary_t`, but their fields at 0x1e and 0x20 differ.
- BUNIT, EQUIP and WORLD use the 236-byte `menu_gfx_context_t`. JOBSTTS's
  `status_screen_gfx_context_t` and CARD's `save_gfx_context_t` omit the
  draw-mode pool pointer at 0x60 and are 244 bytes; CARD's holds one `DR_MODE`
  inline at 0xd0.
- WLDCORE's `g_calendar_zodiac_start_dates` uses `calendar_date_t`
  records; OPEN's `s_birthday_zodiac_start_dates[24]` stores the same
  `{month, day}` pairs as bytes.
- `g_item_location_flags` (`0x80059414`): only the first 64 bytes (512
  Move-Find bits) are proven; the next 64-byte block is saved with it but has no
  known meaning.
- Alias labels: the target binds a second name at the address of an object
  that already has one, or inside it, so that a function reloads the value
  instead of keeping it in a register. The byte-exact build needs both names;
  a port must map every alias onto its owner, because two definitions would
  silently stop sharing the storage. Same address, two names: `g_move_find_result_flags`
  over the static `s_tile_pickup_result` (`0x8018f8a8`, BATTLE),
  `g_action_work` and `s_action_work_view` (`0x801938c0`, BATTLE),
  `g_novel_countdown_frames` and
  `g_novel_countdown_frames_reset` (`0x800bb4f8`, WLDCORE),
  `g_unit_list_help_nodes` and `g_unit_list_help_nodes_reload` (`0x801ec900`,
  BUNIT), `g_unit_list_help_node_index` with its `_reload` and `_reload_2`
  twins (`0x801ec904`, BUNIT), `g_ability_list_selected_ability_data` and
  `g_ability_list_selected_ability_secondary_data` (`0x801f0050`, JOBSTTS),
  `g_unit_panel_status_display_origin_x` over
  `g_unit_panel_status_display_thread_params` (`0x801d007c`, REQUIRE),
  `g_unit_panel_numbers_origin_x` over `g_unit_panel_numbers_thread_params`
  (`0x801d0090`, REQUIRE), `g_unit_panel_character_status_origin_x` over
  `g_unit_panel_character_status_frame_config` (`0x801d00a4`, REQUIRE), and
  `g_gfx_poly_gt4_flat_pool` over `g_gfx_poly_gt4_buffers` (`0x8004c6c8`, MAIN).
- Alias labels inside another object: `g_controller_selected_options` is
  `g_controller_records` + 0x1c, read with a hand-computed 25-word stride
  (OPEN); `g_formation_sort_order` is the `formation`
  member of `g_menu_sort_orders` (+12, MAIN); `s_world_map_fade_color` is
  `g_world_map_fade` + 36, `g_world_map_pan_target_x` is
  `g_world_map_pan` + 28, `g_novel_state` (a biased view from the
  instruction on), `g_novel_counter_delta` and `g_novel_history_0`, `_1`, `_2`
  are `g_novel_reading` + 8, + 18, + 32, + 34 and + 36, and
  `g_window_panel_scroll_offset` is `g_window_panel_render_state` + 0x3a
  (WLDCORE); `g_status_screen_numbers_panel_records` and `g_status_screen_frames`
  are `s_unit_view_stats_panel_pages` + 1496 and + 3740 (BATTLE and WORLD); `g_sprite_sheet_ids_by_vram_slot` is byte 1 of
  `g_sprite_sheet_slots[0]` (BATTLE); `g_war_trophy_item_count` is byte 21 of
  the static `s_war_trophy_search_result` (REQUIRE); `g_unit_editor_hp_max` is
  `g_unit_editor_values` + 4 (DEBUGCHR).
- Overlaps with no second label: the sprite primitive ring is
  `g_map_unit_slots + MAP_UNIT_SLOT_COUNT`, 10,240 bytes past the
  declared array that no label names (`0x800bb704`, BATTLE);
  `build_range_diamond_mask` reads past the eight rows of
  `s_origin_range_row_masks` for radii 8 to 15, into the following static
  `s_status_priority_weights` (`src/battle/ai/evaluation.c`);
  `g_system_work_buffer_0` is an unsized `u8[]` that OPEN views as `u32*`,
  `s32*` and `u16*`.

## Duplicated code

Shared sources compile at each original entrypoint with its own module
bindings. Distinct entrypoints and genuinely different implementations remain.

- `open_fact_detail` and
  `brave_story_open_event_detail` perform the same operation at menu
  levels `0x2f` and `0x1e`; the latter reserves 8 unused stack bytes.
- `map_light_start_ambient_blend` and `map_light_start_darkness_blend` blend
  different records (`blend_ambient_color`, `blend_darkness_color`). WORLD's
  four entries compiled from `menu_init_scaled_draw_area_packets` bind
  `g_menu_draw_area_template` to different second rectangles. Similar bodies
  alone do not make these bindings interchangeable.
- Some routines are compiled twice in one module, and the second entry has no
  caller: `script_run_next_event_unused` repeats `script_event_thread` just
  after it (BATTLE and WORLD), and `map_unit_add_event_offset_unused` repeats
  `map_unit_add_event_offset` just before it (BATTLE).
- Overlays call both the resident `thread_is_running` and local
  `menu_thread_is_running` entries. Keep those callees distinct even though
  their running-flag reads are identical.
- Overlays share load addresses, so an address match is not an identity
  match. Compare bytes: REQUIRE and EQUIP both have code at `0x801c2dcc`, and
  the two routines differ.
- ATTACK's and REQUIRE's `unit_panel_numbers_thread` points both slot branches
  at `g_unit_panel_editor_numeric_state`, where the other programs' copies
  give the preview slot `g_unit_panel_editor_numeric_state_b`. Both overlays start it
  only in `THREAD_SCREEN_SLOT_NUMERIC_PANEL`, so the shared branch never runs.

## Other surprises

- `save_image_restore` skips a saved Unit List order if any of its first 11 keys is 11, and skips a saved Formation order if any of its first seven keys is 11. It preserves the live order, writes its final -1 terminator, and continues loading the save. Formation key 11 would read the fixed 100 CT maximum; the Order Unit menu exposes only keys 0, 1, 2, 4, 5, 7 and 9.
- `weather_dispatch_command` selects precipitation from the raw script weather and snow flag. The rain and snow drawing paths do not test `WEATHER_FLAG_IGNORE_WEATHER`: that indoor flag suppresses elemental and movement weather rules through `weather_get_effective`, but does not itself suppress the particles.
- `start_next_post_move_event` handles `MOVE_POST_EVENT_SOURCE_DISPLAY` (0x200), and `battle_state_post_move_events_update` clears it, but the normal producer chain through `move_resolve_post_events` and `move_get_ability_events` never sets it.
- `animation_set_status` retains two branches selecting animation 0x21 when bit 0 of `status_flags_1_4` is set. Normal status updates never set that bit. Crystal's mask 1 belongs to the separate `status_flags_5_6` word; this does not rule out animation requests from other paths.
- `gfx_load_debug_font`: patches the low immediate byte of `FntLoad` at `0x80022f5c`, changing its font CLUT placement from `y + 128` to `y + 127`.
- `StCdInterrupt`: tracks frame numbers through the header's low `u16`, while DMA completion and OPEN retain the full `u32` frame number.
- `psyq_gte_apply_rotation_ir`: writes saturated IR1–IR3 32-bit output values and returns its unchanged third argument.
- `sprintf`: unsupported conversions terminate output, `%#p` prefixes `0p`, hexadecimal alternate form prefixes zero, and `%#.0o` with zero emits no digit.
- `g_menu_text_decimal_format`, the `"%d"` that the menu number opcodes
  (`menu_opcode_draw_formatted_number`, `menu_opcode_draw_right_aligned_number`)
  pass to `sprintf`, sits just before each overlay's first function: offset
  `0x0` of JOBSTTS.OUT and EQUIP.OUT, `0x16c` of CARD.OUT and `0x200` of
  BUNIT.OUT.
- The 32-bit game-options value (`game_options_t`) packs its last five fields
  (`sound_mode` first) out of the Options menu order (`game_option_index_e`).
- CallFunction (both copies of `script_execute_event`)
  tests selectors in sequence against one operand that the arms modify. So
  `0x06` also runs the `0x0f` warp, and `0x0e` can fall into later arms.
- WORLD's copies of some event-library routines call BATTLE-only functions
  at their BATTLE addresses, which hold unrelated code or data while WORLD and
  WLDCORE are loaded: `script_opcode_map_darkness_thread` calls `map_light_start_ambient_blend`
  (`0x800934d0`, WLDCORE data), `apply_staged_status` runs
  `status_queue_display_flag_change` on the main stack (`0x80068e80`, 0x10 bytes into
  `world_map_update_scroll_axis`) and `battle_unit_status_staging_save`
  runs `status_apply_unit_action_removal` (`0x8018ec44`, WORLD text data).
  `battle_unit_get_id_by_event_unit_id` calls
  `map_unit_get_battle_unit_id` (`0x8008cdd0`), which in WLDCORE
  is the epilogue of `world_map_init_dots`: it would reload
  `s0`..`s4` from the caller's frame and pop 48 bytes of its stack. The other
  unit-core helpers the shared event code calls (`map_unit_exists`,
  `map_unit_get_id_by_battle_unit_id`,
  `battle_unit_get`,
  `battle_unit_find_by_unit_id`,
  `sprite_start_unit_graphics_load`) bind the same way, into the
  middle of WLDCORE's map-dot routines or WORLD's menu data.
  `map_unit_set_event_animation` likewise reaches three BATTLE-only animation setters,
  behind script-variable tests WORLD scenes would have to pass first.
  `camera_wait_idle`, `command_mini_menu_thread` and
  `help_menu_start_thread` call `camera_is_active` (`0x8008e17c`,
  0x1e4 bytes into WLDCORE's `find_path`), and
  `menu_update_event_frame` calls `system_get_vsync_interval`
  (`0x8008ded8`, 0x2dc bytes into `world_map_draw_path_animation`).
- OPTION.OUT carries its own copy of `menu_init_scaled_draw_area_packets`
  (`0x801c02d0`) but its window and option threads call BATTLE's copy
  (`0x80138adc`); target overrides keep that binding.
- `dispatch_targeted_reactions`: formula 7 (Weapon Heal) suppresses
  reactions. Counter Magic (`0x1b3`, reaction byte 1) is tested last, after
  the byte-2 reactions at `+0x8d`, so Absorb Used MP and Counter (`0x1ba`)
  win over it.
- `g_debug_display_enabled` is only ever cleared by retail code; the
  add-unit threads and unit-summary panels print debug output when it is set.
- WLDCORE's `input_update_repeat`: the arms test
  UP, DOWN, RIGHT, LEFT, R1, L1, but the counters sit in UP, DOWN, LEFT, RIGHT,
  R1, L1 address order. The early-out clears only the four direction counters.
- `roll_encounter`: encounter masks count from
  the high bit, so the chosen ENTD is `entd_ids[7 - bit]`.
- `finalize_draw_out_katana`: when the
  katana breaks, the strike work's `can_earn_experience` is copied onto itself.
- `memmove`: the overlap-safe copy returns the original destination on its backward path and the advanced destination on its forward path.
- `StartRCnt` and `StopRCnt` index the IRQ-mask table before validating a counter: selector 3 changes the VBlank mask even though `StartRCnt` returns zero; `StopRCnt` always returns one. Larger indices can read past the four-entry table.
- `SpuGetVoiceEnvelopeAttr`: the public `s32*` key-status output is written with a 16-bit store; its upper half remains unchanged.
- `StartCARD` and `StopCARD` leave their declared signed 32-bit results unspecified.
- `_patch_card2`: exchanges five resident/BIOS 32-bit instructions, so calling it again reverses the exchange rather than repeating an idempotent patch.
- `SetDrawLoad`: initializes the upload header and rectangle but leaves pixel payload storage to its caller.
- `suzuki_sound_get_largest_free_block`: reports the rounded gap including space needed for a new block header, rather than a directly usable payload size.
- `load_vram_sheet`: its fixed 0x30d4-byte copy from the variable compressed SPR tail at 0x9200 extends beyond the file-sized allocation in 153 of the 154 nonempty unit sprite-table entries and includes retained heap bytes.
- `update_animation`: when neither EVTCHR slot is free, one path tests `j` before setting it.
- `s_disc_extents` (`0x801b53e8`) stores whole-sector sizes, and entry 0 repeats entry 1: effect id 0 loads E001.BIN, so E000.BIN's code is never loaded through the table.
- `effect_noop`: the native callback returns without setting a result; opcode 07 ignores that result.
- E464's `effect_altima_transformation_map_vertex_particles_update` accepts
  vertex groups 1-12 but indexes a three-entry location table; all 16 shipped emitters use group 0, so the
  out-of-range path is inactive.
- `EFFECT/E223.BIN` through `E225.BIN` contain effect data but have zero resident load sizes; requesting those file IDs loads Cure instead. The ability-effect mapping does not reference them.
- `load_portraits`: the WLDFACE rectangles (`g_menu_screen_wldface_vram_rects`) use x = 1664–1856; the GPU masks VRAM transfer coordinates, so the pages land at x = 640–832.
- `suzuki_sound_init`: `suzuki_sound_init_sfx_music` reaches `suzuki_smd_insert_music`, which disables and re-enables `g_suzuki_sound_root_counter_2_event` before the initializer opens that event: handle 0 at boot, the handle `suzuki_sound_quit` closed on a soft reset.
- EFFECT scripts never run seven state handlers: E000's script calls only `effect_noop` (opcode 07) and E314's names no file function, so native function ids 1 and 90 are bound by nothing; E134/E242 `effect_ring_stack_mesh_update`, E454/E455 `effect_set_map_3d_objects_state_2` and E456 `effect_freeze_map` sit in callback slots (script opcode 06) that no keyframe selector fires. E457.BIN, which has no code, binds native function 88 (0x801c2578, E259's `effect_dome_mesh_update`; data in its own image) to slot 0, and none of its keyframes names a slot.
- `map_draw`: when map command 0x96 (E458) ends, `weather_dispatch_command(0x8b)` can restore rain or storm (not snow) mid-draw; the rain block (0x55) then re-links the `ft3` drops that the 0x96 block already linked at OT+2, an ordering-table cycle that holds `DrawSync` until its 240-VBlank timeout.
- `save_run_screen`: its 0xCCD8-byte stack frame, containing packet pools and directory entries, reaches below 0x801ff000 into the top cells of MAIN's game heap (0x801df000-0x801ff000); it is safe only while those cells are free.
- `tutorial_opcode_end`: two paths fall off the end of the s32 handler and return a stale nonzero `$v0` (the fade's result, or the 1 just stored), so the formation loop keeps running.
- `unit_panel_draw_backdrops` in WORLD and BUNIT (the ATTACK, DEBUGCHR and REQUIRE
  copies, and EQUIP's `unit_panel_draw_backdrops`, have no caller): while a
  unit-status banner thread (slot 8 or 7) is being stopped its
  `function_parameter_1` is 0, and the panel check reads that display record
  at RAM address 0.
- `ending_draw_script`: passes OT entries by value (`otag[15]`, `otag[8]`) to `AddPrim`; after `ClearOTagR` each holds the physical address of the entry below it, so the credits fade, bands and glyphs link into layers 14 and 7 through the KUSEG mirror.
- `update_picture_dissolve`: the step-0 call (start the dissolve) has no return value of its own; `$v0` keeps its clearing cursor, the address one byte below the band-progress table. Initialization callers ignore that result.
- WORLD's `menu_build_window_sprites`: for the Bar's send-unit list (header 0x10, `MENU_ICON_HEADER_MENU_WITH_ARROW`) the arrow x reads the `source.h` of the layout record at the entry's `context` pointer, `&g_window_panel_value` + 6, which is the upper half of `g_proposition_dispatch_days` (0).
- `generate_treasure_item`: outside action execution (AI simulation, preview) it returns with no value, so `$v0` still holds `g_action_state` and the crystal pickup result names item 1 or 2 as the treasure.
- `update_new_game`: no retail code sets step 5, which pops New Game and opens the Music Test (`music_test_open_menu`, controller slots 7 and 8); only a poke of the 32-bit step value reaches it.
- WORLD's copy of the battle menus is dead code: nothing in any module calls or names `command_dispatch_request` (0x800f5230) or `command_start_mini_menu` (0x800f0e48), so modes 1 (AT list) and 2 (dead-unit panel) of `option_entrypoint` never run; the world map's Options row runs mode 0. With them goes WORLD's event interpreter: `script_start_current_event_thread`, the only starter of the event thread, is called by `command_dispatch_request` and by the menu-building thread that only BATTLE's `command_open_menu` starts (WORLD's copy is empty), so WORLD's `script_execute_event` never runs.
- Abilities no skillset lists: 0x16f (Attack), Reflect (0x1b8), Non-charge,
  Teleport 2, Move Underwater and Silent Walk. Four ids with no name text are
  played as Altima's teleport effects (0x28, 0xb8, 0xdb, 0xdc), and 0x200 is
  the follow-up strike that resolves a knockback.
- Skillset rows 0x86-0x8d, 0xa7 and 0xa8 have no name text but hold
  abilities; 0xa7 is the row the AI uses for a unit under Blood Suck, and
  0xa9, which it pairs with ability 0x16f, is empty.
- The ability names "Move in Water" (0x1f6) and "Walk on Water" (0x1f7) are
  the reverse of what the abilities do: 0x1f6 keeps the unit on the surface
  and 0x1f7 one level below it (`get_effective_height`,
  `init_pathfind_work`).
- Ability 457, named "Equip Knife", sets the Equip Katana support bit
  (`UNIT_SUPPORT_SET_1_EQUIP_KATANA`): the unit can equip katanas, not
  knives.
- Selling in the fur shop adds the sold copies to the poached stock
  (`shop_fur_sell_update_confirm`), so they can be bought back there.
- The fur shop's `update_purchase_quantity` divides the gil on hand by
  `price >> 1` with no lower bound, while the sell side
  (`shop_get_item_half_price`) keeps half price at 1 or more.
- `start_teleport` builds a secondary-effect record in a
  local and never uses it.
- A change to a map CLUT row reaches VRAM on the second call of
  `map_palette_upload_clut_rows`: one call loads the 256x14 image to y 0x1ee,
  the next moves it to y 0x1e0. Rows 8 and 9 have state and are uploaded, but
  no 16-bit CLUT ID in the code addresses them.
- Rolls one off their round number: `roll_encounter` tests
  `chance >= pick` over 0..99, so the retail chance byte 30 gives 31 in 100;
  the preferred-job promotion in `proposition_determine_success`
  passes on a roll below 0x3D, 61 in 100; `formula_apply_status_to_action`
  divides the displayed accuracy of separate statuses by four and keeps each
  status on a roll below 24; `generate_record` and
  `store_names` draw a generic name with
  `rand() * 255 / 0x8000`, 0-254, so the last of each list's 256 names is
  never drawn.
- `apply_level_change` stores the new level before
  calling `battle_unit_apply_level_growth`, so an ability level-up divides each
  raw stat by growth + new level; an EXP level-up (`battle_unit_try_level_up`)
  and the generated-unit replay in `calculate_base_stats` divide
  by growth + old level.
- `reward_find_war_trophies_and_bonus_money` computes
  `level_bonus_money` ((highest enemy level - lowest party level) * 100, 0
  once a player unit has fallen) and nothing reads it; REQUIRE pays
  `level_sum_money` and `bonus_money`.
- `formula_evade_apply_weather_on_bows` reads the weather variable
  itself, not `weather_get_effective`: the bow penalty of a storm
  also applies on a snow map and on a map flagged to ignore weather, where the
  elemental and movement rules see no storm.
- `ai_load_ability_entry`: with Throw Item the AI gives an
  item a range of the unit's Move; `target_set_range_for_action` uses
  4, and gives a Mime the same.
- `apply_gns_command`: command rows 0x85, 0x86, 0x88, 0x89, 0x8a
  and 0x8b skip the map-state comparison and are applied in file order, so the
  last of a kind wins: MAP017.GNS has one set-outdoor row followed by six
  set-indoor rows and ends indoors. Row types 0x87 (49 rows in 14 maps) and
  0x8a (125 rows) have no case in `dispatch_gns_resource`, and no
  retail GNS holds a 0x83, 0x84 or 0x8b row, the three that take arguments.
- Terrain data the code does not expect: five tiles hold a slope byte outside
  `map_slope_type_e` (0x10 in MAP017.10, MAP043.9 and MAP078.9, 0x46 in
  MAP036.46, 0x01 in MAP045.7), and the terrain chunks of 37 maps arrive with
  mark bits already set in tile byte 5, which
  `map_copy_tile_data` copies as they are. Tile flag 0x80
  (`cannot_stop`), which `mark_move_destinations` tests, is set
  in no terrain chunk, and tile byte 1 is 0 in all of them.
- `g_map_directory_path` is the string `C:\FFTIMG\MAP\GNS\`; every map
  file load passes it as the first argument, which
  `start_file_load` ignores.
- The Brave Story "Fact" row never appears:
  `SCRIPT_VAR_FACTS_ENABLED` (0x92) gates it
  (`build_menu_rows`), no code sets it, and the one
  event script that writes it, 0x075, stores 0 while setting facts
  0x1a4-0x1aa. The 24 fact names in WORLD.LZW are empty strings.
- `g_treasure_sound_novel_ids` is 0 for every treasure but 31
  (Germonik Scriptures, novel 5), so `brave_story_update_treasure_detail`
  opens no reader for the four book treasures 17-20.
- World-map script opcodes 0x10 and 0x11 pass on a date on or after and on or
  before their operand (`location_opcode_check_date_on_or_after`,
  `location_opcode_check_date_on_or_before`); scenario condition types 16 and 17 are the reverse
  (`EVENT_SCENARIO_CONDITION_DATE_ON_OR_BEFORE`, `_ON_OR_AFTER`), although the
  war-funds and casualty tests beside them share their numbers.
- The Music Test names and comments (OPEN.LZW text sections 24 and 25, text
  ids 0xC000 and 0xC800 + entry) are not translated: each string is the
  Japanese release's glyph codes around Latin fragments such as the composer
  tags.
- SCUS_942.21 0x80028f94-0x80028fe7, between `s_suzuki_smd_modulator_waveforms`
  and `s_suzuki_smd_key_octaves`, is referenced by no instruction or pointer in
  any executable: a five-byte SMD sequence (`B0 91 7F 99 90`), ten 32-bit values whose
  last nine are the LIBSPU reverb work-area sizes of modes 1-9, and a
  0x23-byte `feds` sound bank with id 0x7000.
- Ability data the code and the tables disagree on:
  `set_ability_area` passes range-record `flags_2`
  bit 0x20 (`ABILITY_SECONDARY_FLAG_2_TOP_DOWN`) to
  `apply_vertical_tolerance` as `single_layer`, and none of the
  368 records sets it; the low nibble of
  `ability_data_t.type_flags` holds the ability type plus one (0 for ids 0,
  0x1fe and 0x1ff) and nothing reads it, because
  `ability_get_records` derives the type from the id.
- `status_init_check_data` builds eleven status check sets; sets 2 and 7
  (`STATUS_CHECK_SET_UNREAD_2`, `_UNREAD_7`) are read by nothing.
- WORLD's copies of BATTLE's action-menu and tile-info routines
  (`build_ability_list_rows`, `build_act_menu`,
  `unit_view_tile_info_thread` and nine more) keep BATTLE's
  absolute data addresses 0x80173cbc, 0x8017403c, 0x80174048 and 0x8017404c.
  In WORLD's image those lie inside the text file embedded at 0x8016ee68, so
  the routines would read and write text bytes there.
- `g_terrain_geomancy_abilities` gives Gusty Wind for Sky and Kamaitachi
  for Darkness, Cross section and the unnamed surfaces 0x2f-0x3e; the terrain
  help text says "None" for Sky, Darkness and Cross section.
- `camera_update_rotation` chains a held R1 through
  `set_left_rotation_destination`, as it does L1; the two helpers differ only in
  which way they move a destination that is a multiple of 0x400.
- `copy_job_data` adds the immortal and formation immunity sets
  only to a unit that is both immortal and flagged save- or load-formation;
  the per-flag tests inside the loop are then always true.
- `formula_apply_status_to_action`: the
  `ACTION_STATE_PREVIEW` test in the separate-status loop cannot pass;
  any state but EXECUTE was switched to all-or-nothing above it.
- `loading_message_build` loads the NOW LOADING CLUT to VRAM (0x70,
  0x1e2), 16-bit CLUT ID 0x7887; in battle that is
  `MAP_CLUT(MAP_CLUT_ROW_SPRITES, 7)`, the status bubble CLUT.
- The effect interpreter indexes fixed tables with wider fields and no check:
  the opcode is masked to nine bits over the 120 entries of
  `g_effect_script_instructions` (0x2e-0x77 all run `effect_opcode_invalid`,
  which reports an error without returning a result: `run_script_instance`
  narrows the leftover `$v0` to `s16` and tests it while the script position
  stays unchanged);
  `g_effect_particle_render_handlers` has four entries for
  `(kind_flags >> 12) & 0xf`, so `EFFECT_WORK_KIND_HEAP_BLOCK` would call
  through `g_effect_camera_rotation_mode`; opcode 06 takes a six-bit
  callback slot over the five 32-bit slots of `g_effect_callback_slots`, and a
  keyframe action names slot 1-7 over the record's four (5 writes
  `target_index`). Retail files use opcodes below 0x2e, kinds 0 and
  2 and slots 1-4.
- Special unit name 0x44, "Rofokare", is given to no ENTD unit; every other
  named entry of the 256 is.
- `effect_heap_alloc_block` has no failure exit: when no free block is large
  enough it walks the circular free list forever.
  `select_first_deployable` likewise loops forever when
  no roster unit is deployable for the squad, and
  `alloc_particle_record`, with its free chain empty, calls the empty
  report stub and then reads the next link through NULL.
- `status_screen_thread` hands a unit with mount info (a
  rider or a mount) to DEBUGCHR.OUT's `debugchr_mounted_unit_entrypoint`
  (`status_screen_open_mounted_unit`); any other unit gets BATTLE's own
  panel.
- The camera's L2/R2 input uses `toggle_zoom` and `toggle_tilt`,
  bypassing `camera_start_zoom` and `camera_start_tilt`. The linear
  position, screen-offset, zoom and rotation countdown setters have no retail
  callers, so their per-frame update routines never move anything.
- `formula_49_unused` is a second entry to formula 0x49's
  handler, but neither the formula table nor any caller names it.
- `battle_state_effect_playback_try_enter` and
  `battle_state_effect_enter` are the only code that enters
  `BATTLE_STATE_EFFECT`; neither has a caller, so
  `battle_state_effect_update` never runs.
- WORLD's `menu_screen_fit_child_entry_to_digits` and
  `helpmenu_after_battle_entrypoint` have no callers; their quirks above do not
  execute in retail WORLD.
- `wait_hardware_event` is OPEN's only caller of
  `poll_hardware_event`, and nothing calls the wait routine.
- MAIN's Orbonne caption fades have no callers. `set_up_caption_display`
  is called only by `new_game_fade_in_caption`, so it is unreachable too.
- `suzuki_sound_set_unread_value` stores a halfword in main RAM at `0x80032a28`;
  initialization and shutdown clear it. Original-code references identify only
  stores, and the traced SPU transfer paths do not send it to sound RAM. No reader
  or caller of the setter has been identified; its original purpose remains unknown.
- `system_zero_padding` (0x80010ba8), which `load_battle_overlay`
  reaches after loading BATTLE.BIN, is not BATTLE code: it is the 4 KB of zero
  words after MAIN's C start-up, run as 1,024 no-ops up to the `jr ra` at
  0x80011ba8. BATTLE runs when `system_run_game_loop` then calls
  `battle_entrypoint`.
- `boot_run_startup` installs the empty `file_cd_ready_callback`
  and `file_cd_read_callback`; the `CdInit` it runs next replaces
  them with LIBCD's `def_cbready` and `def_cbread`, so neither ever runs.
- `FlushCache` is called only by the BIOS patches (`_patch_card`,
  `_patch_card2`, `_patch_gte`, `psyq_card_restore_exception_prefix`); no
  overlay load flushes the instruction cache before the new code runs.
- `_spu_FsetDelayR` sets the SPU bus timing for DMA writes and
  `_spu_FsetDelayW` for reads, and `_spu_t` and `_spu_Fr_` call them that way:
  each suffix names the opposite direction.
- `SpuQuit` closes the SPU DMA event before disabling it, so `DisableEvent`
  gets a closed descriptor.
- `update_playback_phase`: nothing stores playback phase 5 or
  `EFFECT_PLAYBACK_PHASE_RESTART_SCRIPT` (6), nor the two start pointers the
  restart arm rewinds to (`s_effect_frame_data_start`,
  `g_effect_animation_sequences_start`), so the restart path never runs;
  phase 5 has no case and would return `result` unset.
- `ai_target_is_untargetable`: the AI never targets a Transparent enemy
  unless it is a rider whose mount is Transparent too.
- World-map script opcodes 0x22-0x24 (`location_opcode_request_draw_location`,
  `location_opcode_request_erase_location`, `location_opcode_focus_location`)
  read their operand without advancing `ip`; each ends the block, so no opcode
  is read from the operand.
- `move_find_get_result` repeats the same `battle_unit_find_at_tile`
  call up to 21 times: its occupant loop never uses the counter.
- `start_next_turn_event` calls
  `store_team_flags` twice, around
  `command_reset_unit_record`, which leaves team flags alone; the first call
  reads `unit->battle_unit` before the null test that guards the second.
- `effect_secondary_start_projectile` tests Throw Shuriken (382)
  on its own before the Item/Throw range 368-393, which already contains it.
- `formula_steal_exp` (formula 0x28) calls
  `formula_set_exp_stolen` after a failed hit test whenever the action
  is not executing; outside execution only a 0% chance misses, so the preview
  shows 0% with an EXP amount. Formula 0x27 (steal gil) fills nothing then.
- `effect_emitter_spawn_particles`: an emitter with no velocity-spread
  curve uses half its start spread, but with a curve it uses the lerped spread
  whole, so a curve at factor 0 gives twice the angle range of no curve. The
  other spawn values use the start value unchanged when no curve is bound.
- `update_texture_animations`: the two loop modes (1 and 3) add 1
  per frame and step when the timer reaches `duration`; the other modes add
  `g_system_vsync_interval` and step when it exceeds `duration`, so one
  duration means `duration` frames in a loop and over `duration` vertical
  blanks otherwise.
- Lookups whose result is discarded:
  `cursor_show_right_unit_panel`,
  `battle_state_resume_attack_phase_update` and
  `battle_state_change_turn_update` call `map_unit_get_source`;
  `battle_state_targeting_range_update` (twice),
  `map_unit_get_tile_center_height_by_battle_unit_id` and
  `aim_step_vector_at_cursor_tile` (the other level's tile),
  `set_fly_to_edge_velocity`, `set_fly_to_center_velocity` and
  `apply_velocity` (twice) call `map_get_tile`; and
  `map_unit_facing_draw_wait_direction` runs a `RotTrans` of the unit's feet
  that the next `RotTrans` overwrites.
- `item_list_update_action_menu` tests `g_item_action_menu`'s
  `selected_index` for the sort-order row (2) even while the single-item menu
  runs; that index was zeroed when the menu opened, so with one item CIRCLE
  plays the invalid sound on every row. The single menu's row actions agree:
  row 2 has 0x13e8 like rows 0 and 1, where the full menu has -1.
- WORLD's and BUNIT's `menu_gfx_init_contexts` set their draw and display
  environments 255 pixels wide where MAIN and CARD use 256, so the default
  drawing area ends at x = 254; `PutDispEnv` still picks the 256-pixel mode.
- Soldier Office: the hire menu shows 1500 gil for row 0 and 1400 for row 1
  (`get_hire_fee`), but the fee that `shop_soldier_update_menu` tests,
  `shop_soldier_hire_update_name` prompts with (0xF82A) and
  `shop_soldier_hire_update_fee` charges is 1000 plus the generated recruit's
  equipment prices.
- `menu_screen_text_measure_width` adds the operand of 0xE7
  (`TEXT_SET_X_POSITION`) to the width as an advance, while
  `menu_text_render_encoded_ids_to_image` moves the pen to that x. Its only
  caller centers the job wheel's job name.
- `zodiac_build_sprite_polygon` (MAIN, ATTACK) and
  `gfx_set_scaled_poly_ft4_8bpp` (WORLD, CARD, DEBUGCHR, REQUIRE) round a
  scaled offset or size by testing the C remainder against half a pixel, so a
  negative product truncates toward zero: -1.75 pixels gives -1 where 1.75
  gives 2.
- `file_poll_load` and `world_map_update_texture_stream` count failed
  seeks, refused reads and stalls in `error_count`, which nothing reads, so a
  disc error restarts the read indefinitely. `file_poll_load` also counts
  `wait_frames` unread; its stall timeout uses `g_system_wait_frames`.
