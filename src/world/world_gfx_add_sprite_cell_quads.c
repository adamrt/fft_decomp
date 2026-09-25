#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Takes one POLY_FT4 from the pool per sprite cell, fills it as a w x h quad
 * at (x + 0x80, y) with texture origin (u, v), and links it into ordering
 * table entry ot_index. With rgb the quads are shaded with that colour. */
void world_gfx_add_sprite_cell_quads(
    battle_menu_sprite_cell_t* cell, u8* rgb, s32 semi_trans, u16 tpage, u16 clut, s32 ot_index, s32 count) {
    POLY_FT4* poly;

    count--;
    while (count >= 0) {
        poly = &g_world_gfx_active_packet_buffer->textured_quads[g_world_gfx_textured_quad_count++];
        if (rgb != 0) {
            SetShadeTex(poly, 0);
            poly->r0 = rgb[0];
            poly->g0 = rgb[1];
            poly->b0 = rgb[2];
        } else {
            SetShadeTex(poly, 1);
        }
        SetSemiTrans(poly, semi_trans);
        poly->x0 = cell->x + 0x80;
        poly->y0 = cell->y;
        poly->x1 = cell->w + (cell->x + 0x80);
        poly->y1 = cell->y;
        poly->x2 = cell->x + 0x80;
        poly->y2 = cell->y + cell->h;
        poly->x3 = cell->w + (cell->x + 0x80);
        poly->y3 = cell->y + cell->h;
        poly->u0 = cell->u;
        poly->v0 = cell->v;
        poly->u1 = cell->u + cell->w;
        poly->v1 = cell->v;
        poly->u2 = cell->u;
        poly->v2 = cell->v + cell->h;
        poly->u3 = cell->u + cell->w;
        poly->v3 = cell->v + cell->h;
        poly->clut = clut;
        poly->tpage = tpage;
        count--;
        cell++;
        setaddr(poly, getaddr(&g_world_gfx_active_packet_buffer->otag[ot_index]));
        setaddr(&g_world_gfx_active_packet_buffer->otag[ot_index], poly);
    }
}
