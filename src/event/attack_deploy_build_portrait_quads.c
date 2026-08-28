#include "fft/attack.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Build the two POLY_FT4 quads for one deployment roster portrait.
 *
 * The first quad is a 48x48 cell of the portrait sheet (five cells per row,
 * CLUT selected by index % 4 and index / 4) placed over the isometric tile at
 * (column, row), using the same screen projection as the cursor builder at
 * 0x801c7720; a nonzero g_attack_formation_sprite_tall_cell[index] draws it four pixels lower. The
 * second quad squeezes the 20x20 texel cell at (0x90, 0x40) into a 20x10
 * marker under it. Each quad is written in libgpu setXYWH/setUVWH field order,
 * which the target's store order and constant offsets follow. */
void attack_deploy_build_portrait_quads(s32 column, s32 row, s32 index, POLY_FT4* poly) {
    s32 u;
    s32 v;
    s32 x;
    s32 y;
    s32 marker_y;
    s32 x0;
    s32 y0;

    u = (index % 5) * 0x30;
    v = (index / 5) * 0x30;
    poly->u0 = u;
    poly->v0 = v;
    poly->u1 = u + 0x30;
    poly->v1 = v;
    poly->u2 = u;
    poly->v2 = v + 0x30;
    poly->u3 = u + 0x30;
    poly->v3 = v + 0x30;
    x = row * 20 + 0x100 - column * 20;
    y = 0xc4 - row * 10 - column * 10;
    marker_y = y + 2;
    poly->clut = GetClut((index % 4) * 0x10 + 0x140, index / 4 + 0xe0);
    if (g_attack_formation_sprite_tall_cell[index] != 0) {
        x0 = x - 0x17;
        y0 = y - 0x21;
        poly->x0 = x0;
        poly->y0 = y0;
        poly->x1 = x0 + 0x30;
        poly->y1 = y0;
        poly->x2 = x0;
        poly->y2 = y0 + 0x30;
        poly->x3 = x0 + 0x30;
        poly->y3 = y0 + 0x30;
    } else {
        x0 = x - 0x17;
        y0 = y - 0x25;
        poly->x0 = x0;
        poly->y0 = y0;
        poly->x1 = x0 + 0x30;
        poly->y1 = y0;
        poly->x2 = x0;
        poly->y2 = y0 + 0x30;
        poly->x3 = x0 + 0x30;
        poly->y3 = y0 + 0x30;
    }
    poly++;
    poly->x0 = x - 10;
    poly->y0 = marker_y - 1;
    poly->x1 = x + 10;
    poly->y1 = marker_y - 1;
    poly->x2 = x - 10;
    poly->y2 = marker_y + 9;
    poly->x3 = x + 10;
    poly->y3 = marker_y + 9;
    poly->u0 = 0x90;
    poly->v0 = 0x40;
    poly->u1 = 0xa4;
    poly->v1 = 0x40;
    poly->u2 = 0x90;
    poly->v2 = 0x54;
    poly->u3 = 0xa4;
    poly->v3 = 0x54;
    poly->clut = GetClut(0x140, 0xf8);
}
