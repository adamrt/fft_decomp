/* Unit record init and the queued SHP/SEQ/SPR graphics loader. */

#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/event_attack.h"
#include "fft/script.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Claims and initializes unit runtime record for one queued unit: spritesheet VRAM
 * slot and palette, map position, displays and the three weapon/body-part
 * sprite records, then links it at the head of the runtime-unit list.
 *
 * `id` is copied before the claim call, which keeps the unextended copy in
 * fp. The unit block pointer is assigned after the call to
 * battle_unit_set_move_and_world_coords: because it lives across calls,
 * sched1 still hoists it to the join, just after that call's argument copy.
 * Separate animation and sprite-block pointers change register allocation.
 * The contradictory weapon range test (<0x86, <0x3c, >=0x4a, !=0x5d) is
 * literally what the target branches encode (the call is dead code). */
battle_unit_t* battle_unit_init(s32 map_x, s32 map_y, s32 map_level, s16 facing, s32 spritesheet_id, s16 palette,
    s16 requested_runtime_id, unit_stats_t* stats, u32 flags, const battle_gfx_unit_sprite_file_t* data) {
    battle_unit_t* unit;
    union {
        battle_unit_anim_state_t* animation;
        battle_unit_sprite_block_t* block;
    } cursor;
    map_tile_t* tile;
    s32 runtime_id;
    s32 slot;
    u16 id;
    s32 i;
    battle_unit_t** head;
    u8 tile_x;
    u8 tile_y;
    u8 tile_z;

    id = spritesheet_id;
    unit = battle_unit_claim_slot(requested_runtime_id);
    if (unit == 0) {
        return 0;
    }
    runtime_id = unit->runtime_id;
    unit->spritesheet_id = id;
    slot = unit->spritesheet_vram_slot = battle_gfx_claim_spritesheet_slot(spritesheet_id);
    if (slot == 0xffff) {
        g_battle_unit_slot_states[unit->runtime_id].in_use = 0;
        return 0;
    }
    if (id >= BATTLE_SPRITESHEET_ID_JOB_FIRST && id < BATTLE_SPRITESHEET_ID_CHOCOBO && stats != 0) {
        palette = stats->sprite_palette;
    }
    if (palette >= 6) {
        palette = 0;
    }
    unit->stored_palette = palette;
    unit->statuses_to_remove_5_6 = 0;
    unit->statuses_to_remove_1_4 = 0;
    unit->statuses_to_add_5_6 = 0;
    unit->statuses_to_add_1_4 = 0;
    unit->status_flags_5_6 = 0;
    unit->status_flags_1_4 = 0;
    battle_gfx_load_spritesheet_into_vram_slot(data, slot, runtime_id, unit, palette);
    tile = map_get_tile_data_pointer(map_x, map_y, map_level);
    battle_gfx_check_tile_status_palette_mod(unit, tile->flags.shading, 1);
    map_load_palette_data(&g_battle_gfx_shadow_palette, MAP_CLUT_ROW_UNIT_SHADOWS, runtime_id, 0);
    unit->clut = GetClut(runtime_id << 4, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_UNITS);
    unit->tpage = slot + BATTLE_GFX_TPAGE_SPRITESHEET_SLOT_0;
    if (flags == 0) {
        unit->draw_enabled = 1;
        unit->animation.trigger = 1;
    } else {
        unit->draw_enabled = 0;
        unit->animation.trigger = 0;
    }
    unit->standing_on_battle_id_plus_one = 0;
    unit->stats = stats;
    unit->requested_animation = 0;
    unit->facing = facing * 1024;
    unit->camera_facing_sixteenth = unit->camera_facing_quadrant = unit->attack_facing = -1;
    unit->mount_state = BATTLE_UNIT_MOUNT_STATE_NONE;
    unit->horizontal_flip_flag = 0;
    unit->depth_height_offset = 0;
    unit->float_bob_offset = 0;
    unit->float_bob_phase = 0;
    unit->step_phase = MOVE_STEP_PHASE_NONE;
    unit->path.count = 0;
    unit->sprite_display_flags = 0;
    unit->layer_priority = 0;
    unit->turn_action_flags = 0;
    unit->special_graphic_y_offset = 0;
    unit->distortion_animation_id = BATTLE_DISTORTION_ANIMATION_NONE;
    unit->distortion_phase = 0;
    tile_x = map_x;
    unit->map_x = tile_x;
    unit->movement.suppress_facing_update = 0;
    tile_y = map_y;
    unit->map_y = tile_y;
    unit->movement.suppress_palette_update = 0;
    tile_z = map_level;
    unit->map_z = tile_z;
    unit->movement.alternate_short_hop = 0;
    unit->movement.floating = 0;
    unit->movement.force_animation_update = 0;
    unit->movement.destination_x = tile_x;
    unit->movement.destination_y = tile_y;
    unit->movement.destination_z = tile_z;
    battle_unit_set_move_and_world_coords(unit);
    cursor.animation = &unit->animation;
    unit->velocity.vz = 0;
    unit->velocity.vy = 0;
    unit->velocity.vx = 0;
    unit->walk_speed = 0x2000;
    unit->seq_offset.vz = 0;
    unit->seq_offset.vy = 0;
    unit->seq_offset.vx = 0;
    unit->screen_offset.vz = 0;
    unit->screen_offset.vy = 0;
    unit->screen_offset.vx = 0;
    unit->event_offset.vz = 0;
    unit->event_offset.vy = 0;
    unit->event_offset.vx = 0;
    unit->fade_out_phase = BATTLE_UNIT_FADE_OUT_NONE;
    unit->shadow_graphic_trigger = 1;
    unit->shadow_dirty = 1;
    unit->shadow_counter = 0;
    unit->numeric_display.active = 0;
    unit->numeric_display.selector = 0;
    unit->numeric_display.progress = 0;
    unit->numeric_display.displays[0] = battle_gfx_init_numeric_sprite_display(runtime_id, 0);
    unit->numeric_display.displays[1] = battle_gfx_init_numeric_sprite_display(runtime_id, 1);
    unit->numeric_display.displays[2] = battle_gfx_init_numeric_sprite_display(runtime_id, 2);
    unit->numeric_display.displays[0]->scale_x = unit->numeric_display.displays[0]->scale_y
        = unit->numeric_display.displays[1]->scale_x = unit->numeric_display.displays[1]->scale_y
        = unit->numeric_display.displays[2]->scale_x = unit->numeric_display.displays[2]->scale_y = ONE;
    unit->numeric_display.displays[0]->tpage = unit->numeric_display.displays[1]->tpage
        = unit->numeric_display.displays[2]->tpage = BATTLE_GFX_TPAGE_FRAME;
    unit->numeric_display.displays[0]->clut = unit->numeric_display.displays[1]->clut
        = unit->numeric_display.displays[2]->clut
        = unit->clut + 0x100; /* the unit's CLUT of MAP_CLUT_ROW_ACTION_RESULTS */
    unit->numeric_display.displays[0]->part_count = unit->numeric_display.displays[1]->part_count
        = unit->numeric_display.displays[2]->part_count = 1;
    unit->status_bubble_active = 0;
    unit->status_bubble_id = 0;
    unit->status_bubble_timer = 0;
    unit->status_bubble_display = battle_gfx_init_status_bubble_sprite_display(runtime_id);
    unit->item_display_active = 0;
    unit->item_display = battle_gfx_init_item_sprite_display(runtime_id);
    unit->position_copies_active = 0;
    unit->animation.display = battle_gfx_init_unit_sprite_display(runtime_id);
    cursor.animation->graphic_type = BATTLE_SPRITE_GRAPHIC_UNIT;
    unit->animation.shp = unit->animation.saved_shp = battle_gfx_get_unit_shp_by_spritesheet_id(id);
    unit->animation.seq = unit->animation.saved_seq = battle_gfx_get_unit_seq_by_spritesheet_id(id);
    cursor.animation->wait = 1;
    cursor.animation->restart_count = 0;
    cursor.animation->script_pos = 0;
    cursor.animation->sequence_id = BATTLE_UNIT_ANIMATION_IDLE << 1;
    cursor.animation->frame = 0;
    cursor.animation->flags = 0;
    cursor.animation->wait_bias = 0;
    cursor.animation->frame_bias = 0;
    cursor.animation->repeat_count = 0;
    unit->animation.display->scale_x = ONE;
    unit->animation.display->scale_y = ONE;
    unit->animation.display->rotation = 0;
    battle_unit_set_animation_based_on_status(unit);
    battle_gfx_run_unit_seq_script(unit, cursor.animation, 1);
    for (i = 0; i < 3; i++) {
        cursor.block = &unit->sprite_blocks[i];
        cursor.block->display = battle_gfx_init_weapon_sprite_display(runtime_id, i);
        if (i == 0) {
            cursor.block->graphic_type = BATTLE_SPRITE_GRAPHIC_WEAPON;
            cursor.block->shp = battle_gfx_get_weapon_shp_by_spritesheet_id(id);
            cursor.block->seq = battle_gfx_get_weapon_seq_by_spritesheet_id(id);
        } else {
            cursor.block->graphic_type = BATTLE_SPRITE_GRAPHIC_EFFECT;
            cursor.block->shp = battle_gfx_get_effect_shp_data(id);
            cursor.block->seq = battle_gfx_get_effect_seq_data(id);
        }
        cursor.block->trigger = 0;
        cursor.block->wait = 1;
        cursor.block->script_pos = 0;
        cursor.block->sequence_id = 0;
        cursor.block->frame = 0;
        cursor.block->flags = 0;
        cursor.block->wait_bias = 0;
        cursor.block->frame_bias = 0;
        cursor.block->repeat_count = 0;
        cursor.block->display->scale_x = ONE;
        cursor.block->display->scale_y = ONE;
        cursor.block->display->rotation = 0;
        if ((s16)id < BATTLE_SPRITESHEET_ID_CHOCOBO && (s16)id < BATTLE_SPRITESHEET_ID_VELIUS
            && (s16)id >= BATTLE_SPRITESHEET_ID_BOY && (s16)id != BATTLE_SPRITESHEET_ID_DAMI) {
            battle_gfx_run_wep_eff_seq_script(unit, &unit->sprite_blocks[i], i, 1);
        }
    }
    head = &g_battle_unit_list_head;
    unit->previous = *head;
    *head = unit;
    battle_gfx_set_thrown_item_graphic_palette(unit->stats->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON], unit);
    status_init_special_flag_enabling(unit->stats->battle_id);
    unit->movement.suppress_sfx_and_landing_effects = 1;
    battle_unit_update_display_by_runtime_id(unit->runtime_id);
    battle_unit_set_animation_based_on_status(unit);
    unit->movement.suppress_sfx_and_landing_effects = 0;
    return unit;
}

/* The two clearing loops walk their arrays by byte offset. A struct-subscript
 * walk leaves GCC with both the index and the scaled address as induction
 * variables, adding an `addiu`/`li` pair to each loop; the byte offset is what
 * reproduces the target's single `bgez` on the address offset. */
void battle_gfx_reset_unit_graphics_load_state(void) {
    s32 i;
    s32 offset;

    g_battle_unit_list_head = 0;
    for (offset = (BATTLE_UNIT_RUNTIME_SLOT_COUNT - 1) * sizeof(battle_unit_slot_state_t); offset >= 0;
        offset -= sizeof(battle_unit_slot_state_t)) {
        ((battle_unit_slot_state_t*)((u8*)g_battle_unit_slot_states + offset))->in_use = 0;
    }
    for (offset = 8 * sizeof(battle_gfx_spritesheet_slot_t); offset >= 0;
        offset -= sizeof(battle_gfx_spritesheet_slot_t)) {
        ((battle_gfx_spritesheet_slot_t*)((u8*)g_battle_gfx_spritesheet_slots + offset))->in_use = 0;
    }
    for (i = 0; i < 2; i++) {
        g_battle_gfx_vram_slots[i].owner = BATTLE_GFX_VRAM_SLOT_OWNER_NONE;
        g_battle_gfx_vram_slots[i].image_buffer_state = BATTLE_GFX_VRAM_SLOT_BUFFER_FREE;
        g_battle_gfx_vram_slots[i].evtchr_load_state = 0;
    }
    setRECT(&g_battle_gfx_vram_slots[0].image_rect, 0x100, 0, 0x40, 0xc8);
    setRECT(&g_battle_gfx_vram_slots[1].image_rect, 0x140, 0, 0x40, 0xc8);
    g_battle_gfx_seq_data_cursor = g_battle_gfx_unit_animation_data;
    g_battle_gfx_unit_graphics_load_state.file_kind = BATTLE_GFX_UNIT_LOAD_KIND_FIRST;
    g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_START_READ;
    g_battle_gfx_unit_graphics_load_state.index = 0;
    g_battle_gfx_unit_graphics_load_state.buffer = 0;
    g_battle_gfx_shp_frame_data_cursor = g_battle_gfx_shp_frame_data_buffer;
}

/* Advances the unit SHP/SEQ loader by one step and returns the file kind index.
 *
 * g_battle_gfx_unit_graphics_load_state.file_kind selects the file kind (sprite SHP/SEQ, WEP SHP/SEQ, EFF SHP/SEQ,
 * WEP.SPR, OTHER.SPR), g_battle_gfx_unit_graphics_load_state.phase the phase (0 allocate and start the read,
 * 1 wait, 2 copy into the RAM tables, 3 free and advance) and g_battle_gfx_unit_graphics_load_state.index the
 * file index within the kind.
 *
 * The target keeps several of these globals' addresses in registers; only a
 * single-assignment pointer local reproduces that (a plain `g++` folds %lo into
 * each access), and a block-scoped one per case also lets the scheduler hoist
 * the g_battle_gfx_unit_graphics_load_state.file_kind load above the counter store, as the target does. */
s32 battle_gfx_step_load_unit_shp_seq_data(void) {
    s32* phase;
    s32* index;
    void** slot;
    void* buffer;

    phase = &g_battle_gfx_unit_graphics_load_state.phase;
    switch (*phase) {
    case BATTLE_GFX_UNIT_LOAD_PHASE_START_READ:
        switch (g_battle_gfx_unit_graphics_load_state.file_kind) {
        case BATTLE_GFX_SHARED_LOAD_SPRITE_SHP:
            index = &g_battle_gfx_unit_graphics_load_state.index;
            buffer = main_heap_alloc(g_battle_gfx_sprite_shp_files[*index].size);
            g_battle_gfx_unit_graphics_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(
                    g_battle_gfx_sprite_shp_files[*index].sector, g_battle_gfx_sprite_shp_files[*index].size, buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_SHARED_LOAD_SPRITE_SEQ:
            index = &g_battle_gfx_unit_graphics_load_state.index;
            buffer = main_heap_alloc(g_battle_gfx_sprite_seq_files[*index].size);
            g_battle_gfx_unit_graphics_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(
                    g_battle_gfx_sprite_seq_files[*index].sector, g_battle_gfx_sprite_seq_files[*index].size, buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SHP:
            index = &g_battle_gfx_unit_graphics_load_state.index;
            buffer = main_heap_alloc(g_battle_gfx_weapon_shp_files[*index].size);
            g_battle_gfx_unit_graphics_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(
                    g_battle_gfx_weapon_shp_files[*index].sector, g_battle_gfx_weapon_shp_files[*index].size, buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SEQ:
            index = &g_battle_gfx_unit_graphics_load_state.index;
            buffer = main_heap_alloc(g_battle_gfx_weapon_seq_files[*index].size);
            g_battle_gfx_unit_graphics_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(
                    g_battle_gfx_weapon_seq_files[*index].sector, g_battle_gfx_weapon_seq_files[*index].size, buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_SHARED_LOAD_EFFECT_SHP:
            index = &g_battle_gfx_unit_graphics_load_state.index;
            buffer = main_heap_alloc(g_battle_gfx_effect_shp_files[*index].size);
            g_battle_gfx_unit_graphics_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(
                    g_battle_gfx_effect_shp_files[*index].sector, g_battle_gfx_effect_shp_files[*index].size, buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_SHARED_LOAD_EFFECT_SEQ:
            index = &g_battle_gfx_unit_graphics_load_state.index;
            buffer = main_heap_alloc(g_battle_gfx_effect_seq_files[*index].size);
            g_battle_gfx_unit_graphics_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(
                    g_battle_gfx_effect_seq_files[*index].sector, g_battle_gfx_effect_seq_files[*index].size, buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SPR:
            *(slot = &g_battle_gfx_unit_graphics_load_state.buffer) = main_heap_alloc(0x15000);
            if (*slot == 0) {
                break;
            }
            if (main_file_call_build_header(DISC_SECTOR_BATTLE_WEP_SPR, 0x15000, (void*)*slot) == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free((void*)*slot);
            }
            break;
        case BATTLE_GFX_SHARED_LOAD_OTHER_SPR:
            *(slot = &g_battle_gfx_unit_graphics_load_state.buffer) = main_heap_alloc(0x8800);
            if (*slot == 0) {
                break;
            }
            if (main_file_call_build_header(DISC_SECTOR_BATTLE_OTHER_SPR, 0x8800, (void*)*slot) == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free((void*)*slot);
            }
            break;
        }
        break;
    case BATTLE_GFX_UNIT_LOAD_PHASE_WAIT:
        if (main_file_is_still_loading() != 0) {
            break;
        }
        (*phase)++;
        break;
    case BATTLE_GFX_UNIT_LOAD_PHASE_INSTALL:
        switch (g_battle_gfx_unit_graphics_load_state.file_kind) {
        case BATTLE_GFX_SHARED_LOAD_SPRITE_SHP:
            battle_gfx_unpack_unit_shp_data(
                &g_battle_gfx_unit_shp_frame_tables[g_battle_gfx_unit_graphics_load_state.index],
                g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_SHARED_LOAD_SPRITE_SEQ:
            battle_gfx_unpack_unit_seq_data(&g_battle_gfx_unit_seq_tables[g_battle_gfx_unit_graphics_load_state.index],
                g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SHP:
            battle_gfx_copy_shp_data_to_ram(
                &g_battle_gfx_weapon_graphics[g_battle_gfx_unit_graphics_load_state.index].shp,
                g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SEQ:
            battle_gfx_resolve_packed_offset_table(
                g_battle_gfx_weapon_graphics[g_battle_gfx_unit_graphics_load_state.index].seq,
                g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_SHARED_LOAD_EFFECT_SHP:
            battle_gfx_copy_shp_data_to_ram(
                &g_battle_gfx_effect_graphics[g_battle_gfx_unit_graphics_load_state.index].shp,
                g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_SHARED_LOAD_EFFECT_SEQ:
            battle_gfx_resolve_packed_offset_table(
                g_battle_gfx_effect_graphics[g_battle_gfx_unit_graphics_load_state.index].seq,
                g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SPR:
            battle_gfx_load_wep_spr_data(g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_SHARED_LOAD_OTHER_SPR:
            battle_gfx_copy_other_spr_palette_data_to_ram(g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        }
        {
            s32* next_phase = &g_battle_gfx_unit_graphics_load_state.phase;

            (*next_phase)++;
        }
        break;
    case BATTLE_GFX_UNIT_LOAD_PHASE_FINISH:
        main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
        *phase = BATTLE_GFX_UNIT_LOAD_PHASE_START_READ;
        switch (g_battle_gfx_unit_graphics_load_state.file_kind) {
        case BATTLE_GFX_SHARED_LOAD_SPRITE_SHP: {
            s32* counter = &g_battle_gfx_unit_graphics_load_state.index;

            if (++*counter >= 5) {
                *counter = 0;
                g_battle_gfx_unit_graphics_load_state.file_kind++;
            }
            break;
        }
        case BATTLE_GFX_SHARED_LOAD_SPRITE_SEQ: {
            s32* counter = &g_battle_gfx_unit_graphics_load_state.index;

            if (++*counter >= 5) {
                *counter = 0;
                g_battle_gfx_unit_graphics_load_state.file_kind++;
            }
            break;
        }
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SHP:
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SEQ: {
            s32* counter = &g_battle_gfx_unit_graphics_load_state.index;

            if (++*counter >= 2) {
                *counter = 0;
                g_battle_gfx_unit_graphics_load_state.file_kind++;
            }
            break;
        }
        case BATTLE_GFX_SHARED_LOAD_EFFECT_SHP:
        case BATTLE_GFX_SHARED_LOAD_EFFECT_SEQ: {
            s32* counter = &g_battle_gfx_unit_graphics_load_state.index;

            if (++*counter >= 1) {
                *counter = 0;
                g_battle_gfx_unit_graphics_load_state.file_kind++;
            }
            break;
        }
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SPR:
        case BATTLE_GFX_SHARED_LOAD_OTHER_SPR: {
            s32* kind = &g_battle_gfx_unit_graphics_load_state.file_kind;

            g_battle_gfx_unit_graphics_load_state.index = 0;
            (*kind)++;
            break;
        }
        }
        break;
    }
    return g_battle_gfx_unit_graphics_load_state.file_kind;
}

void battle_unit_update_and_animate_units_three_times(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        battle_unit_update_and_animate_units();
    }
}

/* Rewinds the unit graphics loader (g_battle_gfx_unit_graphics_load_state) to its
 * first file kind, the start-read phase, index 0 and no buffer. The argument is
 * not read. */
void battle_gfx_clear_unit_graphics_load_state(s32 unused_map_id) {
    g_battle_gfx_unit_graphics_load_state.file_kind = BATTLE_GFX_UNIT_LOAD_KIND_FIRST;
    g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_START_READ;
    g_battle_gfx_unit_graphics_load_state.index = 0;
    g_battle_gfx_unit_graphics_load_state.buffer = 0;
}

/* Builds the deployed units' data when a map starts and rewinds the unit graphics
 * loader.
 *
 * A battle whose deployment screen is skipped places the units from
 * g_main_deployed_unit_map_coordinates; otherwise the coordinate slots of the
 * deployment screen are used, and in the debug battle style the red team is set
 * up from them first. */
void battle_gfx_init_deployment_and_reset_unit_graphics_state(void) {
    g_main_gfx_unit_graphics_load_queue.count = 0;
    if (script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) == 0) {
        if (g_battle_deployment_skipped != 0) {
            battle_unit_build_deployed_units_data(g_main_deployed_unit_map_coordinates, 0);
            g_battle_gfx_unit_graphics_load_state.file_kind = BATTLE_GFX_UNIT_LOAD_KIND_FIRST;
            g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_START_READ;
            g_battle_gfx_unit_graphics_load_state.index = 0;
            g_battle_gfx_unit_graphics_load_state.buffer = 0;
            return;
        }
    } else {
        battle_unit_init_deployed_units_data_for_debug_red_team(attack_deploy_get_coordinate_slots(), 0);
    }
    battle_unit_build_deployed_units_data(attack_deploy_get_coordinate_slots(), 0);
    g_battle_gfx_unit_graphics_load_state.file_kind = BATTLE_GFX_UNIT_LOAD_KIND_FIRST;
    g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_START_READ;
    g_battle_gfx_unit_graphics_load_state.index = 0;
    g_battle_gfx_unit_graphics_load_state.buffer = 0;
}

/* Steps the per-unit graphics loader for the queued load descriptors.
 *
 * g_battle_gfx_unit_graphics_load_state holds the file kind (0 SHP, 1 SEQ, 2 SPR), the
 * phase (0 read, 1 wait, 2 install, 3 finish), the descriptor index and the
 * heap buffer. SHP/SEQ ids below 5, or equal to the one already resident, and
 * an already loaded spritesheet skip the read. Returns 0 once every
 * descriptor is done, otherwise 1 inside a step and 2 after a step boundary. */
s32 battle_gfx_step_queued_unit_graphics_load(void) {
    s32 result;
    s32 id;

    result = 1;
    switch (g_battle_gfx_unit_graphics_load_state.phase) {
    case BATTLE_GFX_UNIT_LOAD_PHASE_START_READ:
        switch (g_battle_gfx_unit_graphics_load_state.file_kind) {
        case BATTLE_GFX_QUEUED_LOAD_SHP:
            id = g_battle_gfx_spritesheet_data[g_main_gfx_unit_graphics_load_queue
                                                   .descriptors[g_battle_gfx_unit_graphics_load_state.index]
                                                   .spritesheet_id]
                     .shp_id;
            if (id < 5 || id == g_battle_gfx_last_loaded_shp_id) {
                result = 2;
                g_battle_gfx_unit_graphics_load_state.buffer = 0;
                g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_FINISH;
                break;
            }
            g_battle_gfx_last_loaded_shp_id = id;
            g_battle_gfx_unit_graphics_load_state.buffer = main_heap_alloc(g_battle_gfx_sprite_shp_files[id].size);
            if (g_battle_gfx_unit_graphics_load_state.buffer == 0) {
                break;
            }
            if (main_file_call_build_header(g_battle_gfx_sprite_shp_files[id].sector,
                    g_battle_gfx_sprite_shp_files[id].size, g_battle_gfx_unit_graphics_load_state.buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_QUEUED_LOAD_SEQ:
            id = g_battle_gfx_spritesheet_data[g_main_gfx_unit_graphics_load_queue
                                                   .descriptors[g_battle_gfx_unit_graphics_load_state.index]
                                                   .spritesheet_id]
                     .seq_id;
            if (id < BATTLE_SEQ_RUKA || id == g_battle_gfx_last_loaded_seq_id) {
                result = 2;
                g_battle_gfx_unit_graphics_load_state.buffer = 0;
                g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_FINISH;
                break;
            }
            g_battle_gfx_last_loaded_seq_id = id;
            g_battle_gfx_unit_graphics_load_state.buffer = main_heap_alloc(g_battle_gfx_sprite_seq_files[id].size);
            if (g_battle_gfx_unit_graphics_load_state.buffer == 0) {
                break;
            }
            if (main_file_call_build_header(g_battle_gfx_sprite_seq_files[id].sector,
                    g_battle_gfx_sprite_seq_files[id].size, g_battle_gfx_unit_graphics_load_state.buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_QUEUED_LOAD_SPR:
            if (g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index]
                    .spritesheet_id
                >= 0xa0) {
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index]
                    .spritesheet_id = BATTLE_SPRITESHEET_ID_RAMZA_CHAPTER_1;
            }
            if (battle_gfx_is_spritesheet_id_loaded(
                    g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index]
                        .spritesheet_id)
                != 0) {
                g_battle_gfx_unit_graphics_load_state.buffer = 0;
                g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_INSTALL;
                result = 2;
                break;
            }
            g_battle_gfx_unit_graphics_load_state.buffer = main_heap_alloc(
                g_battle_gfx_spritesheet_files[g_main_gfx_unit_graphics_load_queue
                                                   .descriptors[g_battle_gfx_unit_graphics_load_state.index]
                                                   .spritesheet_id]
                    .size);
            if (g_battle_gfx_unit_graphics_load_state.buffer == 0) {
                break;
            }
            if (main_file_call_build_header(
                    g_battle_gfx_spritesheet_files[g_main_gfx_unit_graphics_load_queue
                                                       .descriptors[g_battle_gfx_unit_graphics_load_state.index]
                                                       .spritesheet_id]
                        .sector,
                    g_battle_gfx_spritesheet_files[g_main_gfx_unit_graphics_load_queue
                                                       .descriptors[g_battle_gfx_unit_graphics_load_state.index]
                                                       .spritesheet_id]
                        .size,
                    g_battle_gfx_unit_graphics_load_state.buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        }
        break;
    case BATTLE_GFX_UNIT_LOAD_PHASE_WAIT:
        if (!main_file_is_still_loading()) {
            g_battle_gfx_unit_graphics_load_state.phase++;
        }
        break;
    case BATTLE_GFX_UNIT_LOAD_PHASE_INSTALL:
        switch (g_battle_gfx_unit_graphics_load_state.file_kind) {
        case BATTLE_GFX_QUEUED_LOAD_SHP:
            g_battle_gfx_shp_frame_data_cursor = g_battle_gfx_fallback_shp_frame_data;
            battle_gfx_unpack_unit_shp_data(
                &g_battle_gfx_boss_shp_frame_tables, g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_QUEUED_LOAD_SEQ:
            g_battle_gfx_seq_data_cursor = g_battle_gfx_fallback_seq_data;
            battle_gfx_unpack_unit_seq_data(
                &g_battle_gfx_boss_seq_tables, g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_QUEUED_LOAD_SPR:
            battle_unit_init(
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].map_x,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].map_y,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].map_level,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].facing,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index]
                    .spritesheet_id,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].palette_id,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].runtime_id,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].stats,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].flags,
                g_battle_gfx_unit_graphics_load_state.buffer);
            battle_unit_update_and_animate_units_three_times();
            break;
        }
        g_battle_gfx_unit_graphics_load_state.phase++;
        break;
    case BATTLE_GFX_UNIT_LOAD_PHASE_FINISH:
        if (g_battle_gfx_unit_graphics_load_state.buffer != 0) {
            main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
        }
        g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_START_READ;
        switch (g_battle_gfx_unit_graphics_load_state.file_kind) {
        case BATTLE_GFX_QUEUED_LOAD_SHP:
        case BATTLE_GFX_QUEUED_LOAD_SEQ:
            g_battle_gfx_unit_graphics_load_state.file_kind++;
            break;
        case BATTLE_GFX_QUEUED_LOAD_SPR:
            g_battle_gfx_unit_graphics_load_state.file_kind = BATTLE_GFX_QUEUED_LOAD_SHP;
            g_battle_gfx_unit_graphics_load_state.index++;
            break;
        }
        result = 2;
        break;
    }
    return (g_battle_gfx_unit_graphics_load_state.index < g_main_gfx_unit_graphics_load_queue.count) ? result : 0;
}

/* Cycles the colors of the two deployed-unit palettes (sprite CLUTs 1 and 2).
 *
 * Every fourth animation tick the offset advances through 0..14 and the palettes
 * are uploaded again with colors 1..15 rotated down by that many places; color
 * 0 stays in place, and offset 0 uploads the palettes as stored. */
void battle_gfx_update_unit_palette_animation(void) {
    s32 dst;
    s32 src;
    palette_t* palettes;
    RECT rect; /* never used; retail reserves its 8 bytes */

    g_battle_unit_palette_animation_timer += g_main_system_animation_speed;
    if (g_battle_unit_palette_animation_timer >= 4) {
        g_battle_unit_palette_animation_timer = 0;
        g_battle_unit_palette_animation_offset++;
        if (g_battle_unit_palette_animation_offset >= 15) {
            g_battle_unit_palette_animation_offset = 0;
        }
        if (g_battle_unit_palette_animation_offset != 0) {
            /* Bytewise rotation retains the native stores and first-color 16-bit load. */
            /* dst before src: the reverse order changes the copy loop registers. */
            dst = 2;
            src = g_battle_unit_palette_animation_offset * 2 + 2;
            ((u8*)g_battle_gfx_cycled_deployed_palettes[0])[0] = ((u8*)g_battle_deployed_unit_palettes)[0];
            ((u8*)g_battle_gfx_cycled_deployed_palettes[0])[1] = ((u8*)g_battle_deployed_unit_palettes)[1];
            ((u8*)g_battle_gfx_cycled_deployed_palettes[1])[0] = *(u16*)&g_battle_deployed_unit_palettes[1][0];
            ((u8*)g_battle_gfx_cycled_deployed_palettes[1])[1] = ((u8*)g_battle_deployed_unit_palettes[1])[1];
            /* Colors offset + 1..15 move down to 1.., and colors 1..offset wrap to the top. */
            for (; src < 32; src++, dst++) {
                ((u8*)g_battle_gfx_cycled_deployed_palettes[0])[dst] = ((u8*)g_battle_deployed_unit_palettes)[src];
                ((u8*)g_battle_gfx_cycled_deployed_palettes[1])[dst] = ((u8*)g_battle_deployed_unit_palettes[1])[src];
            }
            for (src = 2, dst = 32 - g_battle_unit_palette_animation_offset * 2;
                src < g_battle_unit_palette_animation_offset * 2 + 2; src++, dst++) {
                ((u8*)g_battle_gfx_cycled_deployed_palettes[0])[dst] = ((u8*)g_battle_deployed_unit_palettes)[src];
                ((u8*)g_battle_gfx_cycled_deployed_palettes[1])[dst] = ((u8*)g_battle_deployed_unit_palettes[1])[src];
            }
            palettes = g_battle_gfx_cycled_deployed_palettes;
        } else {
            palettes = g_battle_deployed_unit_palettes;
        }
        map_load_palette_data(palettes, MAP_CLUT_ROW_SPRITES, 1, 0);
        map_load_palette_data(palettes + 1, MAP_CLUT_ROW_SPRITES, 2, 0);
    }
}
