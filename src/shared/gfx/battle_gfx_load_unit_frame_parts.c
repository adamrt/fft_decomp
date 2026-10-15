#include "fft/battle_gfx.h"

/* Expand a frame into the display's parts, all drawn from texture page
 * `tpage`: that of the EVTCHR slot holding the frame's pictures. */
void battle_gfx_load_unit_frame_parts(
    battle_unit_t* unit, battle_gfx_sprite_display_data_t* display, battle_gfx_source_frame_t* src, s32 tpage) {
    battle_gfx_source_part_t* part;
    u32 i;
    u32 count;
    u8 flags;
    u16 attr;
    u16 size;
    u32 tile;
    s32 u;
    s32 v;
    s32 width;
    s32 height;

    flags = src->flags;
    count = display->part_count;
    display->tpage = (flags & BATTLE_SPRITE_DISPLAY_BLEND_MASK) | tpage;
    display->clut = unit->clut;
    for (i = 0; i < count; i++) {
        part = &src->parts[i];
        attr = part->attributes;
        size = (attr & BATTLE_SHP_PART_SIZE_MASK) >> 10;
        width = g_battle_gfx_part_sizes[size].width;
        height = g_battle_gfx_part_sizes[size].height;
        tile = attr & BATTLE_SHP_PART_TILE_MASK;
        u = (tile & 0x1f) * 8;
        v = (tile >> 5) * 8;
        battle_gfx_store_sprite_display_data(display, i, part->x_shift, part->y_shift, u, v, width * 8, height * 8,
            (flags & (BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT | BATTLE_SPRITE_DISPLAY_BLEND_MASK))
                | ((attr & BATTLE_SHP_PART_FLIP_MASK) >> 13));
    }
}
