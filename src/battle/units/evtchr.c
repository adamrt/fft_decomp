/* Manage attack-image slots and query the unit frame and spritesheet dimensions. */

#include "fft/battle_map_palette.h"
#include "fft/sprite.h"
#include "fft/sprite_sheet.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/libgpu.h"
#include "psx/types.h"

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
    unit_runtime_t* unit = unit_runtime_get(runtime_id);
    s32 i;
    u8* image_data;
    RECT rect;

    if (g_sprite_evtchr_slots[slot].owner == SPRITE_EVTCHR_SLOT_OWNER_EVTCHR) {
        for (i = 0; i < SPRITE_EVTCHR_SLOT_COUNT; i++) {
            if (g_sprite_evtchr_slots[i].image_buffer_state == SPRITE_EVTCHR_SLOT_BUFFER_FREE) {
                image_data = g_sprite_evtchr_slots[i].image_data;
                sprite_attack_image_decompress(
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
    unit_runtime_t* unit;

    for (i = 0; i < SPRITE_EVTCHR_SLOT_COUNT; i++) {
        owner = g_sprite_evtchr_slots[i].owner;
        if (owner == SPRITE_EVTCHR_SLOT_OWNER_NONE) {
            summary[i].spritesheet_id = 0xff;
        } else if (owner == SPRITE_EVTCHR_SLOT_OWNER_EVTCHR) {
            summary[i].spritesheet_id = 0xfe;
        } else if (owner & SPRITE_EVTCHR_SLOT_OWNER_ATTACK_IMAGE) {
            /* Rereads owner, as retail does; the low seven bits are the runtime id. */
            unit = unit_runtime_get(g_sprite_evtchr_slots[i].owner & 0x7f);
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
const sprite_frame_t* sprite_unit_frame_vram_ids_get(u32 runtime_id, sprite_unit_query_t* out) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

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
static u32 spritesheet_height_get(unit_runtime_t* unit) {
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
s32 sprite_runtime_sheet_height_get(u32 runtime_id) {
    return spritesheet_height_get(unit_runtime_get(runtime_id));
}

/* The sprite height of the unit with this battle id, 0 when it is absent, dead,
 * crystal or treasure. */
s32 sprite_battle_sheet_height_get(u32 battle_id) {
    return spritesheet_height_get(unit_battle_get(battle_id));
}
