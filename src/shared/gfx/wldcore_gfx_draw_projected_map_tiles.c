#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Needs aspsx 2.21: four sites that reach the typed global array keep their
 * `addiu at,at,<offset>` under 2.21 and lose it under 2.34.
 *
 * Walks g_wldcore_map_projected_tiles, the table that
 * wldcore_map_project_and_cull_tiles fills: each tile reads its own entry's
 * flags, texture and .screen_x/.screen_y, and the .screen_x of the next
 * entry, so the accesses are written against `index` and `index + 1`.
 *
 * The 32-bit texture value packs u, v, width and height from the top byte down; a
 * width or height that would run past texel 255 is shortened by one. */

#define WLDCORE_MAP_TILE_ROWS    12
#define WLDCORE_MAP_TILE_COLUMNS 16
#define WLDCORE_MAP_ROW_STRIDE   17
#define WLDCORE_MAP_DOT_CLUT_Y   0x1e0

/* Sorts the world map's projected tile grid into `ot` as textured quads.
 * Each tile spans from its own projected corner to the next column's x and
 * the next row's y; a tile flagged off screen or empty is skipped. All tiles
 * share the map CLUT and the current screen tint. */
void wldcore_gfx_draw_projected_map_tiles(GsOT* ot) {
    POLY_FT4 poly;
    CVECTOR color;
    s32 row;
    s32 column;
    s32 index;

    SetPolyFT4(&poly);
    color.r = 0x80;
    color.g = 0x80;
    color.b = 0x80;
    wldcore_gfx_copy_color_with_tint(&color, &color);
    setRGB0(&poly, color.r, color.g, color.b);
    poly.clut = GetClut(0, WLDCORE_MAP_DOT_CLUT_Y);

    for (row = 0; row < WLDCORE_MAP_TILE_ROWS; row++) {
        index = row * WLDCORE_MAP_ROW_STRIDE;

        poly.y0 = poly.y1 = g_wldcore_map_projected_tiles[index].screen_y;
        poly.y2 = poly.y3 = g_wldcore_map_projected_tiles[index + WLDCORE_MAP_ROW_STRIDE].screen_y;

        for (column = 0; column < WLDCORE_MAP_TILE_COLUMNS; column++, index++) {
            s32 flags = g_wldcore_map_projected_tiles[index].flags;
            u32 texture;
            s32 u;
            s32 v;
            s32 w;
            s32 h;

            if (flags & (WLDCORE_MAP_TILE_OFF_SCREEN | WLDCORE_MAP_TILE_EMPTY)) {
                continue;
            }
            poly.tpage = flags & WLDCORE_MAP_TILE_TPAGE_MASK;
            poly.x0 = poly.x2 = g_wldcore_map_projected_tiles[index].screen_x;
            poly.x1 = poly.x3 = g_wldcore_map_projected_tiles[index + 1].screen_x;

            texture = g_wldcore_map_projected_tiles[index].texture;
            u = (texture & 0xff000000) >> 24;
            v = (texture & 0x00ff0000) >> 16;
            w = (texture & 0x0000ff00) >> 8;
            if (u + w >= 0x100) {
                w--;
            }
            h = texture & 0x000000ff;
            if (v + h >= 0x100) {
                h--;
            }
            poly.u0 = poly.u2 = u;
            poly.v0 = poly.v1 = v;
            poly.u1 = poly.u3 = u + w;
            poly.v2 = poly.v3 = v + h;
            GsSortPoly(&poly, ot, 2);
        }
    }
}
