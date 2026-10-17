# Target quirks

Non-obvious facts about the retail code. They explain source that looks wrong
and mark code that a cleanup must not "fix". Details live in the named file.

## Retail bugs the source reproduces

- `unit_calculate_height_data`: the seven-byte result preserves an
  uninitialized elevation high bit; an unavailable unit leaves the other result
  fields unset. A non-Chocobo that shares its tile (a rider) takes its
  statuses, movement abilities and terrain from the other unit, so it is
  unavailable when its mount is Dead and never for its own Dead status.
- `DrawOTagEnv`: the diagnostic string uses literal `&08x` for its second item, so it does not format the environment pointer.
- `StCdInterrupt`: without `CdlModeSize1`, it skips the local disc-position read but still copies the uninitialized four bytes into the ring descriptor.
- `sprintf`: unchecked precision and zero padding can overrun its 512-byte temporary segment buffer.
- `FntPrint`: unsupported conversions leave the field length undefined; the count limit is checked after writing, and oversized hexadecimal widths can overrun the 512-byte temporary buffer.
- `DecDCTvlc`: the pause path saves its Y predictor at `0x80073f34`, overwriting the first four bytes of the overlapping birthday month-length table. Nothing calls `DecDCTvlcSize`, so the limit keeps its initial 0xffffff halfwords and the pause path never runs.
- EFFECT native renderers: `effect_timeline_step_emitter` sets phase 3 when a callback's keyframe window has two frames left, even if init never ran; the destroy path then reads its latch through the null work slot (kernel RAM).
- `camera_store_state_to_script_variables`: all three zoom
  components are stored to 32-bit script variable `0x20`, so the first two are lost.
- `script_focus_speed` accumulates into an uninitialized `$s6`: the event
  dispatcher leaves `operand_3` there, while camera fusion leaves its key count.
  Those caller values affect the camera duration.
- `script_opcode_37_flip_message_portrait` (BATTLE and
  WORLD): the record index is never
  assigned; all six passes use whatever `$s0` held. Keep the local
  uninitialized.
- `proposition_determine_success`: the preferred-job
  check resolves the party unit once, before its loop, from the scoring loop's
  leftover (past-the-end) index, so it tests that one unit every pass.
- `battle_map_calculate_slope_height`,
  `unit_height_calculate_from_coords`: an off-map point
  (a unit staged outside the map, its world coordinates read unsigned) gets no
  tile, and the null tile is read from RAM address 0. The unit shadow, palette
  modulation and per-frame rotation code (`sprite_shadow_draw`,
  `sprite_shadow_build_quad`,
  `sprite_palette_apply_unit_modulation`,
  `sprite_project_all_units`) do the same for units
  that chapter-end cutscenes walk off the map.
- `animation_distortion_fall_to_target`: the event-state
  (`0x34`) path writes the tile-center depth into `real.vy` (not `real.vz`).
  The first frame adds the fall speed to it, which puts the unit below the
  ground, and the next frame snaps it to the ground: an event fall ends on its
  second frame.
- `battle_map_light_state_command`: several arms return an
  uninitialized pointer.
- `unit_view_preview_caster_stats`: for an item the
  secondary-data pointer stays uninitialized, yet its `flags_3` is read before
  the item test.
- `battle_map_gns_dispatch_resource` passes the packed lighting colors as a
  `MATRIX` to `battle_map_light_set_matrices`, which also copies its 32-bit translation
  values from the following 16-bit direction components.
- `battle_map_load_alternate_mesh`: scales the mesh header's color palette byte
  offset by four, placing the palette read past most mesh files. Its wait step
  tests `file_is_still_loading() != 0`, the reverse of the other loaders:
  it applies the palette and frees the buffer while the read runs, then never
  leaves the wait. Its caller runs it only while `g_battle_map_refresh_pending` is
  set, and nothing sets it.
- `battle_map_queue_textured_triangles`, `battle_map_queue_textured_quads`
  and their untextured twins pack SXY with the full signed 32-bit MAC1 value; a
  negative X therefore forces the 16-bit screen Y to `0xffff`.
- `battle_map_ambient_blend_color`,
  `battle_map_darkness_blend_color`: modes `11` and above leave the
  target color uninitialized (no default case).
- `ai_ability_load_entry`: the CT adjustments test the
  Monster Skill (`0x80`) and Defend (`0x40`) support bits; Short Charge and
  Non-Charge were presumably intended.
- `ai_status_evaluate_cancellation`: the Blood Suck branch
  tests decision bit `0x04`, which is built from Jump.
- `ai_origin_build_monster_skill_mask`: the `-1` neighbor
  offsets are loaded unsigned (+255), so only `+x`/`+y` survive, and the level
  loop runs for level 0 only.
- `ai_origin_build_targetable_mask`: radii 8–15 index past
  the 8-entry template table into the following status weights. Jump Level 8
  can reach this path with a non-arcing weapon.
- `ai_origin_evaluate_self_target`: suspension during the
  first origin's target search marks the second-origin stage, so resuming skips
  the moved-origin pass.
- `ai_origin_evaluate_reflected`: the resume entry
  skips the reflector pointer reload and depends on the caller's `$s2`.
- `ai_math_evaluate_targets`: the extra known-ability call
  in the special-ID loop discards its result.
- `camera_real_coords_step_toward_target`: in the positive
  direction the Y (`vz`) step adds the vector component twice; X and Z add it
  once.
- `screen_open_entry_window` receives requested x/y origins but always
  opens at its fixed default origin.
- `result_build_next_display`: the CT-zero
  display case reads `y_shift` without setting it; only the HP/MP and
  stat-change cases assign it, so the sprite row depends on the leftover value.
- `sprite_load_step_queued` replaces sheet ids from 0xa0
  with Ramza's chapter 1 sheet, but the table has 159 sheets (0 to 0x9e), so
  id 0x9f reads one entry past it. `sprite_queue_unit_graphics_load` tests
  against the count correctly.
- `effect_secondary_spell_charge_handler` saves the inertia threshold
  and sets the secondary-effect value but never restores it, unlike the other
  secondary handlers.
- `unit_calculate_random_equipment`: throwing and bomb item
  types index past the four equipment-category bytes into the first innate
  status byte; that adjacent byte controls their eligibility.
- Both copies of `menu_text_render_encoded_ids_to_image`: the encoded-text branch tests
  its glyph counter (`col`) against the line width but never assigns it; the
  stale register stays below the width, so text never wraps.
- `unit_init`: tints the new unit's palette
  before it clears `mount_state`, so a runtime slot last held by a rider looks up
  a mount that is gone and reads its world coordinates through NULL (kernel RAM).
- `unit_view_display_action_preview`: a skipped
  preview leaves `g_unit_view_preview_target_action` null, and its
  `attack_accuracy` is read (from kernel RAM) before the null test.
- `formation_rebuild_unit_list`: the Soldier Office (shop
  0x65) lists only monsters, yet `formation_init_menu_state` stages unit
  0 even when that list is empty; with no earlier list since WORLD loaded, the
  pointer is 0, so the "restore HP/MP" stores copy kernel RAM 0x10/0x16 to
  0x0c/0x12.
- `ai_facing_choose_for_wait`: when the target shares the
  acting unit's tile, `ai_facing_find_target_direction` returns
  `AI_DIRECTION_OVERLAP` (4), which indexes `viability.by_direction[4]`: the
  never-written `past_viable_directions` byte after the array holds whatever
  the stack held and decides whether 4 is returned as the facing.
- Both copies of `ability_list_scrolling_thread`: `SetSemiTrans(&frame, 1)` passes the
  address of the `frame` pointer, so the semi-transparency bit lands in a stack
  byte past the pointer and the sprite is unchanged.
- Unchecked divisions whose divisor can be zero; the R3000 `div` does not trap
  and leaves quotient -1 (1 for a negative dividend) and the dividend as the
  remainder: `100 / ct` in both copies of `ability_list_get_ct` (CT 0 displays -1 + 1 = 0);
  `0x20000 / battle_lerp_fixed8(...)` for the wave steps in
  `effect_wave_mesh_update` (E033, E035, E073, E079, E080, E230, E453, E456);
  and the direction-times-spread products over `scale` in
  `effect_slot_spawn_motion`.
- Locals read before any assignment, so the value is whatever the caller left
  in a register: `menu_icon_timed_thread` (BATTLE and WORLD) tests `unset`
  (`$s6`) and, when it is nonnegative, lowers `g_thread_current_id` around
  `menu_select_icon_cluts`, as `menu_icon_linked_entry_thread` does for its
  linked entry; `effect_file_update_load` tests `result` (`$a0`) for a
  load state outside 0-3, which nothing sets;
  `camera_get_input_direction` tests `input` (`$a1`) when `mode` or the
  controller mode is outside 0-2; `status_set_action_state` leaves
  `status` unset for an action state other than 0 and 5-8 (its `switch` has no
  default; no caller passes one); `battle_map_gns_dispatch_resource` returns a
  local no path assigns (`$s8`); both copies of `menu_get_cursor_bob_offset`
  walk an unset table pointer for a mode other than 0 or 1;
  `effect_secondary_summon_charge_handler` returns `result` (`$a1`)
  unset when the phase is not 1-3, as does
  `effect_secondary_linear_projectile_handler`;
  `unit_list_get_current_stat`, `unit_list_get_max_stat` and
  `formation_get_unit_stat_by_mode` return `value` (`$a1`) for a stat
  mode outside 0-2; `move_step_start_walk` leaves `facing` (`$a2`, the
  destination tile its callers pass) unset for a direction outside 0-3;
  `battle_map_mesh_load_animation_record` returns `next` (`$a1`) for record
  type 0; `formation_ability_set_menu` indexes the entry ids with
  `cursor` when the list is empty; `world_map_destination_list_thread` uses
  `selected` when no entry matches the current item; both copies of
  `helpmenu_battle_entrypoint` read `node`, `x` and `y` in the cursor loop when the
  text thread is already running on the first frame, and `navigation` when the
  unit view mode is not 1-4; `menu_init_system_function` passes `ptr` on unset
  for SCENARIO_CHECK and CLOSE; `sprite_display_init_numeric`
  leaves `display` unset for a section outside 0-2 (its caller passes 0-2);
  `battle_map_mesh_update_part_animations` indexes the instructions with
  `next`, assigned only when a keyframe expires in the same pass, for a part
  whose animation state is set and whose keyframe is 0;
  `battle_map_light_control_back_color` returns `result` unset for DISABLE
  and RESTORE (every caller issues SET); `memcard_get_info_with_retries`
  returns `result` unset for a retry limit below 1 (its caller passes 2);
  `memcard_poll_info_until_stable` has the same empty-loop result for a
  nonpositive retry limit (its callers pass 3);
  `effect_timeline_select_sound_id` returns `sound` (`$a2`) for a channel mode
  above 4; `proposition_select_report_speaker` returns `selected` for
  a speaker mode outside 1-4 when more than one unit went;
  `weather_stop_sfx` returns an unset `result` no caller reads.
- `save_read_file_chunked` sets `result` to -1 when the last read was
  short and then returns `total`; the test has no effect.
- Both copies of `helpmenu_after_battle_entrypoint` reach
  the shadow quads by indexing past the end of the local `cursor[2]`
  (`quad[2]`, `quads[buffer + 2]`), relying on `shadow[2]` following it on
  the stack; one four-entry array does not reproduce the two stack addresses.
  `entry`, `x` and `y` are assigned only on frames where text
  thread 0x10 is idle and are unset if it is already running on the first
  frame.
- `menu_text_render_decimal_value` and its WORLD twin assign 1 on every rung
  of the minimum-width ladder, so the value is always placed as one digit, and
  they read the digit count as the low nibble for alignment and as the low
  byte for the digit loop.
- `game_over_build_polygons`: `texture_mode` is the constant 1,
  so the 4-bit CLUT arm never runs.
- `movie_init_mdec_state` stores the slice width (24) into the
  global `g_movie_mdec_stream_state.output_rect.w`, not through its
  `state` parameter; the one caller passes that global.
- `action_init_strike_data` tests
  `ability_id >= 0 || ability_id < ABILITY_ID_ITEM_FIRST`, which is always
  true.
- `unit_calculate_encounter_data` copies ENTD bytes 0x21-0x27 with one
  seven-byte loop over the two-byte `ai_target_xy`, reading on through the AI
  flags, the target unit, the two AI targeting flags and
  `ai_initial_enemy_flag`.
- EQUIP never fills the physical accessory evade slot of `equipment_unit_preview_t`
  (0x48); it stores the accessory's magic evade to 0x52.
- Event script, both interpreters:
  - Ten instruction values (0x14, 0x20, 0x34-0x36, 0x52, 0x67, 0xe1, 0xe2,
    0xe4) have operand lengths in the size table and no handler; the
    scanners step over them and the interpreter ends the script on them. No
    retail script uses them.
  - AddGhostUnit runs an empty four-pass countdown before its free-slot
    search; with no free runtime slot it `continue`s without advancing or
    yielding, so the script thread spins.
  - BlockStart, AddUnitStart or CameraFusionStart with no matching End gets
    offset 0 from the finder and the script resumes at byte 1.
  - FocusSpeed and RelativeCamera patch the next instruction's operands
    without checking that it is Camera; Focus scans for the next Camera with
    no End check.
  - UnitAnimRotate tests the animation id's high byte as its flag, never
    reads its last operand byte, and has no absent-unit check, so runtime ID
    2000 indexes the rotation table.
  - WaitUnitReady waits on `unit_is_ready`, so it never waits.
  - Effect stores its x and y operands to two globals nothing reads.
  - DebugPause prints "PAUSE", yields one frame and sets a 16-bit value nothing reads
    (`script_print_debug_message`); instruction 0xdc stores 13 to
    another. Neither changes the script's course.
  - In WORLD, WalkTo is an empty arm outside a block, Arrow resolves both
    units and never sets the trajectory, and ChangeMap calls an empty
    `battle_map_reset_script_light_transform` where BATTLE resets the script light
    transform. WORLD's interpreter never runs (see Other surprises).
  - FadeSound builds its request as `byte 2 << (byte 1 + 16)`: the low half,
    the target volume, is always 0, so it only fades the music out, over
    byte 2's frames doubled for each unit of byte 1.
- `location_step_menu_level` opens the outfitter with shop id 0
  for every town; `shop_run_screen` then takes the id from the
  location variable. The fur shop and soldier office pass their own ids.
- `proposition_step_result_level` clears the in-progress bit with
  `& 0xFFFD` on an `s32`, which also drops the variable's high half.
- OPENING's runtime flags 8, 0x20000 and 0x80000 are tested and never set.
- Suzuki driver: music status bits 0x20 and 0x2000, channel flag 0x10 and
  driver flag 0x2000 are tested and never set; channel flags 0x80 and 0x4000
  and voice-attribute bits 0x7000 are set and never tested;
  `suzuki_smd_set_flags_all_channels` and `suzuki_smd_dispatch_snapshot` have no
  caller.
- CARD save menu state 9 is compared and never stored. EQUIP refuses command
  row 1, so its screen state 3 is never entered. `g_fullscreen_graphics` entries 0, 5
  and 6 all name CHAPTER1.BIN.
- `debug_battle_setup_thread` does `g_system_flags += 3`
  on a 32-bit flags value. System flag bits 0x80, 0x400000 and 0x20000000 are
  tested and never set.
- `thread_resolve_id` searches slots 1-16 where BATTLE searches 1-15;
  WORLD's slot 16 (tutorial text) lies beyond the sixteen the reset clears.
- `card_entrypoint` and `save_run_screen` end with
  `thread_set_parameters(THREAD_BORROWED_SLOT_CARD_BACKDROP, 0, -1, 0)`: the
  backdrop thread exits only when its third parameter is set, so that call
  does not stop it. CARD also tests and stops slot 14 and suspends slot 15,
  which nothing in the overlay starts.
- `world_map_step_path_level` plays sound id 0 when the path
  animation ends; that entry has no streams, so it only stops the channel
  pair.
- `scenario_check_condition`: conditions 18 and 19 (casualties) compare
  with the record's 16-bit `id`, not `value` as the war-funds pair does.
- `scenario_load_conditionals` fills deployment squad ids 0x35/0x36 and
  clears 0x37/0x38, although the deployment screen handles four squads.
- 32-bit variables 0xab-0xff of the script variable block are reachable by no variable
  id, yet variable 0xff gates the BATTLE call in
  `overlay_call_battle_return_zero`, which itself has no caller.
- Formation menu states 22 and 23 have entries in
  `g_formation_menu_handlers` and code paths, and no code stores them,
  so `formation_step_portrait_slide` never runs. Menu input mode 1
  is tested in four places and never stored.
- Both copies of `command_wait_thread` store -2 over the
  Wait entry's row action (7, `MENU_COMMAND_ID_WAIT_YES_NO`) before confirming,
  so the Yes/No entry of the menu table never opens. No code, row action or
  system function opens entries 0x09, 0x0d, 0x16, 0x17 (the only way to 0x18)
  or the four no-act turn menus (0x20, 0x21, 0x32, 0x33), yet the confirm-move
  threads reject Move for 0x21 and 0x33.
- `formation_item_build_list` and `shop_build_stock_list` treat
  filters 0 and 1 (the two hand slots) identically.
- The fur shop's "sell" list (`fur_shop_sell_list_step`) sells
  ordinary inventory at half price; its "buy" list
  (`fur_shop_buy_list_step`) is the poached stock.
- `unit_learn_job_abilities` sets the learned bit twice when the JP
  purchase succeeds: once in the purchase branch and again under
  `if (learned)`.
- `unit_apply_level_growth` calls `rand()` and discards the result; the
  call only advances the generator.
- `sprite_evtchr_load_unit_attack_image` searches
  `g_sprite_evtchr_slots[0..1]` for a free load marker and decompresses into
  that slot's image data, but writes the owner, marker and state of the
  caller's slot.
- `party_update_after_battle` searches the formation sprite groups
  for the unit's row, remaps 0x49 to 0 and 0x47 to 0x48, and never uses the
  result.
- Both copies of `helpmenu_battle_entrypoint`: the
  `UNIT_VIEW_MODE_HOVERED_UNIT` arm and its navigation table cannot be reached
  (the first arm, which closes the help, takes modes 1 and 2); request
  kinds 4, 0x11, 0x12 and 0x17 call the close routine for an empty id and
  then start the text thread anyway, relying on close not returning.
- `heap_free`: the clearing loop stops at the first page byte that differs
  from the allocation id, not at the end of the 64-entry
  `g_heap_game_allocator_table`; an allocation that reaches the last page
  reads on into `g_gfx_draw_envs` and clears its bytes while they match.
- `battle_map_load_data`: the four texture quarter files load at
  `buffer + 0x8000 * (n + 1)`, while `battle_map_gns_dispatch_resource` uploads
  quarter n from `buffer + 0x8000 * n`; quarter 3 is read past the end of the
  0x20000-byte buffer.
- `camera_tilt_toggle`: from tilt target 3 it starts a decrease toward
  target 4, which `camera_tilt_update` has no branch to finish.
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
- `ai_ability_add_usable_from_skillset`: the Faith test that
  skips abilities for a unit with innate Innocent reads the AI workspace's
  considered ability (whichever was loaded last), not the ability being
  added. The bare Jump entry for a unit with no level-jump ability is written
  without being counted, so the caller's next entry overwrites it.
- `target_validate_for_weapon`: the weapon-range flag
  is read from `g_ability_range_data[(u8)ability_id]`, so abilities
  0x100 and up use the record of their id's low byte.
- `unit_apply_level_up_down_ability`: a level-1 target of a level-down
  (the Degenerator trap; formula 0x59 misses at level 1) is clamped back to
  level 1 and still loses raw / (growth + 1) of every raw stat.
- `suzuki_sound_set_reverb_mode`: a negative mode is meant to keep the current
  mode, but the stored mode still carries the 0x100 work-area flag that LIBSPU
  strips from its own copy only. It never equals the SPU's mode, so there is
  no early return, and the reloaded value is treated as a mode of 10 or more.
  The 100 music files all hold mode 4.
- `g_suzuki_smd_opcode_lengths`: the 37 unassigned opcodes have length 0, so
  the look-ahead of `suzuki_smd_read_instructions` would never step past one,
  and 0x9D has three parameter bytes there while `suzuki_smd_stop_sfx` consumes
  two. No retail sequence uses any of them.
- `suzuki_smd_set_instrument`: the waveset kept on the Suzuki heap is the
  0xb30-byte header of WAVESET.WD (176 instruments), and most music channels
  end with Instrument 0xFF before End Bar, so entry 255 is read 0x4f0 bytes
  past the copy.
- Both copies, `item_sort_id_list` and `item_sort_id_list`: the evade
  keys read `g_item_shield_data` for ids 0x7a-0x8f and
  `g_item_accessory_data` for 0x90-0xef by `secondary_data_id`, so thrown
  weapons (122-127) read far past the 16 shield records, and headgear and body
  armor (0-63) read the 32 accessory records and on past them.
- `shop_cash_out_excess_items` walks ids below `ITEM_ID_END - 1`, so
  Phoenix Downs (0xfd, the last item) over 99 are never cashed out; every
  other item walk runs to `ITEM_ID_END`.
- `move_path_calculate` copies 256 terrain costs from row 0 of
  `g_terrain_movement_cost_tables` (64 a row) to the scratchpad cost
  block at 0x1f800100, on over the spread state at 0x1f800180. Its flag-0x100
  test (a WalkTo with terrain costs disabled) compares the cost-table index,
  not the cost, with 0xff, so blocked surfaces cost 1 too.
  `move_spread_store_unit_to_scratchpad` copies 64 and keeps 0xff.
- `formula_weapon_heal` (formula 0x07, the Healing Staff) applies
  zodiac compatibility twice, once itself and once inside
  `formula_apply_physical_modifiers`, whose Defense
  Up and Protect steps also cut the healing to two thirds.
- `formula_raise_level_by_one_add_status_on_caster` (formula 0x57,
  Please Eat) stores a full HP and MP heal (max-HP damage for an undead
  target) and then overwrites `attack_type` with the level change's:
  `action_apply_target_result` still applies the
  amounts, but `result_prepare_display` shows no number.
- `music_unload_scenario_slot` releases a slot's music record and SMD
  without clearing `g_music.state` when that slot is the playing
  one. The shop, formation and both save screens unload their own music this
  way, so the next `music_stop` hands the released record
  to `suzuki_smd_stop_music`.
- `battle_map_clut_stop_cycle` clears the active byte of slot `handle`,
  but `battle_map_clut_start_cycle` returns slot + 1: it stops the slot
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
- `unit_calculate_encounter_equipment`: the random body-armor roll tests
  `UNIT_SUPPORT_SET_1_EQUIP_SHIELD` before asking for `ITEM_TYPE_ARMOR`;
  Equip Armor is tested only for the helmet.
- `target_range_clear_untargetable_panels` (ability ranges) visits
  only the 256 lower-level panels, so an untargetable upper-level tile keeps
  its range; the weapon-range twin
  `target_range_clear_untargetable_tiles` visits all 512.
- `move_settle_on_tile`, `unit_facing_restore_after_attack` and
  `unit_facing_turn_to_target`: for a mounted pair the second
  `unit_set_tile_position` call names the unit's own battle record again
  (the first function finds its "mount" by the unit's own `unit_id`), so the
  partner's battle stats take no tile or facing here. The other two also write
  the partner record's facing with no null test.
- `sprite_evtchr_release_reserved_slot` tests owner bit 0x80
  (`SPRITE_EVTCHR_SLOT_OWNER_ATTACK_IMAGE`), which the EVTCHR (0xfe) and free
  (0xff) markers also carry, so the event instruction releases a saved EVTCHR
  slot too.
- `effect_model_draw_projectile`, the Reflect shield: the depth is
  assigned from each corner in turn where the other models sum them, so only
  the last corner counts, and every triangle is linked at ordering-table entry
  0 whatever its depth.
- `battle_map_load_data`: the fourth selection pass does not exclude filler
  rows (type 0x31; 1,533 in the retail GNS files, all with variable 0x22, state
  1 and comparison equal). One whose comparison holds indexes entry 49 of the
  49-entry `g_battle_map_loaded_gns_records` and
  `g_battle_map_pending_gns_records`: it reads
  `g_battle_map_saved_light_matrix` and can overwrite the first 20 bytes of
  `g_battle_map_local_light_matrix`.
- All six copies of `zodiac_build_primitives` (MAIN, WORLD, ATTACK, CARD, DEBUGCHR,
  REQUIRE):
  the round-to-nearest step of the sprite scale adds 1, not `ONE`, to the 4.12
  product before dividing, so it only rounds up a remainder of `ONE` - 1.
- `state_handle_open_sp2_files` frees its 0x8000-byte buffer when
  `sprite_open_sp2_file` returns 0, which means the read has started, and
  keeps it on 1 (loader busy). The next frame allocates again and has its
  second request refused, so the file arrives in the kept block only because
  the heap hands the freed pages back.
- `brave_story_step_treasure_level` and
  `brave_story_step_unexplored_land_level` wait, in their LOAD phase,
  on the render record named by 32-bit field at level offset 0x08 (`saved_record_index`). Their
  pushes never set it: it holds what the last level at that stack depth left.
- `effect_secondary_set` (fall dust, splash, level up, trap,
  zodiac poof, death smoke, teleport) builds its init record in a local of
  which `effect_secondary_build_init` sets five fields;
  `effect_secondary_init` copies the rest of the two ten-byte blocks and
  `used_weapon_id` as the stack left them.
- `sprite_shadow_draw` sets bit 0 of `shadow_dirty` before testing it,
  so the shadow quad is recalculated on every call and the dirty bit the
  movement code sets gates nothing.
- `formula_damage_pa_times_wp_plus_y_status` (formula 0x2D, Holy Sword)
  runs Strengthen and the target's affinities on the weapon's elements only
  (`formula_element_apply_weapon_strengthen`,
  `formula_element_modify_weapon_damage`): the Holy element in the range data
  of the five Holy Sword abilities (0x9b-0x9f; 0x9e adds Lightning) is never
  applied.
- Half of MP: `skillset_get_unit_ability_data` lists the halved
  cost rounded up and `command_build_ability_list` grays an ability with a nonzero
  listed CT whose listed cost exceeds the unit's MP, while
  `action_check_and_consume_mp` and the AI (`ai_ability_load_entry`,
  `ai_ability_add_usable_from_skillset`) round down. A unit holding exactly the rounded-down half sees an odd-cost
  ability grayed out although it could pay for it.
- `calendar_advance_days` (the scripted multi-day advance) disagrees
  with the daily `calendar_advance_day`: an open proposition that does
  not finish within the advance gains no elapsed days, a birthday reached only
  after the advance passes December 31 is missed, and script variable 0x67
  counts the year when the date passes December 31 instead of on March 21,
  with no cap at 99.
- `status_screen_open_after_battle` reserves 0x1f000 bytes at the high overlay
  address (0x801df000), but the companion loader reads the 0x20000 bytes that
  `g_overlay_companion_executable_sizes` gives JOBSTTS.OUT, so the top two heap
  pages (0x801fe000-0x801ff000) are overwritten without being reserved.
  BATTLE's own `status_screen_jobs_thread` reserves 0x20000.
- `move_step_update_walk_to_edge` adds the destination tile's
  water lift to the current edge height rather than to the destination's, so a
  step up onto deep water can pass the height test as a plain walk.
- `state_handle_change_map_jumping_in` sends event result 9 to
  BATTLE and 10 to WORLD, the reverse of `state_handle_event`, and
  ignores results 11 (reset) and 19 (ending).
- `target_range_mark_unit_panels` is meant to mark the panels
  under allied or enemy units, but its loop over the 21 slots never advances
  the unit pointer, so it marks only the caster's own panel. None of the 368
  ability range records sets either flag that calls it.
- `ai_ability_build_list` ORs the formula kinds of both hands'
  weapons and then switches on single values, so a unit holding two kinds (an
  HP-absorbing weapon beside a damaging one) gives its Attack entry no target
  team.
- `screen_clamped_icon_list_thread`: Up steps the cursor up, but the Down
  test is inverted, so the cursor moves down on every frame Down is not held
  until it reaches `max_row_index`. Nothing refers to the thread.
- `menu_build_and_upload_window_frame_image`: the banded upload (mode
  `MENU_WINDOW_FRAME_UPLOAD`, an image of 0x3000 bytes or more) starts a band
  when `(i & 0x5f) == 0`, at rows 0x20, 0x80 and 0xa0 rather than every 0x60
  rows, and its last-band test `i - height >= 0x60` is never true. After the
  band at row 0x20 the rows are written before the start of the buffer. No
  retail window is known to be that large.
- `unit_calculate_random_equipment` restarts its candidate list when an
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
- DEBUGCHR also serves the normal mounted-unit Status screen. That path keeps `g_debug_unit_editor_active` clear; the editor sets it. The editor is registered as battle menu entry 44 (`MENU_COMMAND_ID_DEBUG_UNIT_EDITOR`), but no C caller or retail menu-row action selects that entry. The mode flag does not enable access to it, and `g_debug_display_enabled` controls diagnostics separately.
- DEBUGCHR's unit editor (`debugchr_editor_entrypoint`) lets the job, the five
  equipment slots (Triangle empties one) and the five skillset and ability
  slots be changed, but `debug_unit_editor_apply_values` writes
  back only HP, MP, CT, level, EXP, Brave, Faith, zodiac sign, Move, Speed and
  Jump, and `unit_panel_load_selected` reloads the unit after every
  change, so the other edits are lost at once.
- `sound_novel_calculate_read_percent` (no caller) counts
  0x1000 bits from `g_sound_novel_read_section_bits`, which holds 1,280 (40 words), so it
  also counts `g_item_location_flags` and the first 224 bytes of
  `g_item_poached_quantities`.
- `movie_update_stream_frame` restarts a stream whose ring has fewer than
  16 free sectors through `movie_start_cd_read`, which reads with
  mode 0x1a0; the first read in `movie_start_stream` uses 0x1e0, so the
  restart drops `CdlModeRT` (XA-ADPCM playback).
- `ability_store_skillset_abilities` bounds the support class with
  `<= ABILITY_ID_MOVEMENT_FIRST`, so Move+1 (486) is filed as a support ability;
  every caller asks for action abilities or for all four classes, so nothing
  sees the difference.
- Tests that cannot succeed: `unit_init` runs
  `animation_run_wep_eff_seq_script` for the new unit's blocks only for a
  sheet id below `SPRITE_SHEET_ID_VELIUS` (60) and at least
  `SPRITE_SHEET_ID_BOY` (74);
  `proposition_advance_day` caps waiting days above 32, and
  its only writer, `proposition_step_result_level`, stores 32 or 16;
  `state_sync_frame` zeroes a `g_system_min_vsync_interval` of 1
  under `wait < g_system_min_vsync_interval` with `wait` at least 2;
  `state_run_map_load` loops until
  `deploy_update_controller_input` returns -1, and it always
  returns 1; the first `switch` of `move_step_apply_velocity`
  repeats the layer test that the statement before it has just made false;
  `turn_calculate_status_flags` cannot report Sleep, whose
  `g_status_effect_data` record carries the freeze-CT flag tested first;
  `animation_set_mounted` tests a depth masked to a byte for `< 0`
  and a nonzero animation for 0; `menu_get_thread_activity` tests `i != 3`
  inside its scan of threads 4-9;
  `camera_rotate_when_unit_tile_not_visible` adjusts a negative
  `direction` after masking it to 0-0xfff; selector 15 of
  `battle_map_tile_set_value` retries `battle_map_tile_selector_15_stub`,
  which returns 0 at once, up to 1000 times before load exception 0x80.
  `animation_run_unit_seq_script`, `animation_run_wep_eff_seq_script` and
  `sprite_frame_load_unit` test sequence and frame pointers
  for -1, which the unpackers replace with the start of the data, and the last
  tests a three-bit part count plus one for `> 8`. The weapon interpreter's
  own bounds (`sequence_id > 0x100`, `frame > 0x200`) admit the index one past
  each table. `ai_select_autobattle` falls back from the
  recovery policy to retreat on a -2 result, which
  `ai_select_protect_allies` never returns (only 0 or -1);
  `turn_take_next_event` has a case for turn event 0x400
  (`TURN_EVENT_UNKNOWN_0400`), which its only source,
  `turn_run_clock`, never returns;
  `menu_run_icon_selection_loop` clears both icon records' `_unknown_78` when
  one holds 0 and the other 2, but their only writer,
  `menu_build_window_sprites`, stores 0.
- `action_message_queue_for_unit` tests the 16-record
  capacity of `g_action_messages` once, before it walks the
  unit's 11 reportable outcomes: a unit reached with 15 records queued can
  append ten more past the array, over `g_sprite_compressed_nibble_phase`
  and the globals after it.
- `ai_origin_evaluate_self_target` and
  `ai_origin_evaluate_reflected` run
  `ai_origin_is_allowed` (deep water, Geomancy terrain, an
  adjacent Monster Skill holder) on the unit's current tile in both passes, so
  the starting tile decides the move-then-act pass, which casts from the
  destination; `ai_origin_evaluate_before_and_after_move` tests the destination there.
- `target_select_random_fire_tile` has no test for
  an empty candidate list, and `target_calculate_action_tiles` calls
  it for any result but -1, including 1 (no tile marked); it then targets the
  upper-level tile named by the stale `g_target_panels[0].mark`.
  Whether a retail action gets there is unknown.
- `effect_script_free_state_record` follows each freed work record's `next`,
  which `effect_particle_free_work_record` has just set to the old free-chain
  head. Only an empty list ends cleanly: with no free record the walk stops
  after the first and leaves the rest attached; otherwise it frees free-chain
  records again and then loops between two records forever.
- `battle_map_light_matrix_command`: the blend and delta steps (0x67, 0x69)
  start each translation from the start matrix's `m[i][3]` (the next row's
  first element, or the padding after `m[2][2]`) instead of `t[i]`. Nothing
  issues the start commands 0x66 and 0x68 that queue them, so they never run.
- `effect_camera_execute_position_command`, source
  `EFFECT_CAMERA_SOURCE_TARGET_AVERAGE`, sums the world coordinates of the
  unit targets only but divides by the whole target count, so tile targets
  pull the average toward the map origin, and no targets divides by zero. The
  average is stored only when an offset is given; every caller passes one.
- `unit_place_in_new_location` (WarpUnit and
  `script_warp_unit_display_to_paired_unit`) passes `unit->facing`, the
  quadrant shifted left 10, as the `u8` facing of
  `unit_set_tile_position`; its other callers divide by
  `UNIT_FACING_QUARTER_TURN`. The low byte is 0, so a warped unit's battle
  stats face south while its sprite faces as asked; facing evade reads the
  stats.
- `animation_distortion_jump_start` and `animation_distortion_jump_rise`
  size the rise as `0x120 - ground y`, the altitude above world height 0 of a
  point 0x120 over the ground, but cover it from the unit's current height:
  a unit on ground h units up ends 0x120 + h above it. The second function's
  early exit treats the same value as an absolute altitude.
- `state_handle_confirm_action` stores answer 7 with a unit on the
  cursor tile as the action's `targeting_type`, which otherwise holds
  `TARGET_TYPE_TILE` (5) or `_UNIT` (6): readers that test for UNIT
  then aim at the stored tile, the two that test for TILE at the unit. The
  confirm prompts' command maps give only 5, 6 and 8; 7 comes from
  `command_resolve_selection`'s fallback for a menu outside its table, and
  whether that happens here is unknown.
- `move_path_calculate` (WalkTo): the destination filter repeats the
  water tests of `move_range_set_tile_flags`, but refuses lava only
  when `move_type` is `MOVE_CLASS_FLY`, where that function refuses it unless
  `can_pass_lava` is set. The walker always uses `MOVE_CLASS_NORMAL`, so lava
  is never refused as a WalkTo destination.
- Both copies of `script_opcode_2a_block_start_thread`: the block's WalkToAnim
  clears `g_unit_was_moving_latch[runtime_id]` outside its absent-unit test,
  so an absent unit (2000) zeroes the halfword 4000 bytes on, the `w` of
  `g_menu_window_buffers[5].sprites[3]` in BATTLE and WORLD. The interpreters'
  own WalkToAnim keeps the store inside the test.
- `memcard_file_write_buffer` (CARD and WORLD) and
  `save_read_file_chunked` seek before each of their 40 attempts by the
  bytes transferred so far from the current position (origin 1), not from the
  start: after a short first transfer of n bytes, the next attempt works at
  file offset 2n with the data from offset n.
- `item_get_ranking_value` (EQUIP and WORLD): item 0 is a weapon to
  `item_get_menu_category`, but the weapon branch excludes it, so it falls
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
  (`unit_list_find_by_battle_id`), not the rider's grid cell; they
  differ by the mounts sorted before the rider. Browsing onto such a mount
  with L1/R1 puts the grid cursor on a later unit's cell (or past the last),
  and ending the browse there selects that cell's unit.
- The right-aligned number commands
  (`menu_opcode_17_draw_right_aligned_number`,
  `menu_opcode_17_draw_right_aligned_number`,
  `menu_opcode_17_draw_right_aligned_number`) count their blank cells
  down to -1 in an `s16` with no lower bound: a value with more digits than
  the field starts below -1 and queues a blank quad about 65,535 times.
  Whether any retail field is too narrow is unknown.
- `screen_fade_update_in` does not clamp its level when the fade-in
  ends, unlike `screen_fade_update_out`: at a frame step of 2 the
  level goes from 16 to -16, and the last subtractive tile takes the low byte,
  240, darkening the screen for one frame. Whether a fade-in runs at step 2
  is unknown.
- `formation_get_unit_palette_index` treats every formation sprite code
  from 0x3f up as a monster's: the 28 story sprite sets that map to 0x78-0x93
  (Zalbag, Dycedarg, Wiegraf, the Lucavi and others) index the 20-entry
  `g_formation_monster_sprite_table` 37 to 64 bytes past its end. Whether a
  party unit can carry one of them is unknown.
- `sound_novel_opcode_31_stop_pending_music` clears `tracks[slot]`,
  but `sound_novel_change_music` and the audio queue keep slot n's
  track in `tracks[n - 1]`. The novel always uses slot 2, so the store hits
  `current_slot` (zeroed next) and `tracks[1]` keeps the unloaded id: a later
  queued play of that track switches to the empty slot without loading it.
- `window_draw_gs_animated_record`: a part with its own palette gets
  CLUT x `(clut_x + (word & 0xff0000)) >> 12`, shifting the cel's CLUT x
  origin out with the palette field; OPEN's
  `window_draw_gpu_animated_record` adds the origin after the shift. All
  141 WLDCORE cels have CLUT x 0, so nothing shows.
- `party_find_ramza` returns 16 (`PARTY_MEMBER_SLOT_COUNT`) when
  no formation slot holds Ramza, and the four MCBrave/MCFaith opcodes
  (`location_script_opcode_25_check_ramza_brave_at_least` through
  `location_script_opcode_28_check_ramza_faith_at_most`) use it unchecked, so
  they test roster slot 16, the first guest slot.
- `music_start_load_into_free_slot` allocates the SMD block before
  it requests the read; when `file_request_read_bytes` refuses (loader
  busy) it reports `SYSTEM_ERROR_SMD_LOAD` and returns 0 without freeing
  the block. `music_open_into_slot` (no caller) does the same.

## Calls that disagree with the callee

Where callers pass arguments a callee never reads, the callee declares them as
`unused_` parameters and the calls are plain. The calls below cannot be
written that way.

- `StCdInterrupt`: emulated header/payload copies pass a fourth completion argument to the three-argument `mem2mem` helper, which ignores it.
- `g_thread_call_target` is the main-stack dispatch slot for callees
  with different signatures; assignments erase their function types.
- `screen_fit_parent_entry_to_digits`: calls
  `text_count_decimal_digits` with no argument; the callee reads a stale
  `$a0`. Nothing calls the function itself.
- `animation_distortion_fall_to_target` passes a coordinate
  buffer to the one-argument
  `unit_height_calculate_from_world_coords`, as it does to
  `unit_height_calculate_with_caller_coords`;
  `animation_distortion_jump_to_own_tile` does not.
- `g_suzuki_smd_modulator_waveforms` entries 8-15 call the void
  `suzuki_smd_modulator_deactivate`; `suzuki_smd_update_modulators` adds the
  leftover `$v0` (the cleared modulator flags) to the channel output.
- `music_update_tunes` passes `current_music` to `suzuki_smd_is_music_playing`
  even when no scenario music is loaded; the null record reads the status
  16-bit value at address `0x10`.
- `menu_animate_window_quad_crop` passes an uninitialized
  source (a stale `$s5`) to `menu_zoom_cursor_frame`, which never reads it.
- `battle_map_step_deployment_init_sequence` and
  `battle_map_step_init_sequence` call `sound_stop_sfx` without its sound
  id: step 0 passes `map_id`, still in `$a0`; step 11 passes whatever
  `music_unload_current_scenario` left there.
- `unit_get_world_coords_by_runtime_id` returns NULL for a missing
  unit, but its callers test for -1 and read through it. A teleport-out with
  removal (Wiegraf at the Fovoham windmill) fades the unit out while the
  teleport particles still read its world coordinates, from kernel RAM at address 0.
- `script_process_inflict_status_commands` skips a battle id of 2000, which
  `unit_get_battle_id_by_runtime_id` never returns: a runtime unit with
  no battle stats gives -1, `unit_get_stats` then returns
  NULL, and the Float and Jump tests read kernel RAM.
- `battle_map_get_tile_data_ptr_from_battle_id` and `battle_map_get_unit_tile_by_runtime_id` read the
  unit's map position without checking the lookup, so a unit with no runtime
  record (an arc-trajectory target in a Chapter 2 battle) is read from kernel
  RAM at address 0.
- `portrait_build_unit_poly` reads the speaker's battle record
  through `unit_get_stats`, which returns NULL for a
  speaker who is not on the field; the portrait test then reads kernel RAM.
  `unit_panel_fill_billboard` (BATTLE and DEBUGCHR) does the same while
  counting units, for a runtime record with no battle unit (the final battle).
- `item_sort_id_list` and `item_sort_id_list`: each counts a unit's five equipment ids into a
  256-byte local array, but the ids are 16-bit values and an item tried on in the
  shop's fitting room carries flag bits above the low byte, so sorting the
  list then increments a byte far past the array.
- `script_resolve_runtime_id`: for the active-turn selector 0x69 with no unit
  holding the turn, it falls back to ENTD units 1 and 2 but tests the finder's
  battle id, not the loop counter, against 3, so a unit found in battle slot 3
  counts as missing; with neither on the field the Jump test reads the
  finder's record (NULL when there is none: kernel RAM at 0x58), and if it
  passes the lookup goes on with ENTD unit 3. In the Ramza game-over event
  (0x194) with no Ramza in the 21 records it returns -1, not
  `EVENT_RUNTIME_ID_NONE`, which is what its callers test for.
- `unit_init`: stores the spritesheet-slot
  claim (0xffff when all nine slots are taken) in the u8
  `spritesheet_vram_slot` and tests the stored 0xff against 0xffff, so the
  failure path never runs and a tenth sheet loads into slot 255.
- `ai_ability_load_entry`: a Jump ability's CT estimate
  for the acting unit reads `acting_unit`, which is NULL during the AI
  workspace setup at battle start (`acting_unit_id` is 0), so slot 0's Jump
  divides kernel-RAM bytes.
- `unit_list_entrypoint` passes a second argument, 0, to the
  one-parameter `unit_list_draw_shaded_rows`; its other caller passes
  one.
- `menu_init_system_function` returns `$v0` as the void
  `announcement_select` left it.
- `screen_text_bind_section` has no return statement;
  `save_build_slot_description` uses the address its last store
  left in `$v0`.
- `effect_particle_spawn_from_emitter` has an `s32` result it never sets
  and no caller reads.
- `animation_distortion_scale_pulse` and
  `animation_distortion_teleport` fall through without an
  `s32` result; the animation dispatcher ignores their results.
- `thread_resolve_id` and `thread_resolve_id_after_current` fall
  off their end after `thread_exit_current()`, like the WORLD twins.
- `ai_calculate_clockticks_until_death_counter_expires` is defined
  with three parameters and both callers pass one; the other two are scratch
  it assigns before reading.
- `battle_map_calculate_slope_height` masks its level to a byte; two callers
  pass the 16-bit `action_command.target_elevation` unmasked.
- WLDCORE passes its 8-byte records by value:
  `RECT` to the three render-record builders and
  `point32_t` to `window_build_yes_no_panel`.
- Both copies of `script_filter_unit_id_by_mode` read `in_id` as a 16-bit value; most
  callers pass the address of an `int` loop counter (BATTLE through a
  `(u16*)` cast), so the id is that counter's low half.
- `move_path_calculate_walk_to` has no return statement; its caller
  uses the path block pointer `move_path_calculate` left in `$v0`.
- `battle_map_load_gns_and_move_find_items` falls off its end for a map with no
  GNS sector and returns what `bzero` left in `$v0`. `battle_map_load_data`
  compares the result with the GNS table and, when it is not the table (also
  when the read could not start), returns 0 with its 0x20000-byte load buffer
  still allocated.
- `g_effect_secondary_handlers` entries 0x07, 0x0a, 0x0b, 0x0e and 0x10
  set no result, although `effect_secondary_update`
  ends an effect on a handler result of 0. No record of
  `g_effect_secondary_animations` names them.
- `ability_list_is_non_action_learned` passes one 32-bit pointer slot (0x801f0050, named
  both `g_ability_list_selected_ability_data` and
  `g_ability_list_selected_ability_secondary_data`) for both outputs of
  `ability_get_data_and_type`, so the ability data pointer is
  overwritten by the secondary data pointer.

- `menu_input_update_event_state` passes six 32-bit values starting at `g_input_event`
  to the event input loader; CARD's controller pointer at +4 is copied as held
  buttons, and the remaining 32-bit input values are shifted by one field.

- `screen_input_update_dialog_state` and `script_run_event_frame` pass six 32-bit values
  starting at `g_script_event_input` to the event input loader. The
  next five 32-bit values include unrelated status-label data; the corresponding held,
  previous and repeat globals reside elsewhere.
- `skillset_monster_collect_abilities` returns 0 from its early exits
  but falls off its end after building a list, and writes no terminator after
  it; its one caller, `command_build_ability_list`, ignores the result and has
  filled the id column with 0xffff beforehand.
- `effect_get_world_coords_by_battle_id` passes the unit's runtime id to
  `effect_resolve_target_world_coords`, which takes an index into the
  effect's target list. Nothing calls it.

## Declaration leads

- The heads of `equipment_unit_preview_t` and `unit_list_record_t` overlap
  `unit_panel_summary_t`, but their fields at 0x1e and 0x20 differ.
- BUNIT, EQUIP and WORLD use the 236-byte `menu_gfx_context_t`. JOBSTTS omits the draw-mode pool pointer at 0x60 and uses a
  244-byte stride.
- WLDCORE's `g_calendar_zodiac_start_dates` uses `calendar_date_t`
  records; OPEN's `g_birthday_zodiac_start_dates[24]` stores the same
  `{month, day}` pairs as bytes.
- `g_item_location_flags` (`0x80059414`): only the first 64 bytes (512
  Move-Find bits) are proven; the next 64-byte block is saved with it but has no
  known meaning.
- `g_sound_novel_sound_quads` (`0x8009ec18`) starts one row before
  the sound rows, which begin at `0x8009ec1c` for sound id 1: its row 0 is the
  eighth 32-bit entry of `g_tutorial_battle_event_ids`, whose eight entries
  `tutorial_handle_battle_list_input` indexes.
- `g_effect_secondary_ending_function_id` (`0x801b8530`) is
  `g_effect_secondary_animations[EFFECT_SECONDARY_ANIMATION_ENDING].function_id`
  under a second name; the table's own symbol covers the 21 records before
  it.

## Duplicated code

Shared sources compile at each original entrypoint with its own module
bindings. Distinct entrypoints and genuinely different implementations remain.

- `brave_story_push_fact_level` and
  `brave_story_push_event_level` perform the same operation at menu
  levels `0x2f` and `0x1e`; the latter reserves 8 unused stack bytes.
- The ambient-light and darkness blend routines operate on different records.
  WORLD's scaled-draw-area initializers install different second rectangles.
  Similar bodies alone do not make these bindings interchangeable.
- Some routines occur twice within one module with different call and data
  addresses. The entries named `script_run_next_event_unused` and
  `unit_add_event_offset_by_runtime_id_unused` have no caller, as does
  WORLD's unused next-event entry.
- Overlays call both the resident `thread_is_running` and local
  `menu_thread_is_running` entries. Keep those callees distinct even though
  their running-flag reads are identical.
- Overlays share load addresses, so an address match is not an identity
  match. Compare bytes: REQUIRE and EQUIP both have code at `0x801c2dcc`, and
  the two routines differ.
- `unit_panel_numbers_polarity_thread` (ATTACK, REQUIRE) points both slot branches
  at `g_unit_panel_editor_numeric_state`, where `unit_panel_numbers_thread`
  gives the preview slot `g_unit_panel_editor_numeric_state_b`. Both overlays start it
  only in `THREAD_SCREEN_SLOT_NUMERIC_PANEL`, so the shared branch never runs.

## Other surprises

- `save_restore_image` skips a saved Unit List order if any of its first 11 keys is 11, and skips a saved Formation order if any of its first seven keys is 11. It preserves the live order, writes its final -1 terminator, and continues loading the save. Formation key 11 would read the fixed 100 CT maximum; the Order Unit menu exposes only keys 0, 1, 2, 4, 5, 7 and 9.
- `weather_run_command` selects precipitation from the raw script weather and snow flag. The rain and snow drawing paths do not test `BATTLE_MAP_WEATHER_FLAG_IGNORE_WEATHER`: that indoor flag suppresses elemental and movement weather rules through `weather_get_effective`, but does not itself suppress the particles.
- `move_start_next_post_event` handles `MOVE_POST_EVENT_SOURCE_DISPLAY` (0x200), and `state_handle_crystal_learn` clears it, but the normal producer chain through `move_get_post_events` and `move_get_ability_events` never sets it.
- `animation_set_for_status` retains two branches selecting animation 0x21 when bit 0 of `status_flags_1_4` is set. Normal status updates never set that bit. Crystal's mask 1 belongs to the separate `status_flags_5_6` word; this does not rule out animation requests from other paths.
- `formation_get_unit_palette_index` treats every portrait index of 63 or more as an index into its 20-byte monster remap. A party record with `sprite_set` 5 selects portrait 120 and reads remap byte 57 in the following window data. Other special identities also overread the remap; their normal-game reachability in Formation is unverified.
- `debug_load_efc_font`: patches the low immediate byte of `FntLoad` at `0x80022f5c`, changing its font CLUT placement from `y + 128` to `y + 127`.
- `StCdInterrupt`: tracks frame numbers through the header's low `u16`, while DMA completion and OPEN retain the full `u32` frame number.
- `psyq_gte_apply_rotation_ir`: writes saturated IR1–IR3 32-bit output values and returns its unchanged third argument.
- `sprintf`: unsupported conversions terminate output, `%#p` prefixes `0p`, hexadecimal alternate form prefixes zero, and `%#.0o` with zero emits no digit.
- JOBSTTS.OUT offset `0x0` and BUNIT.OUT offset `0x200` hold the literal `"%d"`
  (`g_status_screen_text_decimal_format`, `g_unit_list_text_decimal_format`) used by `menu_opcode_0d_draw_formatted_number` and
  `menu_opcode_17_draw_right_aligned_number`.
- The 32-bit game-options value (`game_options_t`) packs its last five fields
  out of array order.
- CallFunction (both copies of `script_execute_event`)
  tests selectors in sequence against one operand that the arms modify. So
  `0x06` also runs the `0x0f` warp, and `0x0e` can fall into later arms.
- WORLD's copies of some event-library routines call BATTLE-only functions
  at their BATTLE addresses, which hold unrelated code or data while WORLD and
  WLDCORE are loaded: `script_opcode_1a_map_darkness` calls `battle_map_ambient_start_blend`
  (`0x800934d0`, WLDCORE data), `unit_apply_staged_status_data` runs
  `status_queue_display_flag_change` on the main stack (`0x80068e80`, 0x10 bytes into
  `world_map_update_scroll_axis`) and `unit_save_staged_status_data`
  runs `status_apply_unit_action_removal` (`0x8018ec44`, WORLD text data).
  `unit_get_battle_id_by_event_unit_id` calls
  `unit_get_battle_id_by_runtime_id` (`0x8008cdd0`), which in WLDCORE
  is the epilogue of `world_map_init_dots`: it would reload
  `s0`..`s4` from the caller's frame and pop 48 bytes of its stack. The other
  unit-core helpers the shared event code calls (`unit_has_runtime_id`,
  `unit_get_runtime_id_by_battle_id`,
  `unit_get_stats`,
  `unit_entd_find_stats`,
  `sprite_load_unit_graphics_by_battle_id`) bind the same way, into the
  middle of WLDCORE's map-dot routines or WORLD's menu data.
  `unit_set_animation` likewise reaches three BATTLE-only animation setters,
  behind script-variable tests WORLD scenes would have to pass first.
  `camera_wait_until_idle`, `mini_menu_select` and
  `help_start_menu_thread` call `camera_is_active` (`0x8008e17c`,
  0x1e4 bytes into WLDCORE's `travel_find_best_path`), and
  `menu_update_menus_and_event_speed` calls `system_get_vsync_interval`
  (`0x8008ded8`, 0x2dc bytes into `world_map_draw_path_animation`).
- OPTION.OUT carries its own copy of `menu_init_scaled_draw_area_packets`
  (`0x801c02d0`) but its window and option threads call BATTLE's copy
  (`0x80138adc`); target overrides keep that binding.
- `reaction_dispatch_when_targeted`: formula 7
  suppresses reactions. Counter Magic (`0x1b3`) is tested last, after byte
  `+0x8d`, so Counter (`0x1ba`) wins.
- `g_debug_display_enabled` is only ever cleared by retail code; the
  add-unit threads and unit-summary panels print debug output when it is set.
- `world_map_update_input_repeat`: the arms test
  UP, DOWN, RIGHT, LEFT, R1, L1, but the counters sit in UP, DOWN, LEFT, RIGHT,
  R1, L1 address order. The early-out clears only the four direction counters.
- `random_battle_roll`: encounter masks count from
  the high bit, so the chosen ENTD is `entd_ids[7 - bit]`.
- `action_finalize_draw_out_katana`: when the
  katana breaks, the strike work's `can_earn_experience` is copied onto itself.
- `memmove`: the overlap-safe copy returns the original destination on its backward path and the advanced destination on its forward path.
- `StartRCnt` and `StopRCnt` index the IRQ-mask table before validating a counter: selector 3 changes the VBlank mask even though `StartRCnt` returns zero; `StopRCnt` always returns one. Larger indices can read past the four-entry table.
- `SpuGetVoiceEnvelopeAttr`: the public `s32*` key-status output is written with a 16-bit store; its upper half remains unchanged.
- `StartCARD` and `StopCARD` leave their declared signed 32-bit results unspecified.
- `_patch_card2`: exchanges five resident/BIOS 32-bit instructions, so calling it again reverses the exchange rather than repeating an idempotent patch.
- `SetDrawLoad`: initializes the upload header and rectangle but leaves pixel payload storage to its caller.
- `suzuki_sound_get_largest_free_block`: reports the rounded gap including space needed for a new block header, rather than a directly usable payload size.
- `sprite_sheet_load_into_vram_slot`: its fixed 0x30d4-byte copy from the variable compressed SPR tail at 0x9200 extends beyond the file-sized allocation in 153 of the 154 nonempty unit sprite-table entries and includes retained heap bytes.
- `animation_update_unit_wep_eff`: when neither EVTCHR slot is free, one path tests `j` before setting it.
- `g_effect_file_disc_entries` (`0x801b53e8`) stores whole-sector sizes, and entry 0 repeats entry 1: effect id 0 loads E001.BIN, so E000.BIN's code is never loaded through the table.
- `effect_noop`: the native callback returns without setting a result; opcode 07 ignores that result.
- E464's map-vertex particle callback accepts groups 1-12 but indexes a
  three-entry location table; all 16 shipped emitters use group 0, so the
  out-of-range path is inactive.
- `EFFECT/E223.BIN` through `E225.BIN` contain effect data but have zero resident load sizes; requesting those file IDs loads Cure instead. The ability-effect mapping does not reference them.
- `screen_load_wldface_portraits`: the WLDFACE rectangles (`g_screen_wldface_vram_rects`) use x = 1664–1856; the GPU masks VRAM transfer coordinates, so the pages land at x = 640–832.
- `suzuki_sound_init`: `suzuki_sound_init_sfx_music` reaches `suzuki_smd_insert_music`, which disables and re-enables `g_suzuki_sound_root_counter_2_event` before the initializer opens that event: handle 0 at boot, the handle `suzuki_sound_quit` closed on a soft reset.
- `equipment_load_images` and `unit_list_init_vram_and_fade`: store 16-color CLUTs into the 4-entry `buf1`; they run on into `buf2`, which the frame places directly after it (the source reads as two arrays, the stack holds one).
- `effect_script_opcode_2e_invalid`: opcodes 2e–77 report an error without setting a handler result; `effect_script_run_block` then tests the undefined return value.
- EFFECT scripts never run seven state handlers: E000's script calls only `effect_noop` (opcode 07) and E314's names no file function, so native function ids 1 and 90 are bound by nothing; E134/E242 `effect_ring_stack_mesh_update`, E454/E455 `effect_map_set_3d_objects_state_2` and E456 `effect_map_freeze` sit in callback slots (script opcode 06) that no keyframe selector fires. E457.BIN, which has no code, binds native function 88 (0x801c2578, E259's `effect_dome_mesh_update`; data in its own image) to slot 0, and none of its keyframes names a slot.
- `battle_map_draw_mesh_and_weather`: when map command 0x96 (E458) ends, `weather_run_command(0x8b)` restores a non-snow rain or storm mid-draw; the mode-0x55 block then re-links `ft3` drops the 0x96 block already linked at OT+2, an ordering-table cycle that holds `DrawSync` until its 240-VBlank timeout.
- `save_run_screen`: its 0xCCD8-byte stack frame, containing packet pools and directory entries, reaches below 0x801ff000 into the top cells of MAIN's game heap (0x801df000-0x801ff000); it is safe only while those cells are free.
- `tutorial_opcode_17_end`: two paths fall off the end of the s32 handler and return a stale nonzero `$v0` (the fade's result, or the 1 just stored), so the formation loop keeps running.
- `menu_draw_thread_status_indicators` in WORLD and BUNIT (the ATTACK, DEBUGCHR and REQUIRE
  copies, and EQUIP's `menu_draw_thread_status_indicators`, have no caller): while a unit-status banner thread (8 or 7) is being
  stopped its first parameter is 0, and the panel check reads that display
  record at RAM address 0.
- `ending_draw_text_records`: passes OT entries by value (`otag[15]`, `otag[8]`) to `AddPrim`; after `ClearOTagR` each holds the physical address of the entry below it, so the credits fade, bands and glyphs link into layers 14 and 7 through the KUSEG mirror.
- `picture_step_dissolve_upload`: the step-0 call (start the dissolve) has no return value of its own; `$v0` keeps its clearing cursor, the address one byte below the band-progress table. Initialization callers ignore that result.
- `menu_build_window_sprites`: for the Bar's send-unit list (header 0x10) the arrow x reads the "height" of the record at the panel's value pointer, `&g_window_panel_value` + 6, which is the upper half of `g_proposition_dispatch_days` (0).
- `crystal_generate_treasure_item`: outside action execution (AI simulation, preview) it returns with no value, so `$v0` still holds `g_action_state` and the crystal pickup result names item 1 or 2 as the treasure.
- `new_game_update`: no retail code sets step 5, which pops New Game and opens the Music Test (`music_test_open`, controller slots 7 and 8); only a poke of the 32-bit step value reaches it.
- WORLD's copy of the battle menus is dead code: nothing in any module calls or names `menu_init_system_function` (0x800f5230) or `mini_menu_start` (0x800f0e48), so modes 1 (AT list) and 2 (dead-unit panel) of `option_entrypoint` never run; the world map's Options row runs mode 0. With them goes WORLD's event interpreter: `script_start_current_event_thread`, the only starter of the event thread, is called by `menu_init_system_function` and by the menu-building thread that only BATTLE's `command_open_menu` starts (WORLD's copy is empty), so WORLD's `script_execute_event` never runs.
- Abilities no skillset lists: 0x16f (Attack), Reflect (0x1b8), Non-charge,
  Teleport 2, Move Underwater and Silent Walk. Four ids with no name text are
  played as Altima's teleport effects (0x28, 0xb8, 0xdb, 0xdc), and 0x200 is
  the follow-up strike that resolves a knockback.
- Skillset rows 0x86-0x8d, 0xa7 and 0xa8 have no name text but hold
  abilities; 0xa7 is the row the AI uses for a unit under Blood Suck, and
  0xa9, which it pairs with ability 0x16f, is empty.
- The ability names "Move in Water" (0x1f6) and "Walk on Water" (0x1f7) are
  the reverse of what the abilities do: 0x1f6 keeps the unit on the surface
  and 0x1f7 one level below it (`unit_get_effective_height`,
  `move_spread_store_unit_to_scratchpad`).
- Ability 457, named "Equip Knife", sets the Equip Katana support bit
  (`UNIT_SUPPORT_SET_1_EQUIP_KATANA`): the unit can equip katanas, not
  knives.
- Selling in the fur shop adds the sold copies to the poached stock
  (`fur_shop_sell_confirm_step`), so they can be bought back there.
- `fur_shop_buy_quantity_prompt` divides the gil on hand by
  `price >> 1` with no lower bound, while the sell side
  (`shop_get_item_half_price`) keeps half price at 1 or more.
- `move_step_start_teleport` builds a secondary-effect record in a
  local and never uses it.
- A change to a map CLUT row reaches VRAM on the second call of
  `battle_map_clut_upload_rows`: one call loads the 256x14 image to y 0x1ee,
  the next moves it to y 0x1e0. Rows 8 and 9 have state and are uploaded, but
  no 16-bit CLUT ID in the code addresses them.
- Rolls one off their round number: `random_battle_roll` tests
  `chance >= pick` over 0..99, so the retail chance byte 30 gives 31 in 100;
  the preferred-job promotion in `proposition_determine_success`
  passes on a roll below 0x3D, 61 in 100; `formula_apply_status_to_action`
  divides the displayed accuracy of separate statuses by four and keeps each
  status on a roll below 24; `unit_generate_party_record` and
  `unit_store_names` draw a generic name with
  `rand() * 255 / 0x8000`, 0-254, so the last of each list's 256 names is
  never drawn.
- `unit_apply_level_up_down_ability` stores the new level before
  calling `unit_apply_level_growth`, so an ability level-up divides each
  raw stat by growth + new level; an EXP level-up (`unit_check_level_up`)
  and the generated-unit replay in `unit_calculate_actual_stats` divide
  by growth + old level.
- `reward_find_war_trophies_and_bonus_money` computes
  `level_bonus_money` ((highest enemy level - lowest party level) * 100, 0
  once a player unit has fallen) and nothing reads it; REQUIRE pays
  `level_sum_money` and `bonus_money`.
- `formula_evade_apply_weather_on_bows` reads the weather variable
  itself, not `weather_get_effective`: the bow penalty of a storm
  also applies on a snow map and on a map flagged to ignore weather, where the
  elemental and movement rules see no storm.
- `ai_ability_load_entry`: with Throw Item the AI gives an
  item a range of the unit's Move; `target_range_set_for_action` uses
  4, and gives a Mime the same.
- `battle_map_gns_apply_command`: command rows 0x85, 0x86, 0x88, 0x89, 0x8a
  and 0x8b skip the map-state comparison and are applied in file order, so the
  last of a kind wins: MAP017.GNS has one set-outdoor row followed by six
  set-indoor rows and ends indoors. Row types 0x87 (49 rows in 14 maps) and
  0x8a (125 rows) have no case in `battle_map_gns_dispatch_resource`, and no
  retail GNS holds a 0x83, 0x84 or 0x8b row, the three that take arguments.
- Terrain data the code does not expect: five tiles hold a slope byte outside
  `battle_map_slope_type_e` (0x10 in MAP017.10, MAP043.9 and MAP078.9, 0x46 in
  MAP036.46, 0x01 in MAP045.7), and the terrain chunks of 37 maps arrive with
  mark bits already set in tile byte 5, which
  `battle_map_tile_copy_size_and_data` copies as they are. Tile flag 0x80
  (`cannot_stop`), which `move_range_set_tile_flags` tests, is set
  in no terrain chunk, and tile byte 1 is 0 in all of them.
- `g_battle_map_directory_path` is the string `C:\FFTIMG\MAP\GNS\`; every map
  file load passes it as the first argument, which
  `battle_map_load_start_file` ignores.
- The Brave Story "Fact" row never appears:
  `EVENT_SCRIPT_VAR_FACTS_ENABLED` (0x92) gates it
  (`brave_story_build_rows`), no code sets it, and the one
  event script that writes it, 0x075, stores 0 while setting facts
  0x1a4-0x1aa. The 24 fact names in WORLD.LZW are empty strings.
- `g_treasure_sound_novel_ids` is 0 for every treasure but 31
  (Germonik Scriptures, novel 5), so `brave_story_step_treasure_level`
  opens no reader for the four book treasures 17-20.
- World-map script opcodes 0x10 and 0x11 pass on a date on or after and on or
  before their operand (`location_script_opcode_10_check_date_on_or_after`,
  `location_script_opcode_11_check_date_on_or_before`); scenario condition types 16 and 17 are the reverse
  (`EVENT_SCENARIO_CONDITION_DATE_ON_OR_BEFORE`, `_ON_OR_AFTER`), although the
  war-funds and casualty tests beside them share their numbers.
- The Music Test names and comments (OPEN.LZW text sections 24 and 25, text
  ids 0xC000 and 0xC800 + entry) are not translated: each string is the
  Japanese release's glyph codes around Latin fragments such as the composer
  tags.
- SCUS_942.21 0x80028f94-0x80028fe7, between `g_suzuki_smd_modulator_waveforms`
  and `g_suzuki_smd_key_octaves`, is referenced by no instruction or pointer in
  any executable: a five-byte SMD sequence (`B0 91 7F 99 90`), ten 32-bit values whose
  last nine are the LIBSPU reverb work-area sizes of modes 1-9, and a
  0x23-byte `feds` sound bank with id 0x7000.
- Ability data the code and the tables disagree on:
  `target_area_set_for_ability` passes range-record `flags_2`
  bit 0x20 (`ABILITY_SECONDARY_FLAG_2_TOP_DOWN`) to
  `target_apply_vertical_tolerance` as `single_layer`, and none of the
  368 records sets it; the low nibble of
  `ability_data_t.type_flags` holds the ability type plus one (0 for ids 0,
  0x1fe and 0x1ff) and nothing reads it, because
  `ability_get_data_and_type` derives the type from the id.
- `status_init_check_data` builds eleven status check sets; sets 2 and 7
  (`STATUS_CHECK_SET_UNREAD_2`, `_UNREAD_7`) are read by nothing.
- WORLD's copies of BATTLE's action-menu and tile-info routines
  (`command_build_ability_list`, `command_build_act_menu`,
  `unit_view_tile_info_thread` and nine more) keep BATTLE's
  absolute data addresses 0x80173cbc, 0x8017403c, 0x80174048 and 0x8017404c.
  In WORLD's image those lie inside the text file embedded at 0x8016ee68, so
  the routines would read and write text bytes there.
- `g_terrain_geomancy_abilities` gives Gusty Wind for Sky and Kamaitachi
  for Darkness, Cross section and the unnamed surfaces 0x2f-0x3e; the terrain
  help text says "None" for Sky, Darkness and Cross section.
- `camera_rotation_update` chains a held R1 through
  `camera_rotation_set_left_destination`, as it does L1; the two helpers differ only in
  which way they move a destination that is a multiple of 0x400.
- `unit_copy_job_data` adds the immortal and formation immunity sets
  only to a unit that is both immortal and flagged save- or load-formation;
  the per-flag tests inside the loop are then always true.
- `formula_apply_status_to_action`: the
  `ACTION_STATE_PREVIEW` test in the separate-status loop cannot pass;
  any state but EXECUTE was switched to all-or-nothing above it.
- `loading_build_message` loads the NOW LOADING CLUT to VRAM (0x70,
  0x1e2), 16-bit CLUT ID 0x7887; in battle that is
  `BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_SPRITES, 7)`, the status bubble CLUT.
- The effect interpreter indexes fixed tables with wider fields and no check:
  the opcode is masked to nine bits over the 120 entries of
  `g_effect_script_instructions` (0x2e-0x77 report an error without returning
  a result: `effect_script_run_block` narrows the leftover `$v0` to `s16`
  and tests it while the script position stays unchanged);
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
  `deploy_select_first_deployable_unit` likewise loops forever when
  no roster unit is deployable for the squad, and
  `effect_particle_alloc_work_record`, with its free chain empty, calls the empty
  report stub and then reads the next link through NULL.
- `status_screen_thread` hands a unit with mount info (a
  rider or a mount) to DEBUGCHR.OUT's `debugchr_mounted_unit_entrypoint`
  (`status_screen_open_mounted_unit`); any other unit gets BATTLE's own
  panel.
- The camera's L2/R2 input uses `camera_zoom_toggle` and `camera_tilt_toggle`,
  bypassing `camera_zoom_start` and `camera_tilt_start`. The linear
  position, screen-offset, zoom and rotation countdown setters have no retail
  callers, so their per-frame update routines never move anything.
- `formula_49_unused_with_extra_steps` is a second entry to formula 0x49's
  handler, but neither the formula table nor any caller names it.
- `state_enter_effect_playback` and
  `state_enter_effect` are the only code that enters
  `GAME_STATE_EFFECT`; neither has a caller, so
  `state_handle_effect` never runs.
- WORLD's `screen_fit_parent_entry_to_digits` and
  `helpmenu_after_battle_entrypoint` have no callers; their quirks above do not
  execute in retail WORLD.
- `memcard_wait_for_hardware_event` is OPEN's only caller of
  `memcard_poll_hardware_event_result`, and nothing calls the wait routine.
- MAIN's Orbonne caption fades have no callers. `new_game_set_display_draw`
  is called only by `new_game_fade_in_orbonne_caption`, so it is unreachable too.
- `suzuki_sound_set_unread_value` stores a 16-bit sound-driver value that `suzuki_sound_init`
  and `suzuki_sound_quit` clear and nothing reads; nothing calls the setter.
- `system_zero_padding` (0x80010ba8), which `overlay_exec_battle`
  reaches after loading BATTLE.BIN, is not BATTLE code: it is the 4 KB of zero
  words after MAIN's C start-up, run as 1,024 no-ops up to the `jr ra` at
  0x80011ba8. BATTLE runs when `system_run_game_loop` then calls
  `battle_entrypoint`.
- `boot_run_startup` installs the empty `file_handle_cd_ready`
  and `file_handle_cd_read`; the `CdInit` it runs next replaces
  them with LIBCD's `def_cbready` and `def_cbread`, so neither ever runs.
- `FlushCache` is called only by the BIOS patches (`_patch_card`,
  `_patch_card2`, `_patch_gte`, `psyq_card_restore_exception_prefix`); no
  overlay load flushes the instruction cache before the new code runs.
- `_spu_FsetDelayR` sets the SPU bus timing for DMA writes and
  `_spu_FsetDelayW` for reads, and `_spu_t` and `_spu_Fr_` call them that way:
  each suffix names the opposite direction.
- `SpuQuit` closes the SPU DMA event before disabling it, so `DisableEvent`
  gets a closed descriptor.
- `effect_update_stage`: nothing stores playback phase 5 or
  `EFFECT_PLAYBACK_PHASE_RESTART_SCRIPT` (6), nor the two start pointers the
  restart arm rewinds to (`g_effect_frame_data_start`,
  `g_effect_animation_sequences_start`), so the restart path never runs;
  phase 5 has no case and would return `result` unset.
- `ai_target_is_untargetable`: the AI never targets a Transparent enemy
  unless it is a rider whose mount is Transparent too.
- World-map script opcodes 0x22-0x24 (`location_script_opcode_22_request_draw_location`,
  `location_script_opcode_23_request_erase_location`, `location_script_opcode_24_focus_location`)
  read their operand without advancing `ip`; each ends the block, so no opcode
  is read from the operand.
- `move_find_get_result` repeats the same `unit_find_at_tile`
  call up to 21 times: its occupant loop never uses the counter.
- `turn_start_next_event` calls
  `unit_store_team_flags` twice, around
  `command_reset_unit_record`, which leaves team flags alone; the first call
  reads `unit->stats` before the null test that guards the second.
- `effect_secondary_start_projectile` tests Throw Shuriken (382)
  on its own before the Item/Throw range 368-393, which already contains it.
- `formula_steal_exp_hit_sp_x_percent` (formula 0x28) calls
  `formula_set_exp_stolen` after a failed hit test whenever the action
  is not executing; outside execution only a 0% chance misses, so the preview
  shows 0% with an EXP amount. Formula 0x27 (steal gil) fills nothing then.
- `effect_particle_spawn_from_emitter`: an emitter with no velocity-spread
  curve uses half its start spread, but with a curve it uses the lerped spread
  whole, so a curve at factor 0 gives twice the angle range of no curve. The
  other spawn values use the start value unchanged when no curve is bound.
- `battle_map_texture_update_animations`: the two loop modes (1 and 3) add 1
  per frame and step when the timer reaches `duration`; the other modes add
  `g_system_vsync_interval` and step when it exceeds `duration`, so one
  duration means `duration` frames in a loop and over `duration` vertical
  blanks otherwise.
- Lookups whose result is discarded:
  `cursor_show_right_unit_panel`,
  `state_handle_resume_attack_phase` and
  `state_handle_change_turn` call `unit_get_source`;
  `state_handle_targeting_range` (twice),
  `unit_get_tile_center_and_height_by_battle_id` and
  `camera_calculate_cursor_tile_vector_normal` (the other level's tile)
  call `battle_map_tile_get_pointer`; and
  `unit_facing_draw_wait_direction` runs a `RotTrans` of the unit's feet
  that the next `RotTrans` overwrites.
- `equipment_update_item_actions` tests `g_item_action_menu`'s
  `selected_index` for the sort-order row (2) even while the single-item menu
  runs; that index was zeroed when the menu opened, so with one item CIRCLE
  plays the invalid sound on every row. The single menu's row actions agree:
  row 2 has 0x13e8 like rows 0 and 1, where the full menu has -1.
- `unit_list_gfx_init_buffers` and
  `screen_init_packet_buffers` set their draw and display
  environments 255 pixels wide where MAIN and CARD use 256, so the default
  drawing area ends at x = 254; `PutDispEnv` still picks the 256-pixel mode.
- Soldier Office: the hire menu shows 1500 gil for row 0 and 1400 for row 1
  (`soldier_office_get_fee`), but
  `soldier_office_menu_step` tests, prompts with (0xF82A)
  and charges 1000 plus the generated recruit's equipment prices.
- `screen_text_measure_width` adds the operand of 0xE7
  (`TEXT_SET_X_POSITION`) to the width as an advance, while
  `menu_text_render_encoded_ids_to_image` moves the pen to that x. Its only
  caller centers the job wheel's job name.
- `zodiac_build_sprite_polygon` (MAIN, ATTACK) and
  `gfx_set_scaled_poly_ft4_8bpp` (WORLD, CARD, DEBUGCHR, REQUIRE) round a
  scaled offset or size by testing the C remainder against half a pixel, so a
  negative product truncates toward zero: -1.75 pixels gives -1 where 1.75
  gives 2.
- `file_poll_load` and `world_map_poll_texture_stream` count failed
  seeks, refused reads and stalls in `error_count`, which nothing reads, so a
  disc error restarts the read indefinitely. `file_poll_load` also counts
  `wait_frames` unread; its stall timeout uses `g_system_session_frames`.
