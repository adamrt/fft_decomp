#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Hides a unit: clears draw_enabled and its sprite graphic trigger. Returns 0
 * when no unit has the runtime id. */
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
    battle_unit_t* unit;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xFFFF);
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
        for (i = 0; i < 0x40; i++) {
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
        for (i = 0; i < 0x28; i++) {
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
    battle_unit_t* unit;
    s32 i;
    u8* image_data;
    RECT rect;

    unit = battle_unit_get_by_runtime_id(runtime_id);
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
    map_load_palette_data(&g_battle_gfx_vram_slots[slot_index].clut[clut_index], MAP_CLUT_ROW_UNITS, runtime_id, 0);
    map_load_palette_data(
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
u32 battle_gfx_get_unit_spritesheet_height(battle_unit_t* unit) {
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
    return battle_gfx_get_unit_spritesheet_height(battle_unit_get_by_battle_id(unit_id & 0xffff));
}
