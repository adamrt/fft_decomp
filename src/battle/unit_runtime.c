/* Unit state by runtime id, including event offsets (written twice in the original), update suppression flags, id
 * conversions, graphics loading, EVTCHR VRAM slots and effect starts. */

#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_gfx.h"
#include "fft/map.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* The original wrote the body of battle_unit_add_event_offset_by_runtime_id a second time as
 * battle_unit_add_event_offset_by_runtime_id_unused. */
/* A second, identical copy of battle_unit_add_event_offset_by_runtime_id. No retail code calls
 * it. */
s32 battle_unit_add_event_offset_by_runtime_id_unused(u32 runtime_id, const SVECTOR* amount) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->event_offset.vx = unit->event_offset.vx + amount->vx;
        unit->event_offset.vy = unit->event_offset.vy + amount->vy;
        unit->event_offset.vz = unit->event_offset.vz + amount->vz;
    } else {
        main_system_handle_pointer_exception(12);
        return 0;
    }
    return 1;
}

/* Add `amount` to a unit's event offset. The copy every caller uses;
 * battle_unit_add_event_offset_by_runtime_id_unused is the same routine compiled twice. */
s32 battle_unit_add_event_offset_by_runtime_id(u32 runtime_id, const SVECTOR* amount) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->event_offset.vx = unit->event_offset.vx + amount->vx;
        unit->event_offset.vy = unit->event_offset.vy + amount->vy;
        unit->event_offset.vz = unit->event_offset.vz + amount->vz;
    } else {
        main_system_handle_pointer_exception(12);
        return 0;
    }
    return 1;
}

/* The unit's event offset (moved by battle_unit_add_event_offset_by_runtime_id). A missing unit
 * raises pointer exception 12 and returns 0. */
SVECTOR* battle_unit_get_event_offset_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit == 0) {
        main_system_handle_pointer_exception(12);
        return 0;
    }
    return &unit->event_offset;
}

/*
 * Move a display unit to the supplied tile and facing.
 *
 * Clears the effect vectors, sets both the current and destination tile, and
 * recomputes the integer and fixed-point world positions from the tile.
 */
s32 battle_unit_place_in_new_location(u32 runtime_id, map_tile_coordinates_t* tile, s32 facing) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->event_offset.vz = 0;
        unit->event_offset.vy = 0;
        unit->event_offset.vx = 0;
        unit->seq_offset.vz = 0;
        unit->seq_offset.vy = 0;
        unit->seq_offset.vx = 0;
        unit->screen_offset.vz = 0;
        unit->screen_offset.vy = 0;
        unit->screen_offset.vx = 0;
        unit->map_x = unit->movement.destination_x = tile->x;
        unit->map_y = unit->movement.destination_y = tile->y;
        unit->map_z = unit->movement.destination_z = tile->layer;
        unit->facing = facing << 10;
        unit->world_coords.vx = tile->x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        unit->world_coords.vz = tile->y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        unit->world_coords.vy = battle_map_calculate_slope_height(&unit->world_coords, unit->map_z);
        unit->real.vx = unit->world_coords.vx * ONE;
        unit->real.vz = unit->world_coords.vz * ONE;
        unit->real.vy = unit->world_coords.vy * ONE;
        if (unit->stats != 0) {
            battle_unit_set_tile_position(unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z, unit->facing);
        }
        return 1;
    }
    return 0;
}

s32 battle_unit_has_runtime_id(u32 runtime_id) {
    return battle_unit_get_by_runtime_id(runtime_id) != 0;
}

/* Sets BATTLE_MOTION_FLAG_SUPPRESS_FACING_UPDATE on a unit and clears its 16-bit sprite
 * display flags. */
void battle_unit_set_facing_update_suppression(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->sprite_display_flags = 0;
        unit->movement.suppress_facing_update = 1;
    }
}

/* Clears BATTLE_MOTION_FLAG_SUPPRESS_FACING_UPDATE on a unit; unlike the set function it leaves
 * the sprite display flags alone. */
void battle_unit_clear_facing_update_suppression(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->movement.suppress_facing_update = 0;
    }
}

/* Mirror a unit's sprite (MirrorSprite event instruction, mode 1). */
void battle_unit_set_horizontal_flip_flag(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->horizontal_flip_flag = BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP;
    }
}

/* Undo MirrorSprite's mirroring of a unit's sprite. */
void battle_unit_clear_horizontal_flip_flag(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->horizontal_flip_flag = 0;
    }
}

/* Stop tile and status palette refreshes on a unit: battle_gfx_check_tile_status_palette_mod skips
 * it while this is set. */
void battle_unit_set_palette_update_suppression(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->movement.suppress_palette_update = 1;
    }
}

void battle_unit_clear_palette_update_suppression(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->movement.suppress_palette_update = 0;
    }
}

/* Reloads a unit's palette with its weather and time-of-day tint and no tile
 * effect level, for the ResetPalette event instruction. An absent unit is
 * skipped. */
void battle_gfx_apply_weather_time_tile_palette_mod_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        battle_gfx_tint_unit_palette_for_weather_and_tile(unit, 0, 1);
    }
}

/* Silence a unit's movement sounds and landing effects (battle_sound_play_movement_sfx and the
 * landing handlers check it). */
void battle_unit_set_movement_effect_suppression(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->movement.suppress_sfx_and_landing_effects = 1;
    }
}

void battle_unit_clear_movement_effect_suppression(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->movement.suppress_sfx_and_landing_effects = 0;
    }
}

/* The battle unit slot of a runtime record, or -1 when the record has no battle
 * stats. A runtime id without a record raises pointer exception 12 and also returns
 * -1. */
s32 battle_unit_get_battle_id_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit_stats_t* stats = unit->stats;

        if (stats != 0) {
            return stats->battle_id;
        }
    } else {
        main_system_handle_pointer_exception(12);
    }
    return -1;
}

/* The runtime id of the record whose battle stats occupy unit slot battle_id, or -1. */
s32 battle_unit_get_runtime_id_by_battle_id(u32 battle_id) {
    battle_unit_t* unit = g_battle_unit_list_head;

    while (unit != 0) {
        unit_stats_t* stats = unit->stats;

        if (stats != 0 && stats->battle_id == battle_id) {
            return unit->runtime_id;
        }
        unit = unit->previous;
    }
    return -1;
}

/* The sprite sheet VRAM slot of the unit in slot battle_id, or -1. */
s32 battle_gfx_get_spritesheet_vram_by_battle_id(u32 battle_id) {
    battle_unit_t* unit = g_battle_unit_list_head;

    while (unit != 0) {
        unit_stats_t* stats = unit->stats;

        if (stats != 0 && stats->battle_id == battle_id) {
            return unit->spritesheet_vram_slot;
        }
        unit = unit->previous;
    }
    return -1;
}

/* The command action record (command_state.ai.data.action) of the unit in slot
 * battle_id, or 0 when it has no runtime record. */
action_command_t* battle_unit_get_runtime_action_command_by_battle_id(u32 battle_id) {
    battle_unit_t* unit = battle_unit_get_by_battle_id(battle_id);

    if (unit == 0) {
        return 0;
    }
    return &unit->command_state.ai.data.action;
}

/* 1 while the runtime unit still has a movement path. A missing unit raises pointer
 * exception 12 and reports 0. */
s32 battle_move_is_unit_moving_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit == 0) {
        main_system_handle_pointer_exception(12);
        return 0;
    }
    return unit->path.count != 0;
}

/* TempWeapon event instruction: battle_gfx_set_thrown_item_graphic_palette for a
 * runtime id. An absent unit is skipped. */
void battle_gfx_set_thrown_item_palette_by_runtime_id(u32 item_id, u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        battle_gfx_set_thrown_item_graphic_palette(item_id, unit);
    }
}

s32 battle_script_opcode_add_ghost_unit(
    s32 map_x, s32 map_y, s32 map_level, s16 facing, s16 spritesheet_id, s32 runtime_id, s32 flags) {
    s32 ret;

    if (battle_unit_get_by_runtime_id(runtime_id) != 0) {
        ret = 0;
    } else {
        g_main_gfx_unit_graphics_load_queue.count = 0;
        g_battle_gfx_unit_graphics_load_state.file_kind = BATTLE_GFX_UNIT_LOAD_KIND_FIRST;
        g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_START_READ;
        g_battle_gfx_unit_graphics_load_state.index = 0;
        g_battle_gfx_unit_graphics_load_state.buffer = 0;
        battle_gfx_append_unit_graphics_load_descriptor(
            map_x, map_y, map_level, facing, spritesheet_id, 0, runtime_id, 0, flags);
        ret = 1;
    }
    return ret;
}

/* Restarts the unit graphics loader with one descriptor, for a unit an event adds
 * during the battle: the unit is put on the field
 * (battle_unit_set_enemy_level_data_by_battle_id) and queued at its map position
 * and facing with its sheet and palette, the caller's flags and runtime id 0xff,
 * which takes the first free runtime slot. Always returns 1. */
s32 battle_gfx_load_unit_graphics_by_battle_id(s32 battle_id, s32 flags) {
    unit_stats_t* unit;

    g_main_gfx_unit_graphics_load_queue.count = 0;
    g_battle_gfx_unit_graphics_load_state.file_kind = BATTLE_GFX_UNIT_LOAD_KIND_FIRST;
    g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_START_READ;
    g_battle_gfx_unit_graphics_load_state.index = 0;
    g_battle_gfx_unit_graphics_load_state.buffer = 0;
    unit = battle_unit_get_stats_from_battle_id(battle_id);
    battle_unit_set_enemy_level_data_by_battle_id(battle_id);
    battle_gfx_append_unit_graphics_load_descriptor(unit->x, unit->position.y, unit->position.higher_elevation,
        unit->position.facing, unit->spritesheet_id, unit->job_portrait_palette, 0xFF, unit, flags);
    return 1;
}

/* The underlying loader handles ordinary SPR as well as special SHP/SEQ.
 * Keep polling only its immediate-progress result, not deferred work. */
s32 battle_gfx_poll_unit_graphics_load(void) {
    s32 status;

    do {
        status = battle_gfx_step_queued_unit_graphics_load();
    } while (status == 2);
    return status;
}

/* Makes a unit visible: enables its drawing, triggers its sprite graphic and,
 * when it has battle stats, runs battle_unit_set_enemy_level_data_by_battle_id.
 * Returns 0 when no record has the runtime id. */
s32 battle_unit_draw_and_activate_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);
    unit_stats_t* stats;

    if (unit != 0) {
        stats = unit->stats;
        unit->draw_enabled = 1;
        unit->animation.trigger = 1;
        if (stats != 0) {
            battle_unit_set_enemy_level_data_by_battle_id(stats->battle_id);
        }
        return 1;
    }
    return 0;
}

/* Hides a unit: disables its drawing and sprite trigger, undoing
 * battle_unit_draw_and_activate_by_runtime_id (Erase event instruction). Returns 0 when no record
 * has the runtime id. */
s32 battle_gfx_reset_unit_graphic_trigger(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->draw_enabled = 0;
        unit->animation.trigger = 0;
        return 1;
    }
    return 0;
}

/* Hides every unit that has the Jump status. */
void battle_gfx_reset_jumping_unit_graphic_triggers(void) {
    battle_unit_t* unit = g_battle_unit_list_head;

    while (unit != 0) {
        if ((unit->status_flags_5_6 & BATTLE_UNIT_STATUS_JUMP) != 0) {
            battle_gfx_reset_unit_graphic_trigger(unit->runtime_id);
        }
        unit = unit->previous;
    }
}

/* battle_unit_remove by runtime id. Returns 1, or 0 after raising pointer
 * exception 12 when no record has the id. */
s32 battle_unit_remove_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        battle_unit_remove(unit);
        return 1;
    }
    main_system_handle_pointer_exception(0xC);
    return 0;
}

/* Calls battle_gfx_start_unit_fade_out with the same
 * arguments; the event instructions that remove a unit start its fade-out here. */
void battle_gfx_start_event_unit_fade_out(s32 runtime_id, s32 mode, s16 red, s16 green, s16 blue) {
    battle_gfx_start_unit_fade_out(runtime_id, mode, red, green, blue);
}

/* Unpack a loaded block of EVTCHR.BIN into an EVTCHR slot whose image
 * buffer is free; returns 0 when it is not.
 *
 * The block is, in order: 0x40 sequence offsets and 0x400 bytes of
 * sequences, 0x28 frame offsets and 0x1e0 bytes of frames, sixteen CLUTs and
 * the 0x6400-byte image. The offsets become pointers into the slot's copies
 * (-1 maps to the start of the data), and the frames are stored twice, once
 * for each of the slot's frame tables.
 *
 * The integer sum with the decayed data array keeps the target's
 * offset-plus-0x108-then-base order; a byte pointer for the palettes keeps
 * that copy an end-pointer loop like the others. */
s32 battle_gfx_unpack_evtchr_file_to_vram_slot(s32 index, const evtchr_file_record_t* record) {
    s32 i;
    s32 pos;
    s32 offset;
    const u8* word;
    u8* dst;
    const u8* file = (const u8*)record;

    if (g_battle_gfx_vram_slots[index].image_buffer_state == BATTLE_GFX_VRAM_SLOT_BUFFER_FREE) {
        /* Separate named field bases fold the retail running byte position. */
        pos = offsetof(evtchr_file_record_t, sequence_offsets);
        for (i = 0; i < 64; i++) {
            word = file + pos;
            offset = word[0] + (word[1] << 8) + (word[2] << 16) + (word[3] << 24);
            if (offset == -1) {
                offset = 0;
            }
            g_battle_gfx_vram_slots[index].seq.sequences[i]
                = (u8*)(offset + (s32)g_battle_gfx_vram_slots[index].seq.sequence_data);
            pos += 4;
        }
        for (i = 0; i < 0x400; i++) {
            g_battle_gfx_vram_slots[index].seq.sequence_data[i] = file[pos++];
        }
        for (i = 0; i < 40; i++) {
            word = file + pos;
            offset = word[0] + (word[1] << 8) + (word[2] << 16) + (word[3] << 24);
            if (offset == -1) {
                offset = 0;
            }
            g_battle_gfx_vram_slots[index].shp.tables.frames[i]
                = (battle_gfx_source_frame_t*)(offset + (s32)g_battle_gfx_vram_slots[index].shp.data);
            g_battle_gfx_vram_slots[index].shp.tables.submerged_frames[i]
                = (battle_gfx_source_frame_t*)(offset + (s32)g_battle_gfx_vram_slots[index].shp.submerged_data);
            pos += 4;
        }
        for (i = 0; i < 0x1e0; i++) {
            g_battle_gfx_vram_slots[index].shp.data[i] = file[pos];
            g_battle_gfx_vram_slots[index].shp.submerged_data[i] = file[pos];
            pos++;
        }
        dst = (u8*)g_battle_gfx_vram_slots[index].clut;
        for (i = 0; i < 0x200; i++) {
            dst[i] = file[pos++];
        }
        for (i = 0; i < 0x6400; i++) {
            g_battle_gfx_vram_slots[index].image_data[i] = file[pos++];
        }
        g_battle_gfx_vram_slots[index].image_buffer_state = BATTLE_GFX_VRAM_SLOT_BUFFER_EVTCHR;
        g_battle_gfx_vram_slots[index].evtchr_load_state = 3;
        return 1;
    }
    return 0;
}

/* SaveEVTCHR: upload the slot's unpacked EVTCHR image to its page, if the
 * page is free. */
s32 battle_gfx_save_evtchr_slot(s32 slot_index) {
    if (g_battle_gfx_vram_slots[slot_index].owner == BATTLE_GFX_VRAM_SLOT_OWNER_NONE) {
        LoadImage(
            &g_battle_gfx_vram_slots[slot_index].image_rect, (u32*)g_battle_gfx_vram_slots[slot_index].image_data);
        g_battle_gfx_vram_slots[slot_index].owner = BATTLE_GFX_VRAM_SLOT_OWNER_EVTCHR;
        return 1;
    }

    return 0;
}

/* Free a slot's page if it holds an EVTCHR image. */
s32 battle_gfx_clear_saved_evtchr_slot(s32 slot_index) {
    if (g_battle_gfx_vram_slots[slot_index].owner == BATTLE_GFX_VRAM_SLOT_OWNER_EVTCHR) {
        g_battle_gfx_vram_slots[slot_index].owner = BATTLE_GFX_VRAM_SLOT_OWNER_NONE;
        return 1;
    }

    return 0;
}

/* Free a slot's image buffer if it holds an unpacked EVTCHR block. */
s32 battle_gfx_clear_loaded_evtchr_slot(s32 slot_index) {
    if (g_battle_gfx_vram_slots[slot_index].image_buffer_state == BATTLE_GFX_VRAM_SLOT_BUFFER_EVTCHR) {
        g_battle_gfx_vram_slots[slot_index].image_buffer_state = BATTLE_GFX_VRAM_SLOT_BUFFER_FREE;
        g_battle_gfx_vram_slots[slot_index].evtchr_load_state = 0;
        return 1;
    }

    return 0;
}

/* Put a unit's attack image on the page of a slot that holds an EVTCHR image,
 * for the event instruction that asks for it: decompress the image into a
 * free buffer, upload it and give the slot to the unit. Returns 0 when the
 * slot is in another state or no buffer is free. The buffer marked as
 * uploading is the slot's own, whichever one was used. */
s32 battle_gfx_load_unit_attack_image_into_evtchr_slot(s32 runtime_id, s32 slot) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);
    s32 i;
    u8* image_data;
    RECT rect;

    if (g_battle_gfx_vram_slots[slot].owner == BATTLE_GFX_VRAM_SLOT_OWNER_EVTCHR) {
        for (i = 0; i < 2; i++) {
            if (g_battle_gfx_vram_slots[i].image_buffer_state == BATTLE_GFX_VRAM_SLOT_BUFFER_FREE) {
                image_data = g_battle_gfx_vram_slots[i].image_data;
                battle_gfx_decompress_attack_spritesheet(
                    g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot].compressed_attack_image, image_data);
                LoadImage(&g_battle_gfx_vram_slots[slot].image_rect, (u32*)image_data);
                rect.x = (unit->spritesheet_vram_slot >> 3) * 64 + 0x340;
                rect.y = (unit->spritesheet_vram_slot & 7) * 32 + 0x100;
                rect.w = 0x40;
                rect.h = 0x20;
                MoveImage(&rect, g_battle_gfx_vram_slots[slot].image_rect.x,
                    g_battle_gfx_vram_slots[slot].image_rect.y + 200);
                g_battle_gfx_vram_slots[slot].owner = runtime_id + BATTLE_GFX_VRAM_SLOT_OWNER_ATTACK_IMAGE;
                g_battle_gfx_vram_slots[slot].image_buffer_state = BATTLE_GFX_VRAM_SLOT_UPLOAD_FRAMES;
                g_battle_gfx_vram_slots[slot].evtchr_load_state = 1;
                return 1;
            }
        }
        /* Retail has this exit and the one below as two separate returns. */
        return 0;
    }
    return 0;
}

/* Free a slot's page if it holds a unit's attack image. */
s32 battle_gfx_release_reserved_vram_slot(s32 slot_index) {
    if ((g_battle_gfx_vram_slots[slot_index].owner & BATTLE_GFX_VRAM_SLOT_OWNER_ATTACK_IMAGE) != 0) {
        g_battle_gfx_vram_slots[slot_index].owner = BATTLE_GFX_VRAM_SLOT_OWNER_NONE;
        g_battle_gfx_vram_slots[slot_index].evtchr_load_state = 0;
        return 1;
    }

    return 0;
}

/* Free both EVTCHR slots, pages and image buffers. */
s32 battle_gfx_init_evtchr_vram_slots(void) {
    s32 slot_index;

    for (slot_index = 0; slot_index < 2; slot_index++) {
        g_battle_gfx_vram_slots[slot_index].owner = BATTLE_GFX_VRAM_SLOT_OWNER_NONE;
        g_battle_gfx_vram_slots[slot_index].image_buffer_state = BATTLE_GFX_VRAM_SLOT_BUFFER_FREE;
        g_battle_gfx_vram_slots[slot_index].evtchr_load_state = 0;
    }

    return 1;
}

/*
 * Describe both battle VRAM slots: the owning unit's spritesheet (or the
 * free/saved marker) and the EVTCHR load marker, collapsed to 0xfd..0xff.
 *
 * A struct index into g_battle_gfx_spritesheet_slots keeps the table address out of the loop
 * invariants; the flat byte-array form hoists it into a saved register.
 */
s32 battle_gfx_get_vram_slot_summary(battle_gfx_vram_slot_summary_t* summary) {
    s32 i;
    u32 owner;
    u32 marker;

    for (i = 0; i < 2; i++) {
        owner = g_battle_gfx_vram_slots[i].owner;
        if (owner == BATTLE_GFX_VRAM_SLOT_OWNER_NONE) {
            summary[i].spritesheet_id = 0xff;
        } else if (owner == BATTLE_GFX_VRAM_SLOT_OWNER_EVTCHR) {
            summary[i].spritesheet_id = 0xfe;
        } else if (owner & BATTLE_GFX_VRAM_SLOT_OWNER_ATTACK_IMAGE) {
            summary[i].spritesheet_id
                = g_battle_gfx_spritesheet_slots[battle_unit_get_by_runtime_id(g_battle_gfx_vram_slots[i].owner & 0x7f)
                                                     ->spritesheet_vram_slot]
                      .spritesheet_id;
        } else {
            summary[i].spritesheet_id = 0xfd;
        }
        marker = g_battle_gfx_vram_slots[i].image_buffer_state;
        if (marker != BATTLE_GFX_VRAM_SLOT_BUFFER_EVTCHR) {
            if (marker == BATTLE_GFX_VRAM_SLOT_BUFFER_FREE) {
                summary[i].evtchr_state = 0xff;
                continue;
            }
            marker = 0xfd;
        }
        summary[i].evtchr_state = marker;
    }
    return 1;
}

/* Give a unit CLUT `clut_index` of a slot's EVTCHR block, and the one eight
 * after it as its second. */
void battle_gfx_set_evtchr_palette_by_runtime_id(s32 runtime_id, s32 slot_index, s32 clut_index) {
    battle_map_load_palette_data(
        &g_battle_gfx_vram_slots[slot_index].clut[clut_index], MAP_CLUT_ROW_UNITS, runtime_id, 0);
    battle_map_load_palette_data(
        &g_battle_gfx_vram_slots[slot_index].clut[clut_index + 8], MAP_CLUT_ROW_UNITS_SECOND, runtime_id, 0);
}

/*
 * Give a unit's CLUT, texture page, graphic height and v offset, and return
 * the SHP frame it is showing: the submerged one for a rider. The crystal
 * and later status sheets share the OTHER texture page.
 */
battle_gfx_source_frame_t* battle_gfx_get_unit_sprite_frame_and_vram_ids(
    u32 runtime_id, battle_unit_sprite_query_t* out) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_runtime_id(runtime_id);
    if (unit == 0) {
        return 0;
    }

    out->clut = unit->clut;
    if (unit->spritesheet_id >= BATTLE_SPRITESHEET_ID_CRYSTAL) {
        out->tpage = BATTLE_GFX_TPAGE_OTHER;
    } else {
        out->tpage = unit->tpage;
    }
    out->graphic_height = g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height;
    out->graphic_y_offset = unit->special_graphic_y_offset;

    if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_RIDER) {
        return unit->animation.shp->frames[unit->animation.frame];
    } else {
        return unit->animation.shp->submerged_frames[unit->animation.frame];
    }
}

/* The sprite height of the unit's sheet, or 0 for an absent, dead, crystal or
 * treasure unit. */
static u32 battle_gfx_get_unit_spritesheet_height(battle_unit_t* unit) {
    if (unit == 0) {
        return 0;
    }
    if ((unit->status_flags_5_6 & (BATTLE_UNIT_STATUS_CRYSTAL | BATTLE_UNIT_STATUS_TREASURE)) != 0) {
        return 0;
    }
    if ((unit->status_flags_1_4 & BATTLE_UNIT_STATUS_DEAD) != 0) {
        return 0;
    }
    return g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height;
}

s32 battle_gfx_get_unit_spritesheet_height_by_runtime_id(u32 runtime_id) {
    return battle_gfx_get_unit_spritesheet_height(battle_unit_get_by_runtime_id(runtime_id));
}

struct battle_unit;

s32 battle_gfx_get_unit_spritesheet_height_by_unit_id(u32 unit_id) {
    return battle_gfx_get_unit_spritesheet_height(battle_unit_get_by_battle_id(unit_id));
}

/* Start an ability's effect cast by runtime unit 0 on runtime unit 1 with a normal hit. No retail
 * code calls it. */
void battle_effect_start_basic_ability_animation(s32 ability_id) {
    effect_secondary_init_t effect_data;

    effect_data.target_count = 1;
    effect_data.targets[EFFECT_CASTER_ENTRY].target_type = EFFECT_TARGET_TYPE_UNIT;
    effect_data.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id = 0;
    effect_data.targets[0].target_type = EFFECT_TARGET_TYPE_UNIT;
    effect_data.targets[0].data.unit.runtime_id = 1;
    effect_data.targets[0].data.unit.result_animation = EFFECT_HIT_NORMAL;
    effect_data.spectator_count = 0;
    battle_effect_set_ability_animation(0, ability_id, &effect_data);
}

/* Starts an ability's effect outside an action, on a unit, on a tile or
 * on a unit that is also its caster. */
void battle_effect_start_new(s32 ability_id, s32 target_mode, effect_start_target_data_t target) {
    effect_secondary_init_t effect_data;

    effect_data.target_count = 1;
    effect_data.targets[EFFECT_CASTER_ENTRY].target_type = EFFECT_TARGET_TYPE_UNIT;
    effect_data.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id = 0;

    if (target_mode == EFFECT_START_ON_UNIT) {
        effect_data.targets[0].target_type = EFFECT_TARGET_TYPE_UNIT;
        effect_data.targets[0].data.unit.runtime_id = target.runtime_id;
        effect_data.targets[0].data.unit.result_animation = EFFECT_HIT_NORMAL;
    } else if (target_mode == EFFECT_START_ON_TILE) {
        effect_data.targets[0].target_type = EFFECT_TARGET_TYPE_TILE;
        effect_data.targets[0].data.tile.vx = target.tile->vx;
        effect_data.targets[0].data.tile.vz = target.tile->vz;
        effect_data.targets[0].data.tile.vy = target.tile->vy;
    } else {
        effect_data.targets[0].target_type = EFFECT_TARGET_TYPE_UNIT;
        effect_data.targets[0].data.unit.runtime_id = target.runtime_id;
        effect_data.targets[0].data.unit.result_animation = EFFECT_HIT_NORMAL;
        effect_data.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id = target.runtime_id;
    }

    effect_data.spectator_count = 0;
    battle_effect_set_ability_animation(0, ability_id, &effect_data);
}

/* battle_effect_init_data as a 0 or 1 result: the event script's EffectStart
 * instruction calls it on the main stack each frame until it returns 0. */
s32 battle_effect_try_init_data(void) {
    /* The target loads no argument; the parameter is the $a0 value unhandled states return. */
    return battle_effect_init_data() != 0;
}

/* battle_effect_play as a g_thread_call_target: script_play_effect_and_wait runs it on the main
 * stack. */
void battle_effect_call_play(void) {
    battle_effect_play();
}
