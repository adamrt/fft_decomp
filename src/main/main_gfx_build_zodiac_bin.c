#include "fft/main.h"
#include "psx/gte.h"

void main_gfx_build_zodiac_bin(zodiac_draw_context_t* context, POLY_FT4* poly) {
    s32 i;
    s32 x;
    s32 y;
    main_zodiac_scale_t scale;

    for (i = 0; i < g_main_gfx_zodiac_sprite_count; i++) {
        SetPolyFT4(poly);
        poly->clut = GetClut(0, 0x1fd);
        poly->r0 = context->brightness;
        poly->g0 = context->brightness;
        poly->b0 = context->brightness;
        SetSemiTrans(poly, 1);
        SetShadeTex(poly, 0);
        x = g_main_gfx_zodiac_sprite_scales[i].x;
        x *= context->scale_x;
        y = g_main_gfx_zodiac_sprite_scales[i].y;
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
        main_gfx_build_zodiac_sprite_polygon(poly, &g_main_gfx_zodiac_texture_position,
            &g_main_gfx_zodiac_screen_position, &g_main_gfx_zodiac_sprite_frames[i], &scale, &context->offset);
        if (context->link_primitives != 0) {
            setaddr(poly, getaddr(context->ot));
            setaddr(context->ot, poly);
            poly++;
        }
    }
    /* The backdrop is built as a POLY_F4 but its vertices are stored through
     * the POLY_FT4 layout (x1 at 0x10, x2 at 0x18, x3 at 0x20), as the target does. */
    SetPolyF4((POLY_F4*)poly);
    poly->r0 = 8;
    poly->g0 = 8;
    poly->b0 = 8;
    SetSemiTrans(poly, 1);
    SetShadeTex(poly, 0);
    poly->x0 = 0;
    poly->y0 = 0;
    poly->x1 = 0xff;
    poly->y1 = 0;
    poly->x2 = 0;
    poly->y2 = 0xff;
    poly->x3 = 0xff;
    poly->y3 = 0xff;
    setaddr(poly, getaddr(context->ot));
    setaddr(context->ot, poly);
}
