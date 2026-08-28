#include "psx/gpu.h"
#include "psx/types.h"

void require_gfx_scale_poly_gt4_vertex_colors(const u8* vertex_colors, s32 scale, POLY_GT4* poly) {
    s32 r;
    s32 g;
    s32 b;

    r = vertex_colors[0x04] * scale / 128;
    g = vertex_colors[0x05] * scale / 128;
    b = vertex_colors[0x06] * scale / 128;
    poly->r0 = r;
    poly->g0 = g;
    poly->b0 = b;
    r = vertex_colors[0x08] * scale / 128;
    g = vertex_colors[0x09] * scale / 128;
    b = vertex_colors[0x0a] * scale / 128;
    poly->r1 = r;
    poly->g1 = g;
    poly->b1 = b;
    r = vertex_colors[0x0c] * scale / 128;
    g = vertex_colors[0x0d] * scale / 128;
    b = vertex_colors[0x0e] * scale / 128;
    poly->r2 = r;
    poly->g2 = g;
    poly->b2 = b;
    r = vertex_colors[0x10] * scale / 128;
    g = vertex_colors[0x11] * scale / 128;
    b = vertex_colors[0x12] * scale / 128;
    poly->r3 = r;
    poly->g3 = g;
    poly->b3 = b;
}
