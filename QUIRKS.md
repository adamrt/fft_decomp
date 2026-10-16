# Target quirks

Non-obvious facts about the retail code. They explain source that looks wrong
and mark code that a cleanup must not "fix". Details live in the named file.

## Retail bugs the source reproduces

- `battle_unit_calculate_height_data`: the seven-byte result preserves an uninitialized elevation high bit; an unavailable unit leaves the other result fields unset.
- `DrawOTagEnv`: the diagnostic string uses literal `&08x` for its second item, so it does not format the environment pointer.
- `StCdInterrupt`: without `CdlModeSize1`, it skips the local disc-position read but still copies the uninitialized four bytes into the ring descriptor.
- `sprintf`: unchecked precision and zero padding can overrun its 512-byte temporary segment buffer.
- `FntPrint`: unsupported conversions leave the field length undefined; the count limit is checked after writing, and oversized hexadecimal widths can overrun the 512-byte temporary buffer.
- `DecDCTvlc`: the pause path saves its Y predictor at `0x80073f34`, overwriting the first four bytes of the overlapping birthday month-length table. Nothing calls `DecDCTvlcSize`, so the limit keeps its initial 0xffffff halfwords and the pause path never runs.
- EFFECT native renderers: `battle_effect_step_emitter_timeline` sets phase 3 when a callback's keyframe window has two frames left, even if init never ran; the destroy path then reads its latch through the null work slot (kernel RAM).
- `camera_store_state_to_script_variables`: all three zoom
  components are stored to 32-bit script variable `0x20`, so the first two are lost.
- `script_focus_speed` accumulates into an uninitialized `$s6`: the event
  dispatcher leaves `operand_3` there, while camera fusion leaves its key count.
  Those caller values affect the camera duration.
- `script_toggle_message_portrait_flip` (BATTLE and
  WORLD): the record index is never
  assigned; all six passes use whatever `$s0` held. Keep the local
  uninitialized.
- `wldcore_proposition_determine_success`: the preferred-job
  check resolves the party unit once, before its loop, from the scoring loop's
  leftover (past-the-end) index, so it tests that one unit every pass.
- `battle_map_calculate_slope_height`,
  `battle_gfx_calculate_world_height_from_coords`: an off-map point
  (a unit staged outside the map, its world coordinates read unsigned) gets no
  tile, and the null tile is read from RAM address 0. The unit shadow, palette
  modulation and per-frame rotation code (`battle_gfx_draw_unit_shadow`,
  `battle_gfx_calculate_sprite_shadow_from_tile_slope`,
  `battle_gfx_apply_unit_palette_modulation`,
  `battle_gfx_project_all_units`) do the same for units
  that chapter-end cutscenes walk off the map.
- `battle_move_animate_fall_to_target_tile`: the event-state
  (`0x34`) path writes the tile-center depth into `real.vy` (not `real.vz`).
  The first frame adds the fall speed to it, which puts the unit below the
  ground, and the next frame snaps it to the ground: an event fall ends on its
  second frame.
- `battle_map_light_state_command`: several arms return an
  uninitialized pointer.
- `menu_preview_attack_caster_stats`: for an item the
  secondary-data pointer stays uninitialized, yet its `flags_3` is read before
  the item test.
- `battle_map_dispatch_gns_resource` passes the packed lighting colors as a
  `MATRIX` to `battle_map_set_light_matrices`, which also copies its 32-bit translation
  values from the following 16-bit direction components.
- `battle_map_load_alternate_mesh`: scales the mesh header's color
  palette byte offset by four, placing the palette read past most mesh files.
- `map_queue_textured_triangles`, `map_queue_textured_quads`
  and their untextured twins pack SXY with the full signed 32-bit MAC1 value; a
  negative X therefore forces the 16-bit screen Y to `0xffff`.
- `battle_map_blend_ambient_light_color`,
  `battle_map_blend_darkness_color`: modes `11` and above leave the
  target color uninitialized (no default case).
- `battle_ai_load_ability_entry`: the CT adjustments test the
  Monster Skill (`0x80`) and Defend (`0x40`) support bits; Short Charge and
  Non-Charge were presumably intended.
- `battle_ai_evaluate_status_cancellation`: the Blood Suck branch
  tests decision bit `0x04`, which is built from Jump.
- `battle_ai_build_monster_skill_tile_mask`: the `-1` neighbor
  offsets are loaded unsigned (+255), so only `+x`/`+y` survive, and the level
  loop runs for level 0 only.
- `battle_ai_build_targetable_tile_mask`: radii 8–15 index past
  the 8-entry template table into the following status weights. Jump Level 8
  can reach this path with a non-arcing weapon.
- `battle_ai_evaluate_self_target_origins`: suspension during the
  first origin's target search marks the second-origin stage, so resuming skips
  the moved-origin pass.
- `battle_ai_evaluate_reflected_target_origins`: the resume entry
  skips the reflector pointer reload and depends on the caller's `$s2`.
- `battle_ai_evaluate_math_targets`: the extra known-ability call
  in the special-ID loop discards its result.
- `battle_camera_step_real_coords_toward_target`: in the positive
  direction the Y (`vz`) step adds the vector component twice; X and Z add it
  once.
- `world_menu_open_entry_window` receives requested x/y origins but always
  opens at its fixed default origin.
- `battle_gfx_build_next_action_result_display`: the CT-zero
  display case reads `y_shift` without setting it; only the HP/MP and
  stat-change cases assign it, so the sprite row depends on the leftover value.
- `battle_gfx_step_queued_unit_graphics_load` replaces sheet ids from 0xa0
  with Ramza's chapter 1 sheet, but the table has 159 sheets (0 to 0x9e), so
  id 0x9f reads one entry past it. `battle_gfx_queue_unit_graphics_load` tests
  against the count correctly.
- `battle_effect_spell_charge_secondary_handler` saves the inertia threshold
  and sets the secondary-effect value but never restores it, unlike the other
  secondary handlers.
- `main_unit_calculate_random_equipment`: throwing and bomb item
  types index past the four equipment-category bytes into the first innate
  status byte; that adjacent byte controls their eligibility.
- Both copies of `menu_text_render_encoded_ids_to_image`: the encoded-text branch tests
  its glyph counter (`col`) against the line width but never assigns it; the
  stale register stays below the width, so text never wraps.
- `battle_unit_init`: tints the new unit's palette
  before it clears `mount_state`, so a runtime slot last held by a rider looks up
  a mount that is gone and reads its world coordinates through NULL (kernel RAM).
- `menu_display_projected_action_effect`: a skipped
  preview leaves `g_menu_preview_target_action` null, and its
  `attack_accuracy` is read (from kernel RAM) before the null test.
- `world_formation_rebuild_unit_list`: the Soldier Office (shop
  0x65) lists only monsters, yet `world_formation_init_menu_state` stages unit
  0 even when that list is empty; with no earlier list since WORLD loaded, the
  pointer is 0, so the "restore HP/MP" stores copy kernel RAM 0x10/0x16 to
  0x0c/0x12.
- `battle_ai_choose_wait_facing`: when the target shares the
  acting unit's tile, `battle_ai_find_direction_of_target` returns
  `AI_DIRECTION_OVERLAP` (4), which indexes `viability.by_direction[4]`: the
  never-written `past_viable_directions` byte after the array holds whatever
  the stack held and decides whether 4 is returned as the facing.
- Both copies of `menu_run_scrolling_ability_list_thread_variant`: `SetSemiTrans(&frame, 1)` passes the
  address of the `frame` pointer, so the semi-transparency bit lands in a stack
  byte past the pointer and the sprite is unchanged.
- Unchecked divisions whose divisor can be zero; the R3000 `div` does not trap
  and leaves quotient -1 (1 for a negative dividend) and the dividend as the
  remainder: `100 / ct` in both copies of `menu_ability_get_ct_display_value` (CT 0 displays -1 + 1 = 0);
  `0x20000 / battle_lerp_fixed8(...)` for the wave steps in
  `effect_wave_mesh_update` (E033, E035, E073, E079, E080, E230, E453, E456);
  and the direction-times-spread products over `scale` in
  `battle_effect_spawn_particle_motion`.
- Locals read before any assignment, so the value is whatever the caller left
  in a register: `battle_effect_update_file_load` tests `result` (`$a0`) for a load
  state outside 0-3, which nothing sets; `battle_camera_get_input_direction`
  tests `input` (`$a1`) when `mode` or the controller mode is outside 0-2;
  `main_status_set_action_state` leaves `status` unset for an action state
  other than 0 and 5-8 (its `switch` has no default; no caller passes one);
  `battle_map_dispatch_gns_resource` returns a local no path assigns (`$s8`);
  both copies of `menu_get_cursor_bob_offset`
  walk an unset table pointer for a mode other than 0 or 1; `battle_effect_summon_charge_secondary_handler` returns
  `result` (`$a1`) unset when the phase is not 1-3, as does
  `battle_effect_linear_projectile_secondary_handler`;
  `bunit_unit_get_current_stat`, `bunit_unit_get_max_stat` and
  `world_get_formation_unit_stat_by_mode` return `value` (`$a1`) for a stat
  mode outside 0-2; `battle_move_start_unit_step` leaves `facing` (`$a2`, the
  destination tile its callers pass) unset for a direction outside 0-3;
  `battle_map_load_mesh_animation_record` returns `next` (`$a1`) for record type
  0; `world_formation_run_set_ability_menu` indexes the entry ids with
  `cursor` when the list is empty; `world_menu_build_location_list`
  uses `selected` when no entry matches the current item;
  both copies of `menu_run_battle_help` read
  `node`, `x` and `y` in the cursor loop when the text thread is already
  running on the first frame, and `navigation` when the unit view mode is
  not 1-4; `menu_init_system_function` passes `ptr` on unset for
  SCENARIO_CHECK and CLOSE; `battle_gfx_init_numeric_sprite_display` leaves
  `display` unset for a section outside 0-2 (its caller passes 0-2);
  `battle_map_update_mesh_part_animations` indexes the instructions with
  `next`, assigned only when a keyframe expires in the same pass, for a part
  whose animation state is set and whose keyframe is 0;
  `battle_map_control_gte_background_color` returns `result` unset for DISABLE
  and RESTORE (every caller issues SET); `card_io_get_info_with_retries_variant`
  returns `result` unset for a retry limit below 1 (its caller passes 2);
  `open_card_poll_info_until_stable` has the same empty-loop result for a
  nonpositive retry limit (its callers pass 3);
  `battle_effect_select_sound_id` returns `sound` (`$a2`) for a channel mode
  above 4; `wldcore_proposition_select_report_speaker` returns `selected` for a
  speaker mode outside 1-4 when more than one unit went;
  `battle_sound_stop_weather_sfx` returns an unset `result` no caller reads.
- `world_card_read_file_chunked` sets `result` to -1 when the last read was
  short and then returns `total`; the test has no effect.
- Both copies of `menu_run_require_help` reach
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
- `etc_graphic_build_game_over_polygons`: `texture_mode` is the constant 1,
  so the 4-bit CLUT arm never runs.
- `open_movie_init_mdec_stream_state` stores the slice width (24) into the
  global `g_open_movie_mdec_stream_state.output_rect.w`, not through its
  `state` parameter; the one caller passes that global.
- `battle_action_init_current_ability_strike_data` tests
  `ability_id >= 0 || ability_id < ABILITY_ID_ITEM_FIRST`, which is always
  true.
- `main_unit_calculate_entd_data` copies ENTD bytes 0x21-0x27 with one
  seven-byte loop over the two-byte `ai_target_xy`, reading on through the AI
  flags, the target unit, the two AI targeting flags and
  `ai_initial_enemy_flag`.
- EQUIP never fills the physical accessory evade slot of `equip_unit_data_t`
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
  - WaitUnitReady waits on `battle_return_one`, so it never waits.
  - Effect stores its x and y operands to two globals nothing reads.
  - DebugPause prints "PAUSE", yields one frame and sets a 16-bit value nothing reads
    (`script_print_debug_message`); instruction 0xdc stores 13 to
    another. Neither changes the script's course.
  - In WORLD, WalkTo is an empty arm outside a block, Arrow resolves both
    units and never sets the trajectory, and ChangeMap calls an empty
    `map_reset_script_light_transform` where BATTLE resets the script light
    transform. WORLD's interpreter never runs (see Other surprises).
- `wldcore_menu_step_location_menu_level` opens the outfitter with shop id 0
  for every town; `world_shop_run_screen` then takes the id from the
  location variable. The fur shop and soldier office pass their own ids.
- `wldcore_proposition_step_result_level` clears the in-progress bit with
  `& 0xFFFD` on an `s32`, which also drops the variable's high half.
- OPENING's runtime flags 8, 0x20000 and 0x80000 are tested and never set.
- Suzuki driver: music status bits 0x20 and 0x2000, channel flag 0x10 and
  driver flag 0x2000 are tested and never set; channel flags 0x80 and 0x4000
  and voice-attribute bits 0x7000 are set and never tested;
  `suzuki_smd_set_flags_all_channels` and `suzuki_smd_dispatch_snapshot` have no
  caller.
- CARD save menu state 9 is compared and never stored. EQUIP refuses command
  row 1, so its screen state 3 is never entered. `g_etc_graphics` entries 0, 5
  and 6 all name CHAPTER1.BIN.
- `world_script_reset_event_variables_thread` does `g_main_system_flags += 3`
  on a 32-bit flags value. System flag bits 0x80, 0x400000 and 0x20000000 are
  tested and never set.
- `thread_resolve_id` searches slots 1-16 where BATTLE searches 1-15;
  WORLD's slot 16 (tutorial text) lies beyond the sixteen the reset clears.
- `card_save_run_menu` and `world_card_run_menu_screen` end with
  `thread_set_parameters(THREAD_BORROWED_SLOT_CARD_BACKDROP, 0, -1, 0)`: the
  backdrop thread exits only when its third parameter is set, so that call
  does not stop it. CARD also tests and stops slot 14 and suspends slot 15,
  which nothing in the overlay starts.
- `wldcore_menu_step_map_path_level` plays sound id 0 when the path
  animation ends; that entry has no streams, so it only stops the channel
  pair.
- `script_check_scenario_condition`: conditions 18 and 19 (casualties) compare
  with the record's 16-bit `id`, not `value` as the war-funds pair does.
- `attack_load_scenario_conditionals` fills deployment squad ids 0x35/0x36 and
  clears 0x37/0x38, although the deployment screen handles four squads.
- 32-bit variables 0xab-0xff of the script variable block are reachable by no variable
  id, yet variable 0xff gates the BATTLE call in
  `main_overlay_call_battle_return_zero`, which itself has no caller.
- Formation menu states 22 and 23 have entries in
  `g_world_formation_menu_handlers` and code paths, and no code stores them,
  so `world_formation_step_unit_portrait_slide` never runs. Menu input mode 1
  is tested in four places and never stored.
- Both copies of `menu_wait_thread` store -2 over the
  Wait entry's row action (7, `BATTLE_MENU_ID_WAIT_YES_NO`) before confirming,
  so the Yes/No entry of the menu table never opens. No code, row action or
  system function opens entries 0x09, 0x0d, 0x16, 0x17 (the only way to 0x18)
  or the four no-act turn menus (0x20, 0x21, 0x32, 0x33), yet the confirm-move
  threads reject Move for 0x21 and 0x33.
- `world_item_build_category_list` and `world_shop_build_item_list` treat
  filters 0 and 1 (the two hand slots) identically.
- The fur shop's "sell" list (`world_shop_run_fur_sell_list_step`) sells
  ordinary inventory at half price; its "buy" list
  (`world_shop_run_fur_buy_list_step`) is the poached stock.
- `main_unit_learn_job_abilities` sets the learned bit twice when the JP
  purchase succeeds: once in the purchase branch and again under
  `if (learned)`.
- `main_unit_apply_level_growth` calls `rand()` and discards the result; the
  call only advances the generator.
- `battle_gfx_load_unit_attack_image_into_evtchr_slot` searches
  `g_battle_gfx_vram_slots[0..1]` for a free load marker and decompresses into
  that slot's image data, but writes the owner, marker and state of the
  caller's slot.
- `require_party_update_after_battle` searches the formation sprite groups
  for the unit's row, remaps 0x49 to 0 and 0x47 to 0x48, and never uses the
  result.
- Both copies of `menu_run_battle_help`: the
  `UNIT_VIEW_MODE_HOVERED_UNIT` arm and its navigation table cannot be reached
  (the first arm, which closes the help, takes modes 1 and 2); request
  kinds 4, 0x11, 0x12 and 0x17 call the close routine for an empty id and
  then start the text thread anyway, relying on close not returning.
- `main_heap_free`: the clearing loop stops at the first page byte that differs
  from the allocation id, not at the end of the 64-entry
  `g_main_heap_game_allocator_table`; an allocation that reaches the last page
  reads on into `g_main_gfx_draw_envs` and clears its bytes while they match.
- `battle_map_load_data`: the four texture quarter files load at
  `buffer + 0x8000 * (n + 1)`, while `battle_map_dispatch_gns_resource` uploads
  quarter n from `buffer + 0x8000 * n`; quarter 3 is read past the end of the
  0x20000-byte buffer.
- `battle_camera_toggle_tilt`: from tilt target 3 it starts a decrease toward
  target 4, which `battle_camera_update_tilt` has no branch to finish.
- `battle_action_run_pre_formula_setup`: both tests bound the default
  ability class at `ABILITY_ID_ITEM_FIRST + 1`, so the Potion ability indexes
  `g_main_ability_range_data` one record past its end, which is the bytes of
  `g_main_item_ability_item_ids`. The other callers bound it at
  `ABILITY_ID_ITEM_FIRST`.
- `battle_reaction_prepare_next`: Counter Tackle performs Dash under the
  Black Magic skillset, the skillset Counter Magic passes.
- `battle_formula_apply_elemental`: Oil on a fire-element action doubles XA
  after every caller has stored the HP damage, and nothing reads XA again: Oil
  is removed and the damage is unchanged. The weapon-element path
  (`battle_formula_modify_elemental_damage`) does not test Oil at all.
- `wldcore_proposition_determine_success`: the level score is
  read from the bravery scores (proposition data record 5, three rows of
  five) with a ten-entry stride, so it runs on into the faith scores (record
  6). Record 7, three rows of ten level scores, is fetched by nothing: with
  job list 1 a level 91-99 unit scores 5 where record 7 holds 50.
- `battle_ai_add_usable_skillset_abilities`: the Faith test that
  skips abilities for a unit with innate Innocent reads the AI workspace's
  considered ability (whichever was loaded last), not the ability being
  added. The bare Jump entry for a unit with no level-jump ability is written
  without being counted, so the caller's next entry overwrites it.
- `battle_target_validate_weapon_target`: the weapon-range flag
  is read from `g_main_ability_range_data[(u8)ability_id]`, so abilities
  0x100 and up use the record of their id's low byte.
- `battle_unit_apply_level_up_down_ability`: a level-1 target of a level-down
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
- Both copies of `item_sort_id_list_variant`: the evade
  keys read `g_main_item_shield_data` for ids 0x7a-0x8f and
  `g_main_item_accessory_data` for 0x90-0xef by `secondary_data_id`, so thrown
  weapons (122-127) read far past the 16 shield records, and headgear and body
  armor (0-63) read the 32 accessory records and on past them.
- `world_item_cash_out_excess_inventory` walks ids below `ITEM_ID_END - 1`, so
  Phoenix Downs (0xfd, the last item) over 99 are never cashed out; every
  other item walk runs to `ITEM_ID_END`.
- `battle_move_calculate_pathing` copies 256 terrain costs from row 0 of
  `g_main_terrain_movement_cost_tables` (64 a row) to the scratchpad cost
  block at 0x1f800100, on over the spread state at 0x1f800180. Its flag-0x100
  test (a WalkTo with terrain costs disabled) compares the cost-table index,
  not the cost, with 0xff, so blocked surfaces cost 1 too.
  `battle_move_store_unit_movement_to_scratchpad` copies 64 and keeps 0xff.
- `battle_formula_weapon_heal` (formula 0x07, the Healing Staff) applies
  zodiac compatibility twice, once itself and once inside
  `battle_formula_apply_physical_modifiers`, whose Defense
  Up and Protect steps also cut the healing to two thirds.
- `battle_formula_raise_level_by_one_add_status_on_caster` (formula 0x57,
  Please Eat) stores a full HP and MP heal (max-HP damage for an undead
  target) and then overwrites `attack_type` with the level change's:
  `battle_action_apply_target_result` still applies the
  amounts, but `battle_gfx_prepare_post_action_display` shows no number.
- `main_sound_unload_scenario_mus` releases a slot's music record and SMD
  without clearing `g_main_sound_music.state` when that slot is the playing
  one. The shop, formation and both save screens unload their own music this
  way, so the next `main_sound_stop_current_music` hands the released record
  to `suzuki_smd_stop_music`.
- `battle_map_stop_clut_cycle` clears the active byte of slot `handle`,
  but `battle_map_start_clut_cycle` returns slot + 1: it stops the slot
  after the handle's own, and handle 8 clears the byte after the eight slots,
  the first of `g_battle_gfx_cycled_deployed_palettes`.
- `battle_gfx_queue_unit_graphics_load` writes its record before it
  tests the count, which stops at 16: a seventeenth append lands one past the
  16-entry queue, on the head of `g_main_zodiac_primitives`.
- `battle_effect_remove_secondary`: removing a record from the middle of the
  active chain leaves the record before it with `next_slot_id` naming the freed
  record. When that one is removed while a newer effect is still active, it
  patches the freed record's link (by then a free-chain link) and its real
  successor keeps naming it as the previous record.
- `main_heap_alloc` tags a new block one above the highest tag met below it, so
  it can repeat the tag of the block directly above (free pages 0-3 under a
  block tagged 3: allocating 1, 1 and 2 pages gives tags 1, 2, 3).
  `main_heap_free` tells blocks apart by tag alone: freeing the lower block
  frees both, and freeing the upper one returns 0 and frees nothing.
  `main_heap_reserve_at` picks an unused tag but bounds-checks only the starting
  address, allowing the requested page range to extend beyond the heap.
- `main_unit_calculate_entd_equipment`: the random body-armor roll tests
  `BATTLE_SUPPORT_SET_1_EQUIP_SHIELD` before asking for `ITEM_TYPE_ARMOR`;
  Equip Armor is tested only for the helmet.
- `battle_target_clear_panels_on_untargetable_tiles` (ability ranges) visits
  only the 256 lower-level panels, so an untargetable upper-level tile keeps
  its range; the weapon-range twin
  `battle_target_clear_range_on_untargetable_tiles` visits all 512.
- `battle_unit_settle_on_tile` and
  `battle_unit_restore_facing_after_attack`: for a mounted pair the second
  `battle_unit_set_tile_position` call names the unit's own battle record again
  (the first function finds its "mount" by the unit's own `unit_id`), so the
  partner's battle stats take no tile or facing here. The second function also
  writes the partner record's facing with no null test.
- `battle_gfx_release_reserved_vram_slot` tests owner bit 0x80
  (`BATTLE_GFX_VRAM_SLOT_OWNER_ATTACK_IMAGE`), which the EVTCHR (0xfe) and free
  (0xff) markers also carry, so the event instruction releases a saved EVTCHR
  slot too.
- `battle_effect_draw_projectile_model`, the Reflect shield: the depth is
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
- Both copies of `menu_build_zodiac_primitives`:
  the round-to-nearest step of the sprite scale adds 1, not `ONE`, to the 4.12
  product before dividing, so it only rounds up a remainder of `ONE` - 1.
- `battle_state_handle_open_sp2_files_state` frees its 0x8000-byte buffer when
  `battle_gfx_open_sp2_file` returns 0, which means the read has started, and
  keeps it on 1 (loader busy). The next frame allocates again and has its
  second request refused, so the file arrives in the kept block only because
  the heap hands the freed pages back.
- `wldcore_menu_step_treasure_detail_level` and
  `wldcore_menu_step_unexplored_land_detail_level` wait, in their LOAD phase,
  on the render record named by 32-bit field at level offset 0x08 (`saved_record_index`). Their
  pushes never set it: it holds what the last level at that stack depth left.
- `battle_effect_set_secondary` (fall dust, splash, level up, venom trap,
  zodiac poof, death smoke, teleport) builds its init record in a local of
  which `battle_effect_build_secondary_init` sets five fields;
  `battle_effect_init_secondary` copies the rest of the two ten-byte blocks and
  `used_weapon_id` as the stack left them.
- `battle_gfx_draw_unit_shadow` sets bit 0 of `shadow_dirty` before testing it,
  so the shadow quad is recalculated on every call and the dirty bit the
  movement code sets gates nothing.
- `battle_formula_damage_pa_times_wp_plus_y_status` (formula 0x2D, Holy Sword)
  runs Strengthen and the target's affinities on the weapon's elements only
  (`battle_formula_apply_weapon_element_strengthen`,
  `battle_formula_modify_elemental_damage`): the Holy element in the range data
  of the five Holy Sword abilities (0x9b-0x9f; 0x9e adds Lightning) is never
  applied.
- Half of MP: `battle_menu_get_unit_skillset_ability_data` lists the halved
  cost rounded up and `menu_build_ability_list` grays an ability with a nonzero
  listed CT whose listed cost exceeds the unit's MP, while
  `battle_action_check_and_consume_mp` and the AI (`battle_ai_load_ability_entry`,
  `battle_ai_add_usable_skillset_abilities`) round down. A unit holding exactly the rounded-down half sees an odd-cost
  ability grayed out although it could pay for it.
- `wldcore_calendar_advance_days` (the scripted multi-day advance) disagrees
  with the daily `wldcore_calendar_advance_day`: an open proposition that does
  not finish within the advance gains no elapsed days, a birthday reached only
  after the advance passes December 31 is missed, and script variable 0x67
  counts the year when the date passes December 31 instead of on March 21,
  with no cap at 99.
- `require_overlay_open_jobstts` reserves 0x1f000 bytes at the high overlay
  address (0x801df000), but the companion loader reads the 0x20000 bytes that
  `g_menu_companion_executable_sizes` gives JOBSTTS.OUT, so the top two heap
  pages (0x801fe000-0x801ff000) are overwritten without being reserved.
  BATTLE's own `menu_run_jobstts_thread` reserves 0x20000.
- `battle_move_update_walking_step_at_tile_edge` adds the destination tile's
  water lift to the current edge height rather than to the destination's, so a
  step up onto deep water can pass the height test as a plain walk.
- `battle_state_handle_change_map_jumping_in_state` sends event result 9 to
  BATTLE and 10 to WORLD, the reverse of `battle_state_update_event_mode`, and
  ignores results 11 (reset) and 19 (ending).
- `battle_target_set_state_for_all_unit_panels` is meant to mark the panels
  under allied or enemy units, but its loop over the 21 slots never advances
  the unit pointer, so it marks only the caster's own panel. None of the 368
  ability range records sets either flag that calls it.
- `battle_ai_build_unit_ability_list` ORs the formula kinds of both hands'
  weapons and then switches on single values, so a unit holding two kinds (an
  HP-absorbing weapon beside a damaging one) gives its Attack entry no target
  team.
- `world_menu_icon_clamped_list_thread`: Up steps the cursor up, but the Down
  test is inverted, so the cursor moves down on every frame Down is not held
  until it reaches `max_row_index`. Nothing refers to the thread.
- `menu_build_and_upload_window_frame_image`: the banded upload (mode
  `MENU_WINDOW_FRAME_UPLOAD`, an image of 0x3000 bytes or more) starts a band
  when `(i & 0x5f) == 0`, at rows 0x20, 0x80 and 0xa0 rather than every 0x60
  rows, and its last-band test `i - height >= 0x60` is never true. After the
  band at row 0x20 the rows are written before the start of the buffer. No
  retail window is known to be that large.
- `main_unit_calculate_random_equipment` restarts its candidate list when an
  item with a higher required level appears, but items scanned after it with a
  lower level still join the list, so the pick is not limited to the best
  level reached.
- `battle_status_queue_runtime_graphics_flag_change`: removing Crystal shares
  the Chicken case, so it queues the Chicken display bit for removal and leaves
  the Crystal bit set.
- `battle_formula_apply_zodiac_compatibility` returns early when either unit is
  Serpentarius, skipping the floor of 1 at its end: an XA already cut to 0 (a
  Frog's XA of 1 against Defense Up or Protect) stays 0, and the hit deals 0
  damage where any other sign pair deals 1 * YA.
- DEBUGCHR's unit editor (`debugchr_editor_run_session`) lets the job, the five
  equipment slots (Triangle empties one) and the five skillset and ability
  slots be changed, but `debugchr_editor_apply_values_to_battle_unit` writes
  back only HP, MP, CT, level, EXP, Brave, Faith, zodiac sign, Move, Speed and
  Jump, and `menu_load_selected_unit_variant` reloads the unit after every
  change, so the other edits are lost at once.
- `wldcore_script_calculate_save_data_completion_percent` (no caller) counts
  0x1000 bits from `g_main_saved_data_bits`, which holds 1,280 (40 words), so it
  also counts `g_main_item_location_flags` and the first 224 bytes of
  `g_main_item_poached_quantities`.
- `open_movie_update_stream_frame` restarts a stream whose ring has fewer than
  16 free sectors through `open_movie_start_cd_stream_read`, which reads with
  mode 0x1a0; the first read in `open_movie_start_stream` uses 0x1e0, so the
  restart drops `CdlModeRT` (XA-ADPCM playback).
- `main_ability_store_skillset_abilities` bounds the support class with
  `<= ABILITY_ID_MOVEMENT_FIRST`, so Move+1 (486) is filed as a support ability;
  every caller asks for action abilities or for all four classes, so nothing
  sees the difference.
- Tests that cannot succeed: `battle_state_sync_frame` zeroes a
  `g_main_system_min_vsync_interval` of 1 under `wait < g_main_system_min_vsync_interval` with `wait` at least 2;
  `battle_state_run_deployment` loops until
  `battle_state_update_deployment_controller_input` returns -1, and it always
  returns 1; the first `switch` of `battle_move_apply_unit_step_velocity`
  repeats the layer test that the statement before it has just made false;
  `battle_status_calculate_turn_flags` cannot report Sleep, whose
  `g_main_status_effect_data` record carries the freeze-CT flag tested first;
  `battle_unit_set_mounted_animation` tests a depth masked to a
  byte for `< 0` and a nonzero animation for 0; `menu_get_thread_activity_variant`
  tests `i != 3` inside its scan of threads 4-9;
  `battle_camera_rotate_when_unit_tile_not_visible` adjusts a negative
  `direction` after masking it to 0-0xfff; selector 15 of
  `battle_map_set_tile_data_value` retries `battle_map_tile_data_15_stub`,
  which returns 0 at once, up to 1000 times before load exception 0x80.
  `battle_gfx_run_unit_seq_script`, `battle_gfx_run_wep_eff_seq_script` and
  `battle_gfx_load_trap_and_unit_frame_parts` test sequence and frame pointers
  for -1, which the unpackers replace with the start of the data, and the last
  tests a three-bit part count plus one for `> 8`. The weapon interpreter's own
  bounds (`sequence_id > 0x100`, `frame > 0x200`) admit the index one past each
  table. `battle_ai_select_action_autobattle` falls back from the recovery
  policy to retreat on a -2 result, which `battle_ai_select_action_recovery`
  never returns (only 0 or -1); `battle_action_run_between_turn_events` has a
  case for turn event 0x400 (`BATTLE_TURN_EVENT_UNKNOWN_0400`), which its only
  source, `battle_action_run_between_turn_control`, never returns;
  `menu_run_icon_selection_loop` clears both icon records' `_unknown_78` when
  one holds 0 and the other 2, but their only writer,
  `menu_build_window_sprites`, stores 0.

## Calls that disagree with the callee

Where callers pass arguments a callee never reads, the callee declares them as
`unused_` parameters and the calls are plain. The calls below cannot be
written that way.

- `StCdInterrupt`: emulated header/payload copies pass a fourth completion argument to the three-argument `mem2mem` helper, which ignores it.
- `g_thread_call_target` is the main-stack dispatch slot for callees
  with different signatures; assignments erase their function types.
- `world_menu_resize_parent_entry_to_digits`: calls
  `text_count_decimal_digits` with no argument; the callee reads a stale
  `$a0`. Nothing calls the function itself.
- `battle_move_animate_fall_to_target_tile` passes a coordinate
  buffer to the one-argument
  `battle_gfx_calculate_world_height_from_world_coords`, as it does to
  `battle_gfx_calculate_world_height_with_caller_coords`;
  `battle_move_animate_jump_arc_to_own_tile` does not.
- `g_suzuki_smd_modulator_waveforms` entries 8-15 call the void
  `suzuki_smd_modulator_deactivate`; `suzuki_smd_update_modulators` adds the
  leftover `$v0` (the cleared modulator flags) to the channel output.
- `main_sound_update_tunes` passes `current_music` to `suzuki_smd_is_music_playing`
  even when no scenario music is loaded; the null record reads the status
  16-bit value at address `0x10`.
- `menu_animate_window_quad_crop` passes an uninitialized
  source (a stale `$s5`) to `menu_zoom_cursor_frame`, which never reads it.
- `battle_map_init_units_sprites_event_and_music` and
  `battle_map_step_init_sequence` call `main_sound_stop_sfx` without its sound
  id: step 0 passes `map_id`, still in `$a0`; step 11 passes whatever
  `main_sound_unload_current_scenario_music` left there.
- `battle_unit_get_world_coords_by_runtime_id` returns NULL for a missing
  unit, but its callers test for -1 and read through it. A teleport-out with
  removal (Wiegraf at the Fovoham windmill) fades the unit out while the
  teleport particles still read its world coordinates, from kernel RAM at address 0.
- `script_process_inflict_status_commands` skips a battle id of 2000, which
  `battle_unit_get_battle_id_by_runtime_id` never returns: a runtime unit with
  no battle stats gives -1, `battle_unit_get_stats_from_battle_id` then returns
  NULL, and the Float and Jump tests read kernel RAM.
- `battle_map_get_tile_data_ptr_from_battle_id` and `battle_map_get_unit_tile_by_runtime_id` read the
  unit's map position without checking the lookup, so a unit with no runtime
  record (an arc-trajectory target in a Chapter 2 battle) is read from kernel
  RAM at address 0.
- `menu_build_unit_portrait_poly_variant` reads the speaker's battle record
  through `battle_unit_get_stats_from_battle_id`, which returns NULL for a
  speaker who is not on the field; the portrait test then reads kernel RAM.
  `menu_copy_unit_data_to_status_billboard_variant` (BATTLE and DEBUGCHR) does the same while
  counting units, for a runtime record with no battle unit (the final battle).
- `item_sort_id_list_variant`: it counts each unit's five equipment ids into a
  256-byte local array, but the ids are 16-bit values and an item tried on in the
  shop's fitting room carries flag bits above the low byte, so sorting the
  list then increments a byte far past the array.
- `script_resolve_runtime_id`: for the active-turn selector 0x69 with no unit
  holding the turn, it falls back to ENTD units 1 and 2 and tests the Jump
  status of whatever the finder returned; with neither on the field that is
  NULL, so it reads kernel RAM at address 0x58.
- `battle_unit_init`: stores the spritesheet-slot
  claim (0xffff when all nine slots are taken) in the u8
  `spritesheet_vram_slot` and tests the stored 0xff against 0xffff, so the
  failure path never runs and a tenth sheet loads into slot 255.
- `battle_ai_load_ability_entry`: a Jump ability's CT estimate
  for the acting unit reads `acting_unit`, which is NULL during the AI
  workspace setup at battle start (`acting_unit_id` is 0), so slot 0's Jump
  divides kernel-RAM bytes.
- `bunit_entrypoint` passes a second argument, 0, to the
  one-parameter `bunit_gfx_draw_falloff_shaded_rows`; its other caller passes
  one.
- `menu_init_system_function` returns `$v0` as the void
  `text_select_ability_announcement` left it.
- `world_text_bind_section_data` has no return statement;
  `world_card_build_save_slot_description` uses the address its last store
  left in `$v0`.
- `battle_effect_spawn_emitter_particles` has an `s32` result it never sets
  and no caller reads.
- `battle_unit_animate_teleport_distortion` and
  `battle_unit_advance_altima_teleport_distortion` fall through without an
  `s32` result; the animation dispatcher ignores their results.
- `thread_resolve_id` and `thread_resolve_id_after_current` fall
  off their end after `thread_exit_current()`, like the WORLD twins.
- `battle_ai_calculate_clockticks_until_death_counter_expires` is defined
  with three parameters and both callers pass one; the other two are scratch
  it assigns before reading.
- `battle_map_calculate_slope_height` masks its level to a byte; two callers
  pass the 16-bit `action_command.target_elevation` unmasked.
- WLDCORE passes its 8-byte records by value:
  `RECT` to the three render-record builders and
  `point32_t` to `wldcore_window_build_yes_no_panel`.
- Both copies of `script_filter_unit_id_by_mode` read `in_id` as a 16-bit value; most
  callers pass the address of an `int` loop counter (BATTLE through a
  `(u16*)` cast), so the id is that counter's low half.
- `battle_move_calculate_walkto_pathing` has no return statement; its caller
  uses the path block pointer `battle_move_calculate_pathing` left in `$v0`.
- `battle_map_load_gns_and_move_find_items` falls off its end for a map with no
  GNS sector and returns what `bzero` left in `$v0`. `battle_map_load_data`
  compares the result with the GNS table and, when it is not the table (also
  when the read could not start), returns 0 with its 0x20000-byte load buffer
  still allocated.
- `g_battle_effect_secondary_handlers` entries 0x07, 0x0a, 0x0b, 0x0e and 0x10
  set no result, although `battle_effect_update_secondary_effects`
  ends an effect on a handler result of 0. No record of
  `g_battle_effect_secondary_animations` names them.
- `menu_ability_is_non_action_learned` passes one 32-bit pointer slot (0x801f0050, named
  both `g_menu_selected_ability_data` and
  `g_menu_selected_ability_secondary_data`) for both outputs of
  `main_ability_calculate_pointers_and_type`, so the ability data pointer is
  overwritten by the secondary data pointer.

- `menu_input_update_event_state_variant` passes six 32-bit values starting at `g_input_event`
  to the event input loader; CARD's controller pointer at +4 is copied as held
  buttons, and the remaining 32-bit input values are shifted by one field.

- `world_menu_input_update_dialog_state` and `script_run_event_frame_variant` pass six 32-bit values
  starting at `g_script_event_input` to the event input loader. The
  next five 32-bit values include unrelated status-label data; the corresponding held,
  previous and repeat globals reside elsewhere.

## Declaration leads

- The heads of `equip_unit_data_t` and `bunit_unit_t` overlap
  `menu_unit_status_record_t`, but their fields at 0x1e and 0x20 differ.
- BUNIT, EQUIP and WORLD use the 236-byte `menu_gfx_context_t`. JOBSTTS omits the draw-mode pool pointer at 0x60 and uses a
  244-byte stride.
- WLDCORE's `g_wldcore_calendar_zodiac_start_dates` uses `calendar_date_t`
  records; OPEN's `g_open_birthday_zodiac_start_dates[24]` stores the same
  `{month, day}` pairs as bytes.
- `g_main_item_location_flags` (`0x80059414`): only the first 64 bytes (512
  Move-Find bits) are proven; the next 64-byte block is saved with it but has no
  known meaning.
- `g_wldcore_sound_quad_command_bytes` (`0x8009ec18`) starts one row before
  the sound rows, which begin at `0x8009ec1c` for sound id 1: its row 0 is the
  eighth 32-bit entry of `g_wldcore_tutorial_event_ids`, whose eight entries
  `wldcore_list_handle_battle_tutorials_input` indexes.
- `g_battle_effect_ending_secondary_function_id` (`0x801b8530`) is
  `g_battle_effect_secondary_animations[EFFECT_SECONDARY_ANIMATION_ENDING].function_id`
  under a second name; the table's own symbol covers the 21 records before
  it.
- `g_open_file_openbk_end_sector_offsets` (`0x80073f5c`) is
  `g_open_file_openbk_start_sector_offsets + 1`: eight sector offsets of
  OPNBK.BIN under two names, each image ending where the next starts.
- `g_open_gfx_image_copy_progress` (`0x800854b5`) is
  `g_open_gfx_image_copy_row_flags[1]`;
  `open_gfx_copy_file_image_to_210x180_buffer` clears it through the array.

## Duplicated code

Shared sources compile at each original entrypoint with its own module
bindings. Distinct entrypoints and genuinely different implementations remain.

- `wldcore_menu_push_fact_detail_level` and
  `wldcore_menu_push_story_event_text_level` perform the same operation at menu
  levels `0x2f` and `0x1e`; the latter reserves 8 unused stack bytes.
- The ambient-light and darkness blend routines operate on different records.
  WORLD's scaled-draw-area initializers install different second rectangles.
  Similar bodies alone do not make these bindings interchangeable.
- Some routines occur twice within one module with different call and data
  addresses. The entries named `script_run_next_event_unused` and
  `battle_unit_add_event_offset_by_runtime_id_unused` have no caller, as does
  WORLD's unused next-event entry.
- Overlays call both the resident `thread_is_running` and local
  `menu_thread_is_running_variant` entries. Keep those callees distinct even though
  their running-flag reads are identical.
- Overlays share load addresses, so an address match is not an identity
  match. Compare bytes: REQUIRE and EQUIP both have code at `0x801c2dcc`, and
  the two routines differ.

## Other surprises

- `main_gfx_load_efc_fnt`: patches the low immediate byte of `FntLoad` at `0x80022f5c`, changing its font CLUT placement from `y + 128` to `y + 127`.
- `StCdInterrupt`: tracks frame numbers through the header's low `u16`, while DMA completion and OPEN retain the full `u32` frame number.
- `psyq_gte_apply_rotation_ir`: writes saturated IR1–IR3 32-bit output values and returns its unchanged third argument.
- `sprintf`: unsupported conversions terminate output, `%#p` prefixes `0p`, hexadecimal alternate form prefixes zero, and `%#.0o` with zero emits no digit.
- JOBSTTS.OUT offset `0x0` and BUNIT.OUT offset `0x200` hold the literal `"%d"`
  (`g_jobstts_text_decimal_format`, `g_bunit_text_decimal_format`) used by `menu_cmd_draw_formatted_number_handler_variant` and
  `menu_cmd_draw_right_aligned_number_handler_variant`.
- The 32-bit game-options value (`game_options_t`) packs its last five fields
  out of array order.
- CallFunction (both copies of `script_execute_event_variant`)
  tests selectors in sequence against one operand that the arms modify. So
  `0x06` also runs the `0x0f` warp, and `0x0e` can fall into later arms.
- WORLD's copies of some event-library routines call BATTLE-only functions
  at their BATTLE addresses, which hold unrelated code or data while WORLD and
  WLDCORE are loaded: `script_map_darkness` calls `battle_map_start_ambient_light_blend`
  (`0x800934d0`, WLDCORE data), `unit_apply_staged_status_data` runs
  `battle_status_queue_runtime_graphics_flag_change` on the main stack (`0x80068e80`, 0x10 bytes into
  `wldcore_map_update_scroll_axis_speed`) and `unit_save_staged_status_data`
  runs `battle_status_apply_unit_action_removal` (`0x8018ec44`, WORLD text data).
  `unit_get_battle_id_by_event_unit_id` calls
  `battle_unit_get_battle_id_by_runtime_id` (`0x8008cdd0`), which in WLDCORE
  is the epilogue of `wldcore_map_init_dots`: it would reload
  `s0`..`s4` from the caller's frame and pop 48 bytes of its stack. The other
  unit-core helpers the shared event code calls (`battle_unit_has_runtime_id`,
  `battle_unit_get_runtime_id_by_battle_id`,
  `battle_unit_get_stats_from_battle_id`,
  `battle_unit_find_stats_by_entd_unit_id`,
  `battle_gfx_load_unit_graphics_by_battle_id`) bind the same way, into the
  middle of WLDCORE's map-dot routines or WORLD's menu data.
  `unit_set_animation` likewise reaches three BATTLE-only animation setters,
  behind script-variable tests WORLD scenes would have to pass first.
  `camera_wait_until_idle`, `menu_run_triangle_selection` and
  `menu_start_help_menu_thread` call `battle_camera_is_active` (`0x8008e17c`,
  0x1e4 bytes into WLDCORE's `wldcore_map_find_best_route_path`), and
  `menu_update_menus_and_event_speed` calls `battle_state_get_vsync_interval`
  (`0x8008ded8`, 0x2dc bytes into `wldcore_map_draw_path_animation`).
- OPTION.OUT carries its own copy of `menu_init_scaled_draw_area_packets`
  (`0x801c02d0`) but its window and option threads call BATTLE's copy
  (`0x80138adc`); target overrides keep that binding.
- `battle_action_dispatch_target_reaction_ability`: formula 7
  suppresses reactions. Counter Magic (`0x1b3`) is tested last, after byte
  `+0x8d`, so Counter (`0x1ba`) wins.
- `g_main_debug_display_enabled` is only ever cleared by retail code; the
  add-unit threads and unit-summary panels print debug output when it is set.
- `wldcore_input_update_repeat`: the arms test
  UP, DOWN, RIGHT, LEFT, R1, L1, but the counters sit in UP, DOWN, LEFT, RIGHT,
  R1, L1 address order. The early-out clears only the four direction counters.
- `wldcore_map_roll_random_encounter`: encounter masks count from
  the high bit, so the chosen ENTD is `entd_ids[7 - bit]`.
- `battle_action_finalize_draw_out_katana_result`: when the
  katana breaks, the strike work's `can_earn_experience` is copied onto itself.
- `memmove`: the overlap-safe copy returns the original destination on its backward path and the advanced destination on its forward path.
- `StartRCnt` and `StopRCnt` index the IRQ-mask table before validating a counter: selector 3 changes the VBlank mask even though `StartRCnt` returns zero; `StopRCnt` always returns one. Larger indices can read past the four-entry table.
- `SpuGetVoiceEnvelopeAttr`: the public `s32*` key-status output is written with a 16-bit store; its upper half remains unchanged.
- `StartCARD` and `StopCARD` leave their declared signed 32-bit results unspecified.
- `_patch_card2`: exchanges five resident/BIOS 32-bit instructions, so calling it again reverses the exchange rather than repeating an idempotent patch.
- `SetDrawLoad`: initializes the upload header and rectangle but leaves pixel payload storage to its caller.
- `suzuki_sound_get_largest_free_block`: reports the rounded gap including space needed for a new block header, rather than a directly usable payload size.
- `battle_gfx_load_spritesheet_into_vram_slot`: its fixed 0x30d4-byte copy from the variable compressed SPR tail at 0x9200 extends beyond the file-sized allocation in 153 of the 154 nonempty unit sprite-table entries and includes retained heap bytes.
- `battle_gfx_update_and_animate_unit_wep_eff`: when neither EVTCHR slot is free, one path tests `j` before setting it.
- `g_battle_effect_disc_entries` (`0x801b53e8`) stores whole-sector sizes, and entry 0 repeats entry 1: effect id 0 loads E001.BIN, so E000.BIN's code is never loaded through the table.
- `effect_noop`: the native callback returns without setting a result; opcode 07 ignores that result.
- E464's map-vertex particle callback accepts groups 1-12 but indexes a
  three-entry location table; all 16 shipped emitters use group 0, so the
  out-of-range path is inactive.
- `EFFECT/E223.BIN` through `E225.BIN` contain effect data but have zero resident load sizes; requesting those file IDs loads Cure instead. The ability-effect mapping does not reference them.
- `world_gfx_load_wldface_to_frame_buffer`: the WLDFACE rectangles (`g_world_wldface_vram_rects`) use x = 1664–1856; the GPU masks VRAM transfer coordinates, so the pages land at x = 640–832.
- `suzuki_sound_init`: `suzuki_sound_init_sfx_music` reaches `suzuki_smd_insert_music`, which disables and re-enables `g_suzuki_sound_root_counter_2_event` before the initializer opens that event: handle 0 at boot, the handle `suzuki_sound_quit` closed on a soft reset.
- `equip_menu_load_images_and_reset_lists` and `bunit_gfx_init_vram_and_start_fade`: store 16-color CLUTs into the 4-entry `buf1`; they run on into `buf2`, which the frame places directly after it (the source reads as two arrays, the stack holds one).
- `battle_effect_code_script_2e_invalid`: opcodes 2e–77 report an error without setting a handler result; `battle_effect_run_block` then tests the undefined return value.
- EFFECT scripts never run seven state handlers: E000's script calls only `effect_noop` (opcode 07) and E314's names no file function, so native function ids 1 and 90 are bound by nothing; E134/E242 `effect_ring_stack_mesh_update`, E454/E455 `effect_map_set_3d_objects_state_2` and E456 `effect_map_freeze` sit in callback slots (script opcode 06) that no keyframe selector fires. E457.BIN, which has no code, binds native function 88 (0x801c2578, E259's `effect_dome_mesh_update`; data in its own image) to slot 0, and none of its keyframes names a slot.
- `battle_map_draw_mesh_and_weather`: when map command 0x96 (E458) ends, `battle_map_run_weather_command(0x8b)` restores a non-snow rain or storm mid-draw; the mode-0x55 block then re-links `ft3` drops the 0x96 block already linked at OT+2, an ordering-table cycle that holds `DrawSync` until its 240-VBlank timeout.
- `world_card_run_menu_screen`: its 0xCCD8-byte stack frame, containing packet pools and directory entries, reaches below 0x801ff000 into the top cells of MAIN's game heap (0x801df000-0x801ff000); it is safe only while those cells are free.
- `world_script_handle_tutorial_command_end`: two paths fall off the end of the s32 handler and return a stale nonzero `$v0` (the fade's result, or the 1 just stored), so the formation loop keeps running.
- `menu_draw_thread_status_indicators_variant` and BUNIT's
  `menu_draw_thread_status_indicators_variant` (the ATTACK, DEBUGCHR, EQUIP and REQUIRE
  copies have no caller): while a unit-status banner thread (8 or 7) is being
  stopped its first parameter is 0, and the panel check reads that display
  record at RAM address 0.
- `open_script_draw_text_records`: passes OT entries by value (`otag[15]`, `otag[8]`) to `AddPrim`; after `ClearOTagR` each holds the physical address of the entry below it, so the credits fade, bands and glyphs link into layers 14 and 7 through the KUSEG mirror.
- `wldcore_gfx_step_dissolve_image_upload`: the step-0 call (start the dissolve) has no return value of its own; `$v0` keeps its clearing cursor, the address one byte below the band-progress table. Its three callers ignore that result.
- `menu_build_window_sprites`: for the Bar's send-unit list (header 0x10) the arrow x reads the "height" of the record at the panel's value pointer, `&g_wldcore_active_menu_value` + 6, which is the upper half of `g_wldcore_proposition_dispatch_days` (0).
- `battle_unit_generate_treasure`: outside action execution (AI simulation, preview) it returns with no value, so `$v0` still holds `g_battle_action_state` and the crystal pickup result names item 1 or 2 as the treasure.
- `open_title_step_new_game_start`: no retail code sets step 5, which pops New Game and opens the Music Test (`open_menu_start_music_test_controller`, controller slots 7 and 8); only a poke of the 32-bit step value reaches it.
- WORLD's copy of the battle menus is dead code: nothing in any module calls or names `menu_init_system_function` (0x800f5230) or `menu_start_mini_menu_display_thread` (0x800f0e48), so modes 1 (AT list) and 2 (dead-unit panel) of `world_menu_run_main_mode` never run; the world map's Options row runs mode 0. With them goes WORLD's event interpreter: `script_start_current_event_thread`, the only starter of the event thread, is called by `menu_init_system_function` and by the menu-building thread that only BATTLE's `menu_build_idle_action_menu` starts (WORLD's copy is empty), so WORLD's `script_execute_event_variant` never runs.
- Abilities no skillset lists: 0x16f (Attack), Reflect (0x1b8), Non-charge,
  Teleport 2, Move Underwater and Silent Walk. Four ids with no name text are
  played as Altima's teleport effects (0x28, 0xb8, 0xdb, 0xdc), and 0x200 is
  the follow-up strike that resolves a knockback.
- Skillset rows 0x86-0x8d, 0xa7 and 0xa8 have no name text but hold
  abilities; 0xa7 is the row the AI uses for a unit under Blood Suck, and
  0xa9, which it pairs with ability 0x16f, is empty.
- The ability names "Move in Water" (0x1f6) and "Walk on Water" (0x1f7) are
  the reverse of what the abilities do: 0x1f6 keeps the unit on the surface
  and 0x1f7 one level below it (`battle_unit_get_effective_height`,
  `battle_move_store_unit_movement_to_scratchpad`).
- Ability 457, named "Equip Knife", sets the Equip Katana support bit
  (`BATTLE_SUPPORT_SET_1_EQUIP_KATANA`): the unit can equip katanas, not
  knives.
- Selling in the fur shop adds the sold copies to the poached stock
  (`world_shop_run_fur_sell_confirm_step`), so they can be bought back there.
- `world_shop_run_fur_buy_quantity_menu` divides the gil on hand by
  `price >> 1` with no lower bound, while the sell side
  (`world_item_get_half_price`) keeps half price at 1 or more.
- `battle_move_start_teleport_step` builds a secondary-effect record in a
  local and never uses it.
- A change to a map CLUT row reaches VRAM on the second call of
  `battle_map_upload_clut_rows`: one call loads the 256x14 image to y 0x1ee,
  the next moves it to y 0x1e0. Rows 8 and 9 have state and are uploaded, but
  no 16-bit CLUT ID in the code addresses them.
- Rolls one off their round number: `wldcore_map_roll_random_encounter` tests
  `chance >= pick` over 0..99, so the retail chance byte 30 gives 31 in 100;
  the preferred-job promotion in `wldcore_proposition_determine_success`
  passes on a roll below 0x3D, 61 in 100; `battle_formula_apply_status_to_action`
  divides the displayed accuracy of separate statuses by four and keeps each
  status on a roll below 24; `main_unit_generate_out_of_battle` and
  `main_unit_store_character_names` draw a generic name with
  `rand() * 255 / 0x8000`, 0-254, so the last of each list's 256 names is
  never drawn.
- `battle_unit_apply_level_up_down_ability` stores the new level before
  calling `main_unit_apply_level_growth`, so an ability level-up divides each
  raw stat by growth + new level; an EXP level-up (`main_unit_check_level_up`)
  and the generated-unit replay in `main_unit_calculate_actual_stats` divide
  by growth + old level.
- `battle_unit_find_war_trophies_and_bonus_money` computes
  `level_bonus_money` ((highest enemy level - lowest party level) * 100, 0
  once a player unit has fallen) and nothing reads it; REQUIRE pays
  `level_sum_money` and `bonus_money`.
- `battle_formula_apply_weather_effects_on_bows` reads the weather variable
  itself, not `battle_map_get_effective_weather`: the bow penalty of a storm
  also applies on a snow map and on a map flagged to ignore weather, where the
  elemental and movement rules see no storm.
- `battle_ai_load_ability_entry`: with Throw Item the AI gives an
  item a range of the unit's Move; `battle_target_set_panels_for_action` uses
  4, and gives a Mime the same.
- `battle_map_apply_gns_command`: command rows 0x85, 0x86, 0x88, 0x89, 0x8a
  and 0x8b skip the map-state comparison and are applied in file order, so the
  last of a kind wins: MAP017.GNS has one set-outdoor row followed by six
  set-indoor rows and ends indoors. Row types 0x87 (49 rows in 14 maps) and
  0x8a (125 rows) have no case in `battle_map_dispatch_gns_resource`, and no
  retail GNS holds a 0x83, 0x84 or 0x8b row, the three that take arguments.
- Terrain data the code does not expect: five tiles hold a slope byte outside
  `map_slope_type_e` (0x10 in MAP017.10, MAP043.9 and MAP078.9, 0x46 in
  MAP036.46, 0x01 in MAP045.7), and the terrain chunks of 37 maps arrive with
  mark bits already set in tile byte 5, which
  `battle_map_copy_xy_coords_and_tile_data` copies as they are. Tile flag 0x80
  (`cannot_stop`), which `battle_move_set_reachable_tile_flags` tests, is set
  in no terrain chunk, and tile byte 1 is 0 in all of them.
- `g_battle_map_directory_path` is the string `C:\FFTIMG\MAP\GNS\`; every map
  file load passes it as the first argument, which
  `battle_map_start_file_load` ignores.
- The Brave Story "Fact" row never appears:
  `EVENT_SCRIPT_VAR_FACTS_ENABLED` (0x92) gates it
  (`wldcore_menu_build_brave_story_entries`), no code sets it, and the one
  event script that writes it, 0x075, stores 0 while setting facts
  0x1a4-0x1aa. The 24 fact names in WORLD.LZW are empty strings.
- `g_wldcore_treasure_sound_novel_ids` is 0 for every treasure but 31
  (Germonik Scriptures, novel 5), so `wldcore_menu_step_treasure_detail_level`
  opens no reader for the four book treasures 17-20.
- World-map script opcodes 0x10 and 0x11 pass on a date on or after and on or
  before their operand (`wldcore_opcode_map_check_date_on_or_after`,
  `_on_or_before`); scenario condition types 16 and 17 are the reverse
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
  `battle_target_set_green_panels_for_action` passes range-record `flags_2`
  bit 0x20 (`ABILITY_SECONDARY_FLAG_2_TOP_DOWN`) to
  `battle_target_apply_vertical_tolerance` as `single_layer`, and none of the
  368 records sets it; the low nibble of
  `ability_data_t.type_flags` holds the ability type plus one (0 for ids 0,
  0x1fe and 0x1ff) and nothing reads it, because
  `main_ability_calculate_pointers_and_type` derives the type from the id.
- `main_status_init_check_data` builds eleven status check sets; sets 2 and 7
  (`MAIN_STATUS_CHECK_SET_UNREAD_2`, `_UNREAD_7`) are read by nothing.
- WORLD's copies of BATTLE's action-menu and tile-info routines
  (`menu_build_ability_list`, `menu_build_skillset_entries`,
  `menu_run_selected_tile_info_display_thread` and nine more) keep BATTLE's
  absolute data addresses 0x80173cbc, 0x8017403c, 0x80174048 and 0x8017404c.
  In WORLD's image those lie inside the text file embedded at 0x8016ee68, so
  the routines would read and write text bytes there.
- `g_main_terrain_geomancy_abilities` gives Gusty Wind for Sky and Kamaitachi
  for Darkness, Cross section and the unnamed surfaces 0x2f-0x3e; the terrain
  help text says "None" for Sky, Darkness and Cross section.
- `battle_camera_update_rotation` chains a held R1 through
  `battle_camera_set_left_turn_destination`, as it does L1; the two helpers differ only in
  which way they move a destination that is a multiple of 0x400.
- `main_unit_copy_job_data` adds the immortal and formation immunity sets
  only to a unit that is both immortal and flagged save- or load-formation;
  the per-flag tests inside the loop are then always true.
- `battle_formula_apply_status_to_action`: the
  `ACTION_STATE_PREVIEW` test in the separate-status loop cannot pass;
  any state but EXECUTE was switched to all-or-nothing above it.
- `main_gfx_build_now_loading` loads the NOW LOADING CLUT to VRAM (0x70,
  0x1e2), 16-bit CLUT ID 0x7887; in battle that is
  `MAP_CLUT(MAP_CLUT_ROW_SPRITES, 7)`, the status bubble CLUT.
- The effect interpreter indexes fixed tables with wider fields and no check:
  the opcode is masked to nine bits over the 120 entries of
  `g_battle_effect_instructions` (0x2e-0x77 report an error without returning
  a result: `battle_effect_run_block` narrows the leftover `$v0` to `s16`
  and tests it while the script position stays unchanged);
  `g_battle_effect_particle_render_handlers` has four entries for
  `(kind_flags >> 12) & 0xf`, so `EFFECT_WORK_KIND_HEAP_BLOCK` would call
  through `g_battle_effect_camera_rotation_mode`; opcode 06 takes a six-bit
  callback slot over the five 32-bit slots of `g_battle_effect_callback_slots`, and a
  keyframe action names slot 1-7 over the record's four (5 writes
  `target_index`). Retail files use opcodes below 0x2e, kinds 0 and
  2 and slots 1-4.
- Special unit name 0x44, "Rofokare", is given to no ENTD unit; every other
  named entry of the 256 is.
- `battle_heap_alloc_block` has no failure exit: when no free block is large
  enough it walks the circular free list forever.
  `attack_deploy_select_unit_for_stats_display` likewise loops forever when
  no roster unit is deployable for the squad, and
  `battle_effect_alloc_work_record`, with its free chain empty, calls the empty
  report stub and then reads the next link through NULL.
- `menu_run_unit_editor_panel_thread` hands a unit with mount info (a
  rider or a mount) to DEBUGCHR.OUT's `debugchr_editor_run_unit_thread`
  (`menu_open_debugchr_unit_editor`); any other unit gets BATTLE's own
  panel.
- The camera's L2/R2 input uses `battle_camera_toggle_zoom` and `battle_camera_toggle_tilt`,
  bypassing `battle_camera_zoom_map` and `battle_camera_handle_tilt_request`. The linear
  position, screen-offset, zoom and rotation countdown setters have no retail
  callers, so their per-frame update routines never move anything.
- `battle_formula_49_unused_with_extra_steps` is a second entry to formula 0x49's
  handler, but neither the formula table nor any caller names it.
- `battle_state_enter_effect_playback` and
  `battle_state_enter_effect_state` are the only code that enters
  `BATTLE_GAME_STATE_EFFECT`; neither has a caller, so
  `battle_state_handle_effect_state` never runs.
- WORLD's `world_menu_resize_parent_entry_to_digits` and
  `menu_run_require_help` have no callers; their quirks above do not
  execute in retail WORLD.
- `card_io_wait_for_hardware_event` is OPEN's only caller of
  `card_io_poll_hardware_event_result`, and nothing calls the wait routine.
- MAIN's Orbonne caption fades have no callers. `main_set_display_draw_new_game`
  is called only by `main_gfx_fade_in_orbonne_monastery`, so it is unreachable too.
- `suzuki_sound_set_unknown_80032a28` stores a 16-bit sound-driver value that `suzuki_sound_init`
  and `suzuki_sound_quit` clear and nothing reads; nothing calls the setter.
- `main_run_zero_padding` (0x80010ba8), which `main_overlay_exec_battle_bin`
  reaches after loading BATTLE.BIN, is not BATTLE code: it is the 4 KB of zero
  words after MAIN's C start-up, run as 1,024 no-ops up to the `jr ra` at
  0x80011ba8. BATTLE runs when `main_system_run_game_loop` then calls
  `battle_state_run_game_loop`.
- `main_boot_run_startup` installs the empty `main_file_handle_cd_ready_callback`
  and `main_file_handle_cd_read_callback`; the `CdInit` it runs next replaces
  them with LIBCD's `def_cbready` and `def_cbread`, so neither ever runs.
- `FlushCache` is called only by the BIOS patches (`_patch_card`,
  `_patch_card2`, `_patch_gte`, `psyq_card_restore_exception_prefix`); no
  overlay load flushes the instruction cache before the new code runs.
- `_spu_FsetDelayR` sets the SPU bus timing for DMA writes and
  `_spu_FsetDelayW` for reads, and `_spu_t` and `_spu_Fr_` call them that way:
  each suffix names the opposite direction.
- `SpuQuit` closes the SPU DMA event before disabling it, so `DisableEvent`
  gets a closed descriptor.
- `battle_effect_update_stage`: nothing stores playback phase 5 or
  `EFFECT_PLAYBACK_PHASE_RESTART_SCRIPT` (6), nor the two start pointers the
  restart arm rewinds to (`g_battle_effect_frame_data_start`,
  `g_battle_effect_animation_sequences_start`), so the restart path never runs;
  phase 5 has no case and would return `result` unset.
- `battle_ai_is_unit_untargetable`: the AI never targets a Transparent enemy
  unless it is a rider whose mount is Transparent too.
- World-map script opcodes 0x22-0x24 (`wldcore_opcode_map_request_draw_location`,
  `wldcore_opcode_map_request_erase_location`, `wldcore_opcode_map_focus_location`)
  read their operand without advancing `ip`; each ends the block, so no opcode
  is read from the operand.
