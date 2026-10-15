#include "fft/menu.h"

/* Build the scaled zodiac sprites and their backdrop. */
void menu_build_zodiac_primitives(zodiac_draw_context_t* context, POLY_FT4* poly) {
    s32 i;
    s32 x;
    s32 y;
    point16_t scale;

    for (i = 0; i < g_menu_gfx_scaled_sprite_count; i++) {
        SetPolyFT4(poly);
        poly->clut = GetClut(0, 0x1fd);
        setRGB0(poly, context->brightness, context->brightness, context->brightness);
        SetSemiTrans(poly, 1);
        SetShadeTex(poly, 0);
        x = g_menu_gfx_scaled_sprite_scale[i].x;
        x *= context->scale_x;
        y = g_menu_gfx_scaled_sprite_scale[i].y;
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
        gfx_set_scaled_poly_ft4_8bpp(poly, &g_menu_gfx_scaled_sprite_texture_page, &g_menu_draw_area_template,
            &g_menu_gfx_scaled_sprite_uv_rects[i], &scale, &context->offset);
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
