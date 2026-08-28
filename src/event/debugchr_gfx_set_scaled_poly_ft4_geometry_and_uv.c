#include "fft/debugchr.h"
#include "fft/main_zodiac.h"
#include "psx/gpu.h"
#include "psx/types.h"

void debugchr_gfx_set_scaled_poly_ft4_geometry_and_uv(POLY_FT4* poly, const debug_chr_texture_page_t* texture,
    const RECT* position, const main_zodiac_sprite_frame_t* uv_rect, const point16_t* scale, const s16* offset) {
    s32 xoff = uv_rect->offset_x * scale->x;
    s32 yoff = uv_rect->offset_y * scale->y;
    s32 width = uv_rect->width * scale->x;
    s32 height = uv_rect->height * scale->y;
    s32 round_xoff = 0;
    s32 round_yoff = 0;
    s32 round_width = 0;
    s32 round_height = 0;
    s32 xoff_quotient;
    s32 yoff_quotient;
    s32 width_quotient;
    s32 height_quotient;

    xoff_quotient = xoff / ONE;
    if (xoff - xoff_quotient * ONE >= 2048)
        round_xoff = 1;
    yoff_quotient = yoff / ONE;
    if (yoff - yoff_quotient * ONE >= 2048)
        round_yoff = 1;
    width_quotient = width / ONE;
    if (width - width_quotient * ONE >= 2048)
        round_width = 1;
    height_quotient = height / ONE;
    if (height - height_quotient * ONE >= 2048)
        round_height = 1;
    xoff = xoff_quotient + round_xoff;
    yoff = yoff_quotient + round_yoff;
    width = width_quotient + round_width;
    height = height_quotient + round_height;
    if (width < 0)
        width = -width;
    if (height < 0)
        height = -height;

    poly->tpage = GetTPage(1, 0, texture->x, texture->y & 0xf00);
    poly->u0 = uv_rect->u;
    poly->v0 = uv_rect->v;
    poly->u1 = uv_rect->u + (u8)uv_rect->width;
    poly->v1 = uv_rect->v;
    poly->u2 = uv_rect->u;
    poly->v2 = uv_rect->v + (u8)uv_rect->height;
    poly->u3 = uv_rect->u + (u8)uv_rect->width;
    poly->v3 = uv_rect->v + (u8)uv_rect->height;
    poly->x0 = position->x + xoff + offset[4];
    poly->y0 = position->y + yoff + offset[5];
    poly->x1 = position->x + xoff + offset[4] + width;
    poly->y1 = position->y + yoff + offset[5];
    poly->x2 = position->x + xoff + offset[4];
    poly->y2 = position->y + yoff + offset[5] + height;
    poly->x3 = position->x + xoff + offset[4] + width;
    poly->y3 = position->y + yoff + offset[5] + height;
}
