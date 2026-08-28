#include "fft/debugchr.h"
#include "psx/gpu.h"
#include "psx/types.h"

void debugchr_gfx_set_scaled_poly_ft4_geometry(POLY_FT4* poly, const debug_chr_texture_page_t* texture,
    const s16* position, const s16* uv_rect, const s16* scale, const s16* offset) {
    s32 xoff = uv_rect[4] * scale[0] / ONE;
    s32 yoff = uv_rect[5] * scale[1] / ONE;
    s32 width = uv_rect[2] * scale[0] / ONE;
    s32 height = uv_rect[3] * scale[1] / ONE;

    if (width < 0)
        width = -width;
    if (height < 0)
        height = -height;
    poly->tpage = GetTPage(0, 0, texture->x, texture->y & 0xf00);
    poly->u0 = (u8)uv_rect[0];
    poly->v0 = (u8)uv_rect[1];
    poly->u1 = (u8)uv_rect[0] + (u8)uv_rect[2];
    poly->v1 = (u8)uv_rect[1];
    poly->u2 = (u8)uv_rect[0];
    poly->v2 = (u8)uv_rect[1] + (u8)uv_rect[3];
    poly->u3 = (u8)uv_rect[0] + (u8)uv_rect[2];
    poly->v3 = (u8)uv_rect[1] + (u8)uv_rect[3];
    poly->x0 = position[0] + xoff + offset[4];
    poly->y0 = position[1] + yoff + offset[5];
    poly->x1 = position[0] + xoff + offset[4] + width;
    poly->y1 = position[1] + yoff + offset[5];
    poly->x2 = position[0] + xoff + offset[4];
    poly->y2 = position[1] + yoff + offset[5] + height;
    poly->x3 = position[0] + xoff + offset[4] + width;
    poly->y3 = position[1] + yoff + offset[5] + height;
}
