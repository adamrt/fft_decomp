#include "fft/battle.h"

/* Expand a unit graphic frame into the renderer's sprite-part records. */
void battle_gfx_load_unit_frame_parts(battle_unit_misc_data_t* unit, battle_gfx_sprite_display_data_t* display,
    battle_gfx_source_frame_t* src, s32 sheet_flags) {
    battle_gfx_source_part_t* part;
    u32 i;
    u32 count;
    u32 flags;
    /* attr, u, v and off take the target's registers; GCC allocates each of
     * them differently without its pin. */
    register u32 attr __asm__("$9");
    u32 tile;
    register s32 u __asm__("$8");
    register s32 v __asm__("$4");
    s32 width;
    s32 height;
    register s32 off __asm__("$2");
    s32 pad[2]; /* unreferenced; reserves the target's 0x48-byte frame */

    flags = src->flags;
    count = display->part_count;
    display->spritesheet_id = (flags & 0x60) | sheet_flags;
    i = 0;
    display->clut = unit->vram_palette_id;
    if (count != 0) {
        do {
            part = &src->parts[i];
            attr = part->attributes;
            off = (attr & 0x3C00) >> 7;
            tile = attr & 0x3FF;
            /* Emits nothing; keeps u and v derived from the masked tile, where
             * combine would fold the masks into `attr & 0x1f` and
             * `(attr >> 2) & 0xf8`. */
            __asm__("" : "=r"(tile) : "0"(tile));
            u = (tile & 0x1F) * 8;
            /* Emits nothing; keeps u's shift ahead of v's computation. */
            __asm__("" : : "r"(u));
            v = (tile >> 5) * 8;
            /* Typed s32-array indexing breaks the byte-exact load schedule;
             * the byte displacement preserves the exact target instructions. */
            width = *(s32*)((u8*)g_battle_gfx_part_sizes + off);
            height = *(s32*)((u8*)&g_battle_gfx_part_sizes[0].height + off);
            off = flags & 0x61;
            off |= (attr & 0xC000) >> 13;
            /* Emits nothing; without it the callee-saved registers shift up
             * by one (s1..s4 become s2..s5). */
            __asm__("" : "=r"(i) : "0"(i));
            /* This caller passes signed coordinates and full-width arguments. */
            ((void (*)(battle_gfx_sprite_display_data_t*, s32, s32, s32, s32, s32, s32, s32,
                s32))battle_gfx_store_sprite_display_data)(
                display, i, part->x_shift, part->y_shift, u, v, (s16)(width * 8), (s16)(height * 8), off);
            i++;
        } while (i < count);
    }
}
