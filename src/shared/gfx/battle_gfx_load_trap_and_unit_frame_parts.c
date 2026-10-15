#include "fft/battle_gfx.h"

/* Expand one SHP frame into the parts of a unit's sprite display.
 *
 * Where the frame's pictures are decides the texture page: the OTHER page
 * for the crystal, chicken, frog and treasure sheets; the unit's spritesheet
 * slot for frames below the SHP's first attack frame; above that, the EVTCHR
 * slot that holds the unit's attack image (no slot, no parts). Frames from
 * BATTLE_SHP_FRAME_EVTCHR_FIRST up belong to an EVTCHR slot's own SHP and
 * the sequence id says which slot.
 *
 * A 48x48 part from a spritesheet slot ends the frame: it is stored as two
 * 48x24 halves, the upper one flagged to draw nearer the camera. The two
 * Altima sheets keep theirs whole. */
void battle_gfx_load_trap_and_unit_frame_parts(
    battle_unit_t* unit, battle_unit_anim_state_t* state, u16 frame, u16 sequence_id) {
    battle_gfx_unit_shp_frame_tables_t* frame_table;
    battle_gfx_source_frame_t* frame_data;
    battle_gfx_sprite_display_data_t* display;
    battle_gfx_source_part_t* part;
    u32 y_offset;
    u32 count;
    u16 i;
    u8 flags;
    u16 attr;
    u16 size;
    s32 u;
    u32 tile_index;
    s16 part_flags;
    u32 row;

    y_offset = unit->special_graphic_y_offset;
    /* The unpackers never store the -1 tested here, and three bits
     * of part count cannot pass 8. */
    battle_gfx_select_unit_shp_frame(unit, state, frame, sequence_id, &frame_table, &frame_data);
    if (frame_data == (battle_gfx_source_frame_t*)-1) {
        main_system_handle_error(MAIN_SYSTEM_ERROR_SHP_FRAME);
    }
    display = state->display;
    count = display->part_count = (frame_data->part_count_and_rotation & 7) + 1;
    display->rotation = g_battle_gfx_sprite_y_rotations[frame_data->part_count_and_rotation >> 3];
    if (count > 8) {
        count = 8;
        main_system_handle_error(MAIN_SYSTEM_ERROR_FRAME_PART_COUNT);
    }
    flags = frame_data->flags;
    if (unit->spritesheet_id >= BATTLE_SPRITESHEET_ID_CRYSTAL) {
        display->tpage = (flags & BATTLE_SPRITE_DISPLAY_BLEND_MASK) | BATTLE_GFX_TPAGE_OTHER;
        display->clut = unit->clut;
        for (i = 0; i < count; i++) {
            part = &frame_data->parts[i];
            attr = part->attributes;
            size = (attr & BATTLE_SHP_PART_SIZE_MASK) >> 10;
            {
                u32 tile;
                s32 part_u;
                s16 part_v;
                s32 width;
                s32 height;
                width = g_battle_gfx_part_sizes[size].width;
                height = g_battle_gfx_part_sizes[size].height;
                tile = attr & BATTLE_SHP_PART_TILE_MASK;
                part_u = (tile & 0x1f) * 8;
                part_v = y_offset + (tile >> 5) * 8;
                battle_gfx_store_sprite_display_data(display, i, part->x_shift, part->y_shift, part_u, part_v,
                    width * 8, height * 8,
                    (flags & (BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT | BATTLE_SPRITE_DISPLAY_BLEND_MASK))
                        | ((attr & BATTLE_SHP_PART_FLIP_MASK) >> 13));
            }
        }
    } else if (frame < (u32)state->shp->first_attack_frame) {
        display->tpage = unit->tpage | (flags & BATTLE_SPRITE_DISPLAY_BLEND_MASK);
        display->clut = unit->clut;
        i = 0;
        if (count != 0) {
            do {
                part = &frame_data->parts[i];
                attr = part->attributes;
                size = (attr & BATTLE_SHP_PART_SIZE_MASK) >> 10;
                if (size == BATTLE_SHP_PART_SIZE_48X48) {
                    if (unit->spritesheet_id == BATTLE_SPRITESHEET_ID_ALTIMA_SECOND_FORM
                        || unit->spritesheet_id == BATTLE_SPRITESHEET_ID_ALTIMA_FIRST_FORM) {
                        u32 tile;
                        s32 part_u;
                        s16 part_v;
                        s32 width;
                        s32 height;
                        tile = attr & BATTLE_SHP_PART_TILE_MASK;
                        part_u = (tile & 0x1f) * 8;
                        part_v = y_offset + (tile >> 5) * 8;
                        width = g_battle_gfx_part_sizes[size].width;
                        height = g_battle_gfx_part_sizes[size].height;
                        battle_gfx_store_sprite_display_data(display, i, part->x_shift, part->y_shift, part_u, part_v,
                            width * 8, height * 8,
                            (flags & (BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT | BATTLE_SPRITE_DISPLAY_BLEND_MASK))
                                | ((attr & BATTLE_SHP_PART_FLIP_MASK) >> 13));
                    } else {
                        tile_index = attr & BATTLE_SHP_PART_TILE_MASK;
                        u = (tile_index & 0x1f) * 8;
                        row = tile_index >> 5;
                        part_flags
                            = (flags & (BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT | BATTLE_SPRITE_DISPLAY_BLEND_MASK))
                            | ((attr & BATTLE_SHP_PART_FLIP_MASK) >> 13);
                        battle_gfx_store_sprite_display_data(
                            display, i, part->x_shift, part->y_shift + 0x18, u, row * 8 + 0x18, 0x30, 0x18, part_flags);
                        battle_gfx_store_sprite_display_data(display, i + 1, part->x_shift, part->y_shift, u, row * 8,
                            0x30, 0x18, part_flags | BATTLE_SPRITE_DISPLAY_NEARER);
                        display->part_count++;
                        return;
                    }
                } else {
                    u32 tile;
                    s32 part_u;
                    s16 part_v;
                    s32 width;
                    s32 height;
                    width = g_battle_gfx_part_sizes[size].width;
                    height = g_battle_gfx_part_sizes[size].height;
                    tile = attr & BATTLE_SHP_PART_TILE_MASK;
                    part_u = (tile & 0x1f) * 8;
                    part_v = y_offset + (tile >> 5) * 8;
                    battle_gfx_store_sprite_display_data(display, i, part->x_shift, part->y_shift, part_u, part_v,
                        width * 8, height * 8,
                        (flags & (BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT | BATTLE_SPRITE_DISPLAY_BLEND_MASK))
                            | ((attr & BATTLE_SHP_PART_FLIP_MASK) >> 13));
                }
            } while (++i < count);
        }
    } else if (frame < BATTLE_SHP_FRAME_EVTCHR_FIRST) {
        if ((g_battle_gfx_vram_slots[0].owner & BATTLE_GFX_VRAM_SLOT_OWNER_ID_MASK) == unit->runtime_id) {
            battle_gfx_load_unit_frame_parts(unit, display, frame_data, BATTLE_GFX_TPAGE_EVTCHR_SLOT_0);
        } else if ((g_battle_gfx_vram_slots[1].owner & BATTLE_GFX_VRAM_SLOT_OWNER_ID_MASK) == unit->runtime_id) {
            battle_gfx_load_unit_frame_parts(unit, display, frame_data, BATTLE_GFX_TPAGE_EVTCHR_SLOT_1);
        }
    } else if (sequence_id < BATTLE_SEQUENCE_EVTCHR_SLOT_1) {
        battle_gfx_load_unit_frame_parts(unit, display, frame_data, BATTLE_GFX_TPAGE_EVTCHR_SLOT_0);
    } else {
        battle_gfx_load_unit_frame_parts(unit, display, frame_data, BATTLE_GFX_TPAGE_EVTCHR_SLOT_1);
    }
}
