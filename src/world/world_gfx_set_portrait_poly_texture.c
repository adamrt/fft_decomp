#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * Set the texture window, texture page and CLUT of a 32x48 portrait quad.
 *
 * The low byte of `code` is the portrait index: 40 portraits per texture
 * page, laid out 8 across and 5 down, with four 16-color CLUTs per CLUT row.
 * Indices 0x3F-0x52 instead use a 6-row CLUT block at y 0x1FA whose column
 * group comes from bits 10-11 of `code`.
 *
 * `code` is reused for the cell number that selects the CLUT.
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

    index = code & 0xFF;
    palette = (code & 0xC00) >> 10;
    page = index / 40;
    cell = index % 40;
    code = cell;
    u = (cell % 8) << 5;
    v = (cell / 8) * 48;
    clut_x = (code % 4) << 4;
    clut_y = code / 4;
    page_x = page << 6;
    setUV4(poly, u, v, u + 31, v, u, v + 48, u + 31, v + 48);
    poly->tpage = GetTPage(0, 0, page_x + 0x680, 0x100);
    if (index >= 0x3F && index <= 0x52) {
        s32 x;

        index -= 0x3F;
        x = (palette << 4) + 0x680;
        clut_y = index % 6;
        clut_x = ((index / 6) << 6) + x;
        poly->clut = GetClut(clut_x, clut_y + 0x1FA);
    } else {
        s32 x;

        x = clut_x + 0x680;
        poly->clut = GetClut(page_x + x, clut_y + 0x1F0);
    }
}
