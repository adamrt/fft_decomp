#include "fft/main_gfx.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

void world_gfx_enqueue_oriented_textured_quad(
    const world_oriented_quad_t* descriptor, const u8* color, s32 orientation, s32 semitrans, s32 otag_index) {
    s32 quad_semitrans = semitrans;
    s32 quad_orientation = orientation;
    POLY_FT4* poly;
    const CVECTOR* rgb = (const CVECTOR*)color;
    u16 index = g_world_gfx_textured_quad_count;

    g_world_gfx_textured_quad_count = index + 1;
    poly = &g_world_gfx_active_packet_buffer->textured_quads[index];

    if (rgb != 0) {
        SetShadeTex(poly, 0);
        poly->r0 = rgb->r;
        poly->g0 = rgb->g;
        poly->b0 = rgb->b;
    } else {
        SetShadeTex(poly, 1);
    }
    SetSemiTrans(poly, quad_semitrans);

    poly->x0 = descriptor->x + 0x80;
    poly->y0 = descriptor->y;
    {
        s32 right_offset = descriptor->w + 0x80;
        s32 right_x = descriptor->x;

        right_x += right_offset;
        poly->x1 = right_x;
    }
    poly->y1 = descriptor->y;
    poly->x2 = descriptor->x + 0x80;
    poly->y2 = descriptor->y + descriptor->h;
    {
        s32 right_offset = descriptor->w + 0x80;
        s32 right_x = descriptor->x;

        right_x += right_offset;
        poly->x3 = right_x;
    }
    poly->y3 = descriptor->y + descriptor->h;

    {
        s32 orientation_index = quad_orientation & 0xff;

        switch (orientation_index) {
        case TEXTURE_UV_FLIP_NONE:
            poly->u0 = descriptor->u;
            poly->v0 = descriptor->v;
            poly->u1 = descriptor->u + descriptor->uw;
            poly->v1 = descriptor->v;
            poly->u2 = descriptor->u;
            poly->v2 = descriptor->v + descriptor->vh;
            poly->u3 = descriptor->u + descriptor->uw;
            poly->v3 = descriptor->v + descriptor->vh;
            break;
        case TEXTURE_UV_FLIP_X:
            poly->u1 = descriptor->u;
            poly->v1 = descriptor->v;
            poly->u0 = descriptor->u + descriptor->uw;
            poly->v0 = descriptor->v;
            poly->u3 = descriptor->u;
            poly->v3 = descriptor->v + descriptor->vh;
            poly->u2 = descriptor->u + descriptor->uw;
            poly->v2 = descriptor->v + descriptor->vh;
            break;
        case TEXTURE_UV_FLIP_Y:
            poly->u2 = descriptor->u;
            poly->v2 = descriptor->v;
            poly->u3 = descriptor->u + descriptor->uw;
            poly->v3 = descriptor->v;
            poly->u0 = descriptor->u;
            poly->v0 = descriptor->v + descriptor->vh;
            poly->u1 = descriptor->u + descriptor->uw;
            poly->v1 = descriptor->v + descriptor->vh;
            break;
        case TEXTURE_UV_FLIP_XY:
            poly->u3 = descriptor->u;
            poly->v3 = descriptor->v;
            poly->u2 = descriptor->u + descriptor->uw;
            poly->v2 = descriptor->v;
            poly->u1 = descriptor->u;
            poly->v1 = descriptor->v + descriptor->vh;
            poly->u0 = descriptor->u + descriptor->uw;
            poly->v0 = descriptor->v + descriptor->vh;
            break;
        }
    }

    poly->clut = descriptor->clut;
    poly->tpage = descriptor->tpage;
    setaddr(poly, getaddr(&g_world_gfx_active_packet_buffer->otag[otag_index]));
    setaddr(&g_world_gfx_active_packet_buffer->otag[otag_index], poly);
}
