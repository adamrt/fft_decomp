#include "psx/gpu.h"
#include "psx/types.h"

/*
 * Set the texture window, texture page and CLUT of a 32x48 portrait quad.
 *
 * The low byte of `code` is the portrait index: 40 portraits per texture
 * page, laid out 8 across and 5 down, with four 16-colour CLUTs per CLUT row.
 * Indices 0x3F-0x52 instead use a 6-row CLUT block at y 0x1FA whose column
 * group comes from bits 10-11 of `code`.
 *
 * `code` is reused for a copy of the cell number, as the target keeps that
 * copy in the incoming argument register. The empty asm gives `clut_x` a
 * second set without emitting code; with a single set, the pre-allocation
 * scheduler hoists the GetTPage argument moves above the `% 4`, and the
 * argument register conflicts with `code`.
 */
void world_gfx_set_portrait_poly_texture(POLY_FT4* poly, s32 code) {
    s32 index;
    s32 palette;
    s32 page;
    s32 cell;
    s32 u;
    s32 v;
    s32 clut_x;
    s32 clut_y;
    s32 page_x;
    s32 y;

    index = code & 0xFF;
    palette = (code & 0xC00) >> 10;
    page = index / 40;
    cell = index % 40;
    code = cell;
    u = (cell % 8) << 5;
    v = (cell / 8) * 48;
    clut_y = cell / 4;
    clut_x = (code % 4) << 4;
    /* Second set of clut_x (see above). */
    __asm__("" : "=r"(clut_x) : "0"(clut_x));
    page_x = page << 6;
    poly->u0 = u;
    poly->v0 = v;
    poly->u1 = u + 31;
    poly->v1 = v;
    poly->u2 = u;
    poly->v2 = v + 48;
    poly->u3 = u + 31;
    poly->v3 = v + 48;
    poly->tpage = GetTPage(0, 0, page_x + 0x680, 0x100);
    if ((u32)(index - 0x3F) < 20) {
        s32 x;

        index -= 0x3F;
        x = (palette << 4) + 0x680;
        y = index % 6;
        poly->clut = GetClut(((index / 6) << 6) + x, y + 0x1FA);
    } else {
        s32 x;

        x = clut_x + 0x680;
        poly->clut = GetClut(page_x + x, clut_y + 0x1F0);
    }
}
