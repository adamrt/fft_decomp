/* Unit state by runtime id, including event offsets (written twice in the original), update suppression flags, id
 * conversions, graphics loading, EVTCHR VRAM slots and effect starts. */

#include "fft/unit_runtime.h"
#include "fft/animation.h"
#include "fft/battle_map.h"
#include "fft/battle_map_palette.h"
#include "fft/effect.h"
#include "fft/effect_script.h"
#include "fft/gfx.h"
#include "fft/move.h"
#include "fft/script.h"
#include "fft/sprite.h"
#include "fft/sprite_sheet.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* A second, identical copy of unit_add_event_offset_by_runtime_id, placed just before it.
 * No retail code calls it. */
s32 unit_add_event_offset_by_runtime_id_unused(u32 runtime_id, const SVECTOR* amount) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->event_offset.vx = unit->event_offset.vx + amount->vx;
        unit->event_offset.vy = unit->event_offset.vy + amount->vy;
        unit->event_offset.vz = unit->event_offset.vz + amount->vz;
    } else {
        system_handle_pointer_exception(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
        return 0;
    }
    return 1;
}

/* Add `amount` to a unit's event offset. The copy every caller uses;
 * unit_add_event_offset_by_runtime_id_unused is the same routine compiled twice. */
s32 unit_add_event_offset_by_runtime_id(u32 runtime_id, const SVECTOR* amount) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->event_offset.vx = unit->event_offset.vx + amount->vx;
        unit->event_offset.vy = unit->event_offset.vy + amount->vy;
        unit->event_offset.vz = unit->event_offset.vz + amount->vz;
    } else {
        system_handle_pointer_exception(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
        return 0;
    }
    return 1;
}

/* The unit's event offset (moved by unit_add_event_offset_by_runtime_id). A missing unit
 * raises a pointer exception and returns 0. */
const SVECTOR* unit_get_event_offset_by_runtime_id(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit == 0) {
        system_handle_pointer_exception(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
        return 0;
    }
    return &unit->event_offset;
}

/* Warp a unit to `tile` facing `facing` (in quarter turns), for event placement and warps:
 * clears its event, sequence and screen offsets, sets both its current and destination tile and
 * recomputes its world position from the tile center and slope height. A unit with battle stats
 * also re-registers its tile there; that call passes the 16-bit facing, whose low byte is always
 * 0, so the stats record facing 0 (south) whatever was asked. Returns 0 when no record has the
 * runtime id. */
s32 unit_place_in_new_location(u32 runtime_id, battle_map_tile_coordinates_t* tile, s32 facing) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

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
        unit->map_layer = unit->movement.destination_layer = tile->layer;
        unit->facing = facing << 10;
        unit->world_coords.vx = tile->x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        unit->world_coords.vz = tile->y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        unit->world_coords.vy = battle_map_calculate_slope_height(&unit->world_coords, unit->map_layer);
        unit->real.vx = unit->world_coords.vx * ONE;
        unit->real.vz = unit->world_coords.vz * ONE;
        unit->real.vy = unit->world_coords.vy * ONE;
        if (unit->stats != 0) {
            unit_set_tile_position(unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_layer, unit->facing);
        }
        return 1;
    }
    return 0;
}

/* Return 1 when a battle unit with this runtime id exists, else 0. */
s32 unit_has_runtime_id(u32 runtime_id) {
    return unit_get_by_runtime_id(runtime_id) != 0;
}

/* Sets UNIT_MOTION_FLAG_SUPPRESS_FACING_UPDATE on a unit and clears its 16-bit sprite
 * display flags. */
void unit_set_facing_update_suppression(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->sprite_display_flags = 0;
        unit->movement.suppress_facing_update = 1;
    }
}

/* Clears UNIT_MOTION_FLAG_SUPPRESS_FACING_UPDATE on a unit; unlike the set function it leaves
 * the sprite display flags alone. */
void unit_clear_facing_update_suppression(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->movement.suppress_facing_update = 0;
    }
}

/* Mirror a unit's sprite (MirrorSprite event instruction, mode 1). */
void sprite_set_horizontal_flip_flag(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->horizontal_flip_flag = SPRITE_DISPLAY_HORIZONTAL_FLIP;
    }
}

/* Undo MirrorSprite's mirroring of a unit's sprite. */
void sprite_clear_horizontal_flip_flag(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->horizontal_flip_flag = 0;
    }
}

/* Stop tile and status palette refreshes on a unit: sprite_palette_check_tile_status_mod skips
 * it while this is set. */
void unit_set_palette_update_suppression(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->movement.suppress_palette_update = 1;
    }
}

/* Let tile and status palette refreshes reach a unit again; the counterpart of
 * unit_set_palette_update_suppression. */
void unit_clear_palette_update_suppression(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->movement.suppress_palette_update = 0;
    }
}

/* Reloads a unit's palette with its weather and time-of-day tint and no tile
 * effect level, for the ResetPalette event instruction. An absent unit is
 * skipped. */
void sprite_palette_reset_by_runtime_id(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        sprite_palette_tint_unit(unit, 0, 1);
    }
}

/* Silence a unit's sounds and landing effects (unit_play_sfx and the
 * landing handlers check it). */
void unit_set_movement_effect_suppression(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->movement.suppress_sfx_and_landing_effects = 1;
    }
}

/* Restore a unit's sounds and landing effects; the counterpart of
 * unit_set_movement_effect_suppression. */
void unit_clear_movement_effect_suppression(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->movement.suppress_sfx_and_landing_effects = 0;
    }
}

/* The battle unit slot of a runtime record, or -1 when the record has no battle
 * stats. A runtime id without a record raises a pointer exception and also returns
 * -1. */
s32 unit_get_battle_id_by_runtime_id(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit_stats_t* stats = unit->stats;

        if (stats != 0) {
            return stats->battle_id;
        }
    } else {
        system_handle_pointer_exception(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
    }
    return -1;
}

/* The runtime id of the record whose battle stats occupy unit slot battle_id, or -1. */
s32 unit_get_runtime_id_by_battle_id(u32 battle_id) {
    unit_t* unit = g_unit_list_head;

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
s32 sprite_sheet_get_slot_by_battle_id(u32 battle_id) {
    unit_t* unit = g_unit_list_head;

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
action_command_t* unit_get_runtime_action_command_by_battle_id(u32 battle_id) {
    unit_t* unit = unit_get_by_battle_id(battle_id);

    if (unit == 0) {
        return 0;
    }
    return &unit->command_state.ai.data.action;
}

/* 1 while the runtime unit still has a movement path. A missing unit raises a pointer
 * exception and reports 0. */
s32 move_is_unit_moving_by_runtime_id(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit == 0) {
        system_handle_pointer_exception(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
        return 0;
    }
    return unit->path.count != 0;
}

/* TempWeapon event instruction: sprite_palette_set_thrown_item for a
 * runtime id. An absent unit is skipped. */
void sprite_palette_set_thrown_item_by_runtime_id(u32 item_id, u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        sprite_palette_set_thrown_item(item_id, unit);
    }
}

/* AddGhostUnit: when `runtime_id` is free, restart the unit graphics load queue with one
 * descriptor for a sprite with no unit stats at the given tile and facing, and return
 * 1; return 0 when the id is taken. */
s32 script_opcode_47_add_ghost_unit(
    s32 map_x, s32 map_y, s32 map_level, s16 facing, s16 spritesheet_id, s32 runtime_id, s32 flags) {
    s32 ret;

    if (unit_get_by_runtime_id(runtime_id) != 0) {
        ret = 0;
    } else {
        g_unit_graphics_load_queue.count = 0;
        g_sprite_load_state.file_kind = SPRITE_LOAD_KIND_FIRST;
        g_sprite_load_state.phase = SPRITE_LOAD_PHASE_START_READ;
        g_sprite_load_state.index = 0;
        g_sprite_load_state.buffer = 0;
        sprite_queue_unit_graphics_load(map_x, map_y, map_level, facing, spritesheet_id, 0, runtime_id, 0, flags);
        ret = 1;
    }
    return ret;
}

/* Restarts the unit graphics loader with one descriptor, for a unit an event adds
 * during the battle: the unit is put on the field
 * (unit_put_on_field) and queued at its map position
 * and facing with its sheet and palette, the caller's flags and runtime id 0xff,
 * which takes the first free runtime slot. Always returns 1. */
s32 sprite_load_unit_graphics_by_battle_id(s32 battle_id, s32 flags) {
    unit_stats_t* unit;

    g_unit_graphics_load_queue.count = 0;
    g_sprite_load_state.file_kind = SPRITE_LOAD_KIND_FIRST;
    g_sprite_load_state.phase = SPRITE_LOAD_PHASE_START_READ;
    g_sprite_load_state.index = 0;
    g_sprite_load_state.buffer = 0;
    unit = unit_get_stats(battle_id);
    unit_put_on_field(battle_id);
    sprite_queue_unit_graphics_load(unit->x, unit->position.y, unit->position.higher_elevation, unit->position.facing,
        unit->spritesheet_id, unit->job_portrait_palette, 0xFF, unit, flags);
    return 1;
}

/* Advance the queued unit graphics load as far as it goes this frame: a step that ends on a step
 * boundary (2) continues at once, and the loop stops on 1 (a read in progress) or 0 (every
 * descriptor done), which it returns. */
s32 sprite_poll_unit_graphics_load(void) {
    s32 status;

    do {
        status = sprite_load_step_queued();
    } while (status == 2);
    return status;
}

/* Makes a unit visible: enables its drawing, triggers its sprite graphic and,
 * when it has battle stats, puts it on the field (unit_put_on_field).
 * Returns 0 when no record has the runtime id. */
s32 unit_draw_and_activate_by_runtime_id(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);
    unit_stats_t* stats;

    if (unit != 0) {
        stats = unit->stats;
        unit->draw_enabled = 1;
        unit->animation.trigger = 1;
        if (stats != 0) {
            unit_put_on_field(stats->battle_id);
        }
        return 1;
    }
    return 0;
}

/* Hides a unit: disables its drawing and sprite trigger, undoing
 * unit_draw_and_activate_by_runtime_id (Erase event instruction). Returns 0 when no record
 * has the runtime id. */
s32 unit_hide_by_runtime_id(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->draw_enabled = 0;
        unit->animation.trigger = 0;
        return 1;
    }
    return 0;
}

/* Hides every unit that has the Jump status. */
void unit_hide_all_jumping(void) {
    unit_t* unit = g_unit_list_head;

    while (unit != 0) {
        if ((unit->status_flags_5_6 & UNIT_STATUS_JUMP) != 0) {
            unit_hide_by_runtime_id(unit->runtime_id);
        }
        unit = unit->previous;
    }
}

/* unit_remove by runtime id. Returns 1, or 0 after raising a pointer
 * exception when no record has the id. */
s32 unit_remove_by_runtime_id(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit_remove(unit);
        return 1;
    }
    system_handle_pointer_exception(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
    return 0;
}

/* Calls unit_fade_out_start with the same
 * arguments; the event instructions that remove a unit start its fade-out here. */
void unit_fade_out_start_for_event(s32 runtime_id, s32 mode, s16 red, s16 green, s16 blue) {
    unit_fade_out_start(runtime_id, mode, red, green, blue);
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
 * that copy an end-pointer loop like the others. Every access indexes
 * g_sprite_evtchr_slots[index] again: a slot pointer local compiles far
 * shorter than the retail code. */
s32 sprite_evtchr_unpack_to_slot(s32 index, const evtchr_file_record_t* record) {
    s32 i;
    s32 pos;
    s32 offset;
    const u8* word;
    u8* dst;
    const u8* file = (const u8*)record;

    if (g_sprite_evtchr_slots[index].image_buffer_state == SPRITE_EVTCHR_SLOT_BUFFER_FREE) {
        /* Separate named field bases fold the retail running byte position. */
        pos = offsetof(evtchr_file_record_t, sequence_offsets);
        for (i = 0; i < 64; i++) {
            word = file + pos;
            offset = word[0] + (word[1] << 8) + (word[2] << 16) + (word[3] << 24);
            if (offset == -1) {
                offset = 0;
            }
            g_sprite_evtchr_slots[index].seq.sequences[i]
                = (u8*)(offset + (s32)g_sprite_evtchr_slots[index].seq.sequence_data);
            pos += 4;
        }
        for (i = 0; i < 0x400; i++) {
            g_sprite_evtchr_slots[index].seq.sequence_data[i] = file[pos++];
        }
        for (i = 0; i < 40; i++) {
            word = file + pos;
            offset = word[0] + (word[1] << 8) + (word[2] << 16) + (word[3] << 24);
            if (offset == -1) {
                offset = 0;
            }
            g_sprite_evtchr_slots[index].shp.tables.frames[i]
                = (sprite_frame_t*)(offset + (s32)g_sprite_evtchr_slots[index].shp.data);
            g_sprite_evtchr_slots[index].shp.tables.submerged_frames[i]
                = (sprite_frame_t*)(offset + (s32)g_sprite_evtchr_slots[index].shp.submerged_data);
            pos += 4;
        }
        for (i = 0; i < 0x1e0; i++) {
            g_sprite_evtchr_slots[index].shp.data[i] = file[pos];
            g_sprite_evtchr_slots[index].shp.submerged_data[i] = file[pos];
            pos++;
        }
        dst = (u8*)g_sprite_evtchr_slots[index].clut;
        for (i = 0; i < 0x200; i++) {
            dst[i] = file[pos++];
        }
        for (i = 0; i < 0x6400; i++) {
            g_sprite_evtchr_slots[index].image_data[i] = file[pos++];
        }
        g_sprite_evtchr_slots[index].image_buffer_state = SPRITE_EVTCHR_SLOT_BUFFER_EVTCHR;
        g_sprite_evtchr_slots[index].evtchr_load_state = 3;
        return 1;
    }
    return 0;
}

/* SaveEVTCHR: upload the slot's unpacked EVTCHR image to its page, if the
 * page is free. */
s32 sprite_evtchr_save_slot(s32 slot_index) {
    if (g_sprite_evtchr_slots[slot_index].owner == SPRITE_EVTCHR_SLOT_OWNER_NONE) {
        LoadImage(&g_sprite_evtchr_slots[slot_index].image_rect, (u32*)g_sprite_evtchr_slots[slot_index].image_data);
        g_sprite_evtchr_slots[slot_index].owner = SPRITE_EVTCHR_SLOT_OWNER_EVTCHR;
        return 1;
    }

    return 0;
}

/* Free a slot's page if it holds an EVTCHR image. */
s32 sprite_evtchr_clear_saved_slot(s32 slot_index) {
    if (g_sprite_evtchr_slots[slot_index].owner == SPRITE_EVTCHR_SLOT_OWNER_EVTCHR) {
        g_sprite_evtchr_slots[slot_index].owner = SPRITE_EVTCHR_SLOT_OWNER_NONE;
        return 1;
    }

    return 0;
}

/* Free a slot's image buffer if it holds an unpacked EVTCHR block. */
s32 sprite_evtchr_clear_loaded_slot(s32 slot_index) {
    if (g_sprite_evtchr_slots[slot_index].image_buffer_state == SPRITE_EVTCHR_SLOT_BUFFER_EVTCHR) {
        g_sprite_evtchr_slots[slot_index].image_buffer_state = SPRITE_EVTCHR_SLOT_BUFFER_FREE;
        g_sprite_evtchr_slots[slot_index].evtchr_load_state = 0;
        return 1;
    }

    return 0;
}

/* Put a unit's attack image on the page of a slot that holds an EVTCHR image,
 * for the event instruction that asks for it: decompress the image into a
 * free buffer, upload it and give the slot to the unit. Returns 0 when the
 * slot is in another state or no buffer is free. The buffer marked as
 * uploading is the slot's own, whichever one was used. */
s32 sprite_evtchr_load_unit_attack_image(s32 runtime_id, s32 slot) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);
    s32 i;
    u8* image_data;
    RECT rect;

    if (g_sprite_evtchr_slots[slot].owner == SPRITE_EVTCHR_SLOT_OWNER_EVTCHR) {
        for (i = 0; i < SPRITE_EVTCHR_SLOT_COUNT; i++) {
            if (g_sprite_evtchr_slots[i].image_buffer_state == SPRITE_EVTCHR_SLOT_BUFFER_FREE) {
                image_data = g_sprite_evtchr_slots[i].image_data;
                sprite_decompress_attack_image(
                    g_sprite_sheet_slots[unit->spritesheet_vram_slot].compressed_attack_image, image_data);
                LoadImage(&g_sprite_evtchr_slots[slot].image_rect, (u32*)image_data);
                /* The unit's 64x32 block in the spritesheet area (its sheet rows below the
                 * first 256, the portrait and lower body) goes 200 lines below the image. */
                rect.x = (unit->spritesheet_vram_slot >> 3) * SPRITE_SHEET_STRIP_WIDTH + SPRITE_SHEET_STRIP_VRAM_X;
                rect.y = (unit->spritesheet_vram_slot & 7) * SPRITE_SHEET_STRIP_HEIGHT + SPRITE_SHEET_STRIP_VRAM_Y;
                rect.w = SPRITE_SHEET_STRIP_WIDTH;
                rect.h = SPRITE_SHEET_STRIP_HEIGHT;
                MoveImage(
                    &rect, g_sprite_evtchr_slots[slot].image_rect.x, g_sprite_evtchr_slots[slot].image_rect.y + 200);
                g_sprite_evtchr_slots[slot].owner = runtime_id + SPRITE_EVTCHR_SLOT_OWNER_ATTACK_IMAGE;
                g_sprite_evtchr_slots[slot].image_buffer_state = SPRITE_EVTCHR_SLOT_UPLOAD_FRAMES;
                g_sprite_evtchr_slots[slot].evtchr_load_state = 1;
                return 1;
            }
        }
        /* Retail has this exit and the one below as two separate returns. */
        return 0;
    }
    return 0;
}

/* Free a slot's page if it holds a unit's attack image. The EVTCHR (0xfe) and free (0xff)
 * markers carry the same 0x80 bit, so a saved EVTCHR page is freed too (QUIRKS.md). */
s32 sprite_evtchr_release_reserved_slot(s32 slot_index) {
    if ((g_sprite_evtchr_slots[slot_index].owner & SPRITE_EVTCHR_SLOT_OWNER_ATTACK_IMAGE) != 0) {
        g_sprite_evtchr_slots[slot_index].owner = SPRITE_EVTCHR_SLOT_OWNER_NONE;
        g_sprite_evtchr_slots[slot_index].evtchr_load_state = 0;
        return 1;
    }

    return 0;
}

/* Free both EVTCHR slots, pages and image buffers. */
s32 sprite_evtchr_init_slots(void) {
    s32 slot_index;

    for (slot_index = 0; slot_index < SPRITE_EVTCHR_SLOT_COUNT; slot_index++) {
        g_sprite_evtchr_slots[slot_index].owner = SPRITE_EVTCHR_SLOT_OWNER_NONE;
        g_sprite_evtchr_slots[slot_index].image_buffer_state = SPRITE_EVTCHR_SLOT_BUFFER_FREE;
        g_sprite_evtchr_slots[slot_index].evtchr_load_state = 0;
    }

    return 1;
}

/*
 * Describe both battle VRAM slots: the owning unit's spritesheet (or the
 * free/saved marker) and the EVTCHR load marker, collapsed to 0xfd..0xff.
 *
 * A struct index into g_sprite_sheet_slots keeps the table address out of the loop
 * invariants; the flat byte-array form hoists it into a saved register.
 */
s32 sprite_evtchr_get_slot_summary(sprite_evtchr_slot_summary_t* summary) {
    s32 i;
    u32 owner;
    u32 marker;
    unit_t* unit;

    for (i = 0; i < SPRITE_EVTCHR_SLOT_COUNT; i++) {
        owner = g_sprite_evtchr_slots[i].owner;
        if (owner == SPRITE_EVTCHR_SLOT_OWNER_NONE) {
            summary[i].spritesheet_id = 0xff;
        } else if (owner == SPRITE_EVTCHR_SLOT_OWNER_EVTCHR) {
            summary[i].spritesheet_id = 0xfe;
        } else if (owner & SPRITE_EVTCHR_SLOT_OWNER_ATTACK_IMAGE) {
            /* Rereads owner, as retail does; the low seven bits are the runtime id. */
            unit = unit_get_by_runtime_id(g_sprite_evtchr_slots[i].owner & 0x7f);
            summary[i].spritesheet_id = g_sprite_sheet_slots[unit->spritesheet_vram_slot].spritesheet_id;
        } else {
            summary[i].spritesheet_id = 0xfd;
        }
        marker = g_sprite_evtchr_slots[i].image_buffer_state;
        if (marker != SPRITE_EVTCHR_SLOT_BUFFER_EVTCHR) {
            if (marker == SPRITE_EVTCHR_SLOT_BUFFER_FREE) {
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
void sprite_evtchr_set_palette_by_runtime_id(s32 runtime_id, s32 slot_index, s32 clut_index) {
    battle_map_palette_load_data(
        &g_sprite_evtchr_slots[slot_index].clut[clut_index], BATTLE_MAP_CLUT_ROW_UNITS, runtime_id, 0);
    battle_map_palette_load_data(
        &g_sprite_evtchr_slots[slot_index].clut[clut_index + 8], BATTLE_MAP_CLUT_ROW_UNITS_SECOND, runtime_id, 0);
}

/*
 * Give a unit's CLUT, texture page, graphic height and v offset, and return
 * the SHP frame it is showing: the submerged one for a rider. The crystal
 * and later status sheets share the OTHER texture page.
 */
const sprite_frame_t* sprite_get_unit_frame_and_vram_ids(u32 runtime_id, sprite_unit_query_t* out) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit == 0) {
        return 0;
    }

    out->clut = unit->clut;
    if (unit->spritesheet_id >= SPRITE_SHEET_ID_CRYSTAL) {
        out->tpage = SPRITE_TPAGE_OTHER;
    } else {
        out->tpage = unit->tpage;
    }
    out->graphic_height = g_sprite_sheet_data[unit->spritesheet_id].graphic_height;
    out->graphic_y_offset = unit->special_graphic_y_offset;

    if (unit->mount_state != UNIT_MOUNT_STATE_RIDER) {
        return unit->animation.shp->frames[unit->animation.frame];
    } else {
        return unit->animation.shp->submerged_frames[unit->animation.frame];
    }
}

/* The sprite height of the unit's sheet, or 0 for an absent, dead, crystal or
 * treasure unit. */
static u32 sprite_get_unit_spritesheet_height(unit_t* unit) {
    if (unit == 0) {
        return 0;
    }
    if ((unit->status_flags_5_6 & (UNIT_STATUS_CRYSTAL | UNIT_STATUS_TREASURE)) != 0) {
        return 0;
    }
    if ((unit->status_flags_1_4 & UNIT_STATUS_DEAD) != 0) {
        return 0;
    }
    return g_sprite_sheet_data[unit->spritesheet_id].graphic_height;
}

/* The sprite height of the unit with this runtime id, 0 when it is absent, dead,
 * crystal or treasure. */
s32 sprite_get_unit_spritesheet_height_by_runtime_id(u32 runtime_id) {
    return sprite_get_unit_spritesheet_height(unit_get_by_runtime_id(runtime_id));
}

/* The sprite height of the unit with this battle id, 0 when it is absent, dead,
 * crystal or treasure. */
s32 sprite_get_unit_spritesheet_height_by_battle_id(u32 battle_id) {
    return sprite_get_unit_spritesheet_height(unit_get_by_battle_id(battle_id));
}

/* Start an ability's effect cast by runtime unit 0 on runtime unit 1 with a normal hit. No retail
 * code calls it. */
void effect_start_basic_ability_animation(s32 ability_id) {
    effect_context_t effect_data;

    effect_data.target_count = 1;
    effect_data.targets[EFFECT_CASTER_ENTRY].target_type = EFFECT_TARGET_TYPE_UNIT;
    effect_data.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id = 0;
    effect_data.targets[0].target_type = EFFECT_TARGET_TYPE_UNIT;
    effect_data.targets[0].data.unit.runtime_id = 1;
    effect_data.targets[0].data.unit.result_animation = EFFECT_HIT_NORMAL;
    effect_data.spectator_count = 0;
    effect_set_ability_animation(0, ability_id, &effect_data);
}

/* Starts an ability's effect outside an action, on a unit, on a tile or
 * on a unit that is also its caster. */
void effect_start_new(s32 ability_id, s32 target_mode, effect_start_target_data_t target) {
    effect_context_t effect_data;

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
    effect_set_ability_animation(0, ability_id, &effect_data);
}

/* effect_file_update_load as a 0 or 1 result: the event script's EffectStart
 * instruction calls it on the main stack each frame until it returns 0. */
s32 effect_file_poll_load(void) {
    /* No argument is passed: for a load state outside 0..3, effect_file_update_load
     * returns whatever this caller left in $a0. */
    return effect_file_update_load() != 0;
}

/* effect_play as a g_thread_call_target: script_play_effect_and_wait runs it on the main
 * stack. */
void effect_call_play(void) {
    effect_play();
}
