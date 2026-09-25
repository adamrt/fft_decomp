#include "fft/wldcore.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Needs aspsx 2.21: four sites that reach the typed global array keep their
 * `addiu at,at,<offset>` under 2.21 and lose it under 2.34.
 *
 * Expression form and source order carry most of the schedule: the target
 * masks then shifts, so the mask-first spelling is written directly, and one
 * masked value is defined below its neighbour so it can reach a branch delay
 * slot. The $2/$3/$6 pins close a register rotation among the UV values, and
 * the $4/$5 pins steer the inner call's arguments into their own registers.
 * Two empty asm operands are each needed on their own: the `texture`
 * launder keeps the loaded word whole, and the `poly_a0` launder anchors the
 * inner call's first argument setup at its statement, where the target has it
 * (the $4 pin alone does not).
 *
 * Walks g_wldcore_map_projected_tiles, the table that
 * wldcore_map_project_and_cull_tiles fills: each tile reads its own entry's
 * flags, texture and .screen_x/.screen_y, and the .screen_x of the next
 * entry, so the accesses are written against `index` and `index + 1`. */

#define WLDCORE_MAP_TILE_ROWS    12
#define WLDCORE_MAP_TILE_COLUMNS 16
#define WLDCORE_MAP_ROW_STRIDE   17
#define WLDCORE_TILE_CULLED_MASK 0x300
#define WLDCORE_MAP_DOT_CLUT_Y   0x1e0

void wldcore_gfx_draw_projected_map_tiles(s32 ot) {
    POLY_FT4 poly;
    CVECTOR color;
    s32 ot_s3 = ot;
    s32 row_s2;
    s32 column_s1;
    register s32 store_tmp __asm__("$2");
    s32 index;

    SetPolyFT4(&poly);
    color.r = 0x80;
    color.g = 0x80;
    color.b = 0x80;
    wldcore_gfx_copy_color_with_tint(&color, &color);
    poly.r0 = color.r;
    poly.g0 = color.g;
    poly.b0 = color.b;
    poly.clut = GetClut(0, WLDCORE_MAP_DOT_CLUT_Y);

    for (row_s2 = 0; row_s2 < WLDCORE_MAP_TILE_ROWS; row_s2++) {
        index = row_s2 * WLDCORE_MAP_ROW_STRIDE;

        poly.y1 = g_wldcore_map_projected_tiles[index].screen_y;
        poly.y0 = poly.y1;
        poly.y3 = g_wldcore_map_projected_tiles[index + WLDCORE_MAP_ROW_STRIDE].screen_y;
        poly.y2 = poly.y3;

        for (column_s1 = 0; column_s1 < WLDCORE_MAP_TILE_COLUMNS; column_s1++, index++) {
            s32 flags = g_wldcore_map_projected_tiles[index].flags;
            u32 texture;
            s32 u0;
            s32 u1;
            s32 v0;
            s32 v1;
            register s32 edge_u __asm__("$3");
            register s32 edge_v __asm__("$6");
            register s32 ot_a1 __asm__("$5");
            register POLY_FT4* poly_a0 __asm__("$4");

            if (flags & WLDCORE_TILE_CULLED_MASK) {
                continue;
            }
            poly.tpage = flags & 0xff;
            poly.x2 = g_wldcore_map_projected_tiles[index].screen_x;
            poly.x0 = poly.x2;
            poly.x3 = g_wldcore_map_projected_tiles[index + 1].screen_x;
            poly.x1 = poly.x3;

            texture = g_wldcore_map_projected_tiles[index].texture;
            /* Emits nothing; the target masks the loaded word, and without
             * this the byte fields narrow into separate lbu loads. */
            __asm__("" : "=r"(texture) : "0"(texture));
            u1 = (texture & 0xff00) >> 8;
            v1 = texture >> 24;
            v0 = (texture >> 16) & 0xff;
            if (v1 + u1 >= 0x100) {
                u1--;
            }
            edge_u = v1 + u1;
            u0 = texture & 0xff;
            if (v0 + u0 >= 0x100) {
                u0--;
            }
            edge_v = v0 + u0;
            poly_a0 = &poly;
            /* Emits nothing; keeps `addiu a0,sp,0x10` here, ahead of the UV
             * stores, instead of next to the call. */
            __asm__("" : "=r"(poly_a0) : "0"(poly_a0));
            ot_a1 = ot_s3;
            store_tmp = v1;
            poly.u2 = store_tmp;
            poly.u0 = store_tmp;
            poly.v3 = edge_v;
            poly.v2 = edge_v;
            store_tmp = v0;
            poly.v1 = store_tmp;
            poly.v0 = store_tmp;
            poly.u3 = edge_u;
            poly.u1 = edge_u;
            world_gs_sortpoly(poly_a0, ot_a1, 2, v1);
        }
    }
}
