/* Scaled 8bpp POLY_FT4 setup and the zodiac primitives. */

#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/menu.h"
#include "fft/zodiac.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* MAIN, WORLD, ATTACK, CARD, DEBUGCHR and REQUIRE each link this 29-sprite catalog. */
extern const u8 g_zodiac_sprite_count;
extern const point16_t g_zodiac_sprite_scales[29];
extern const point16_t g_zodiac_texture_page_origin;
extern const sprite_sheet_frame_t g_zodiac_sprite_frames[29];

#if defined(FFT_EVENT_CARD) || defined(FFT_EVENT_DEBUGCHR) || defined(FFT_EVENT_REQUIRE) || defined(FFT_WORLD)
/* Build an 8bpp sprite quad, rounding positive fixed-point remainders at half a pixel. */
void gfx_set_scaled_poly_ft4_8bpp(POLY_FT4* poly, const point16_t* texture, const RECT* position,
    const sprite_sheet_frame_t* uv_rect, const point16_t* scale, const sprite_draw_offset_t* offset) {
    s32 xoff = uv_rect->offset.x * scale->x;
    s32 yoff = uv_rect->offset.y * scale->y;
    s32 width = uv_rect->source.w * scale->x;
    s32 height = uv_rect->source.h * scale->y;
    s32 round_xoff = 0;
    s32 round_yoff = 0;
    s32 round_width = 0;
    s32 round_height = 0;
    s32 xoff_quotient;
    s32 yoff_quotient;
    s32 width_quotient;
    s32 height_quotient;

    xoff_quotient = xoff / ONE;
    if (xoff % ONE >= ONE / 2)
        round_xoff = 1;
    yoff_quotient = yoff / ONE;
    if (yoff % ONE >= ONE / 2)
        round_yoff = 1;
    width_quotient = width / ONE;
    if (width % ONE >= ONE / 2)
        round_width = 1;
    height_quotient = height / ONE;
    if (height % ONE >= ONE / 2)
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
    setUV4(poly, uv_rect->source.x, uv_rect->source.y, uv_rect->source.x + uv_rect->source.w, uv_rect->source.y,
        uv_rect->source.x, uv_rect->source.y + uv_rect->source.h, uv_rect->source.x + uv_rect->source.w,
        uv_rect->source.y + uv_rect->source.h);
    setXY4(poly, position->x + xoff + offset->position.x, position->y + yoff + offset->position.y,
        position->x + xoff + offset->position.x + width, position->y + yoff + offset->position.y,
        position->x + xoff + offset->position.x, position->y + yoff + offset->position.y + height,
        position->x + xoff + offset->position.x + width, position->y + yoff + offset->position.y + height);
}
#endif

/* Build the zodiac sprites, each scaled by the context's Q12 scale (rounded) and shaded by its
 * brightness, and the dark 255 x 255 backdrop behind them. The sprites are linked into
 * context->ot only when link_primitives is set; otherwise each one is built into the same packet,
 * which the backdrop then overwrites. */
void zodiac_build_primitives(zodiac_draw_context_t* context, POLY_FT4* poly) {
    s32 i;
    s32 x;
    s32 y;
    point16_t scale;

    for (i = 0; i < g_zodiac_sprite_count; i++) {
        SetPolyFT4(poly);
        poly->clut = GetClut(0, 0x1fd);
        setRGB0(poly, context->brightness, context->brightness, context->brightness);
        SetSemiTrans(poly, 1);
        SetShadeTex(poly, 0);
        x = g_zodiac_sprite_scales[i].x;
        x *= context->scale_x;
        y = g_zodiac_sprite_scales[i].y;
        y *= context->scale_y;
        if (x % ONE >= 0x800) {
            x += 1;
        }
        if (y % ONE >= 0x800) {
            y += 1;
        }
        x /= ONE;
        y /= ONE;
        scale.x = x;
        scale.y = y;
        gfx_set_scaled_poly_ft4_8bpp(poly, &g_zodiac_texture_page_origin, &g_menu_draw_area_template,
            &g_zodiac_sprite_frames[i], &scale, &context->offset);
        if (context->link_primitives != 0) {
            addPrim(context->ot, poly);
            poly++;
        }
    }
    /* The backdrop is built as a POLY_F4 but its vertices are stored through
     * the POLY_FT4 layout (x1 at 0x10, x2 at 0x18, x3 at 0x20), as the target does. */
    SetPolyF4((POLY_F4*)poly);
    setRGB0(poly, 8, 8, 8);
    SetSemiTrans(poly, 1);
    SetShadeTex(poly, 0);
    setXY4(poly, 0, 0, 0xff, 0, 0, 0xff, 0xff, 0xff);
    addPrim(context->ot, poly);
}
