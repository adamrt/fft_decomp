#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Takes one POLY_FT4 from the pool, fills it as the rect->w x rect->h quad at
 * (rect->x + 0x80, rect->y) with texture origin (u, v), and links it into
 * ordering table entry priority. With color the quad is shaded with it.
 *
 * Old-style definition: the target reads texture_page and clut as halfwords
 * from their argument slots, while the int prototype in fft/world.h keeps the
 * callers from masking them. */
/* clang-format off */
void world_gfx_enqueue_textured_quad(rect, u, v, color, semi_trans, texture_page, clut, priority)
    RECT* rect;
    s32 u;
    s32 v;
    void* color;
    s32 semi_trans;
    u16 texture_page;
    u16 clut;
    s32 priority;
/* clang-format on */
{
    POLY_FT4* poly;
    CVECTOR* rgb = color;

    poly = &g_world_gfx_active_packet_buffer->textured_quads[g_world_gfx_textured_quad_count++];
    if (rgb != 0) {
        SetShadeTex(poly, 0);
        poly->r0 = rgb->r;
        poly->g0 = rgb->g;
        poly->b0 = rgb->b;
    } else {
        SetShadeTex(poly, 1);
    }
    SetSemiTrans(poly, semi_trans);
    poly->x0 = rect->x + 0x80;
    poly->y0 = rect->y;
    poly->x1 = rect->x + 0x80 + rect->w;
    poly->y1 = rect->y;
    poly->x2 = rect->x + 0x80;
    poly->y2 = rect->y + rect->h;
    poly->x3 = rect->x + 0x80 + rect->w;
    poly->y3 = rect->y + rect->h;
    poly->u0 = u;
    poly->v0 = v;
    poly->u1 = u + rect->w;
    poly->v1 = v;
    poly->u2 = u;
    poly->v2 = v + rect->h;
    poly->u3 = u + rect->w;
    poly->v3 = v + rect->h;
    poly->clut = clut;
    poly->tpage = texture_page;
    setaddr(poly, getaddr(&g_world_gfx_active_packet_buffer->otag[priority]));
    setaddr(&g_world_gfx_active_packet_buffer->otag[priority], poly);
}
