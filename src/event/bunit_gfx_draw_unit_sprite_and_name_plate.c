#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/bunit.h"
#include "psx/types.h"

/* Stack view of bunit_oriented_quad_t whose clut/tpage slots receive the
 * sprite query's palette and spritesheet IDs directly; the query's last two
 * halfwords extend it to 0x18 bytes. */
typedef struct {
    s16 x;
    s16 y;
    s16 w;
    s16 h;
    s16 u;
    s16 v;
    s16 uw;
    s16 vh;
    battle_unit_sprite_query_t query; /* 0x10 */
} bunit_sprite_quad_t;

/* One SHP frame cell: signed pixel offset, then a 10-bit 8x8 cell index (32
 * cells per row) and a 4-bit size class indexing g_bunit_sprite_cell_sizes. */
typedef struct {
    s8 dx;
    s8 dy;
    u16 cell : 10;
    u16 size : 4;
    u16 unk_2_14 : 2;
} bunit_sprite_cell_t;

/* Draw a unit's current SHP frame at (x, y), then its two name-plate quads.
 *
 * Brightness falls off with distance from the animated focal point; the
 * selected unit's plate also pulses. The target keeps two short-lived values
 * in the loop counter and OT-index registers (the falloff result in `i`, the
 * facing quadrant in `value`), so the source reuses those variables. */
void bunit_gfx_draw_unit_sprite_and_name_plate(s16 unit, s32 x, s32 y) {
    u8 rgb[3];
    bunit_sprite_quad_t quad;
    u8* frame;
    s32 base_y;
    s32 i;
    bunit_sprite_cell_t* cells;
    s32 dx;
    s32 orientation;
    s32 value;
    s32 pulse;
    s32 size;

    i = bunit_gfx_calculate_distance_falloff(x, y, 0xC8, 0x50);
    rgb[0] = rgb[1] = rgb[2] = i > 0x80 ? 0x80 : i;
    frame = battle_gfx_get_unit_sprite_frame_and_vram_ids(g_bunit_unit_data[unit]->misc_unit_id, &quad.query);
    base_y = (quad.query.graphic_height >> 1) + 0x16;
    if (frame == (u8*)-1) {
        return;
    }
    cells = (bunit_sprite_cell_t*)(frame + 2);
    for (i = 0; i <= frame[0]; i++) {
        size = cells[i].size;
        quad.uw = quad.w = g_bunit_sprite_cell_sizes[size][0];
        quad.vh = quad.h = g_bunit_sprite_cell_sizes[size][1];
        value = battle_unit_get_camera_facing_quadrant_by_battle_id(g_bunit_unit_data[unit]->roster_id);
        dx = cells[i].dx;
        orientation = 0;
        if (value >= 2) {
            orientation = 1;
            dx = -dx - quad.w;
        }
        /* The (s16) keeps the 0x1C offset on dx instead of letting fold
         * reassociate it onto x. */
        quad.x = x + (s16)(dx + 0x1C);
        quad.y = cells[i].dy + (y + base_y);
        if (g_bunit_unit_data[unit]->formation_order_key & 0x80) {
            quad.y -= 8;
        }
        quad.u = cells[i].cell % 32 * 8;
        quad.v = cells[i].cell / 32 * 8 + quad.query.graphic_y_offset;
        value = 7;
        if (g_bunit_unit_data[unit]->formation_order_key & 0x40) {
            value = 8;
            if (i >= 3) {
                value = 6;
            }
        }
        bunit_gfx_enqueue_oriented_textured_quad((bunit_oriented_quad_t*)&quad, rgb, orientation, 0, value);
    }
    if (g_bunit_unit_data[unit]->formation_order_key & 0x40) {
        return;
    }
    g_bunit_unit_shadow_sprite.x = x + 0x12;
    g_bunit_unit_shadow_sprite.y = y + base_y - 4;
    g_bunit_unit_shadow_sprite.tpage = GetTPage(0, 2, 0x3C0, 0x100);
    bunit_gfx_enqueue_oriented_textured_quad(&g_bunit_unit_shadow_sprite, 0, 0, 1, 5);
    g_bunit_unit_dot_sprite.x = x;
    g_bunit_unit_dot_sprite.y = y;
    if (g_bunit_unit_data[unit]->team_kind == 1) {
        g_bunit_unit_dot_sprite.clut = 0x3F12;
    } else {
        g_bunit_unit_dot_sprite.clut = 0x3F13;
    }
    g_bunit_unit_dot_sprite.tpage = GetTPage(0, 1, 0x3C0, 0x100);
    if (unit == g_bunit_unit_selected_index) {
        pulse = g_bunit_unit_dot_glow_timer + g_bunit_unit_dot_glow_direction * 2;
        g_bunit_unit_dot_glow_timer = pulse;
        if ((s8)pulse > 0x28) {
            g_bunit_unit_dot_glow_direction = -1;
        }
        if ((s8)pulse < -0x28) {
            g_bunit_unit_dot_glow_direction = 1;
        }
        rgb[0] += pulse;
        rgb[1] += pulse;
        rgb[2] += pulse;
    }
    bunit_gfx_enqueue_oriented_textured_quad(&g_bunit_unit_dot_sprite, rgb, 0, 1, 8);
}
