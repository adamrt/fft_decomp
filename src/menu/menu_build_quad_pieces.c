#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

s32 menu_build_quad_pieces(s32 group, POLY_GT4* poly) {
    menu_quad_piece_t* piece;
    s32 count;
    s32 i;
    menu_quad_colors_t* colors;

    colors = &g_menu_quad_colors;
    piece = &g_menu_quad_pieces[g_menu_quad_group_starts[group]];
    count = g_menu_quad_group_starts[group + 1] - g_menu_quad_group_starts[group];
    for (i = 0; i < count; i++) {
        setXY4(poly, g_menu_quad_origin.x + piece->x, g_menu_quad_origin.y + piece->y,
            g_menu_quad_origin.x + piece->x + piece->w, g_menu_quad_origin.y + piece->y,
            g_menu_quad_origin.x + piece->x, g_menu_quad_origin.y + piece->y + (piece->h - 1),
            g_menu_quad_origin.x + piece->x + piece->w, g_menu_quad_origin.y + piece->y + (piece->h - 1));
        setUV4(poly, piece->u, piece->v - 0x78, piece->u + piece->w, piece->v - 0x78, piece->u,
            piece->v - 0x78 + (piece->h - 1), piece->u + piece->w, piece->v - 0x78 + (piece->h - 1));
        setRGB0(poly, colors->vertex[0].r, colors->vertex[0].g, colors->vertex[0].b);
        setRGB1(poly, colors->vertex[1].r, colors->vertex[1].g, colors->vertex[1].b);
        setRGB2(poly, colors->vertex[2].r, colors->vertex[2].g, colors->vertex[2].b);
        setRGB3(poly, colors->vertex[3].r, colors->vertex[3].g, colors->vertex[3].b);
        poly++;
        piece++;
    }
    return count;
}
