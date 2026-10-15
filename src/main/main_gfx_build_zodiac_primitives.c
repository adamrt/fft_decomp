#include "fft/main.h"
#include "psx/libgte.h"

/* Build the ZODIAC.BIN backdrop of the deployment screen: one textured quad per
 * record of g_main_gfx_zodiac_sprite_frames, then a dark (8, 8, 8)
 * semi-transparent quad over (0, 0)..(255, 255).
 *
 * Each sprite is scaled by its own table scale times the context scale (ONE =
 * 1.0) and shaded with the context brightness. The rounding step adds 1 to the
 * product before the division by ONE, so it only rounds up a remainder of ONE -
 * 1. The dark quad is always linked into context->ot. The sprites are linked only
 * when context->link_primitives is set; otherwise poly does not advance, so every
 * sprite and then the dark quad are built in the same primitive. */
void main_gfx_build_zodiac_primitives(zodiac_draw_context_t* context, POLY_FT4* poly) {
    s32 i;
    s32 x;
    s32 y;
    point16_t scale;

    for (i = 0; i < g_main_gfx_zodiac_sprite_count; i++) {
        SetPolyFT4(poly);
        poly->clut = GetClut(0, 0x1fd);
        setRGB0(poly, context->brightness, context->brightness, context->brightness);
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
        gfx_build_zodiac_sprite_polygon(poly, &g_main_gfx_zodiac_texture_position, &g_main_gfx_zodiac_screen_position,
            &g_main_gfx_zodiac_sprite_frames[i], &scale, &context->offset);
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
