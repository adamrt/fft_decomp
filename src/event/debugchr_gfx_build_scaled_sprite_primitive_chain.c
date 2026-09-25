#include "fft/debugchr.h"
#include "psx/types.h"

/* DEBUGCHR copy of main_gfx_build_zodiac_bin. */
void debugchr_gfx_build_scaled_sprite_primitive_chain(
    battle_menu_status_panel_scaled_sprite_t* sprite, POLY_FT4* poly) {
    s32 i;
    s32 x;
    s32 y;
    point16_t scale;

    for (i = 0; i < g_debugchr_gfx_scaled_sprite_count; i++) {
        SetPolyFT4(poly);
        poly->clut = GetClut(0, 0x1fd);
        poly->r0 = sprite->intensity;
        poly->g0 = sprite->intensity;
        poly->b0 = sprite->intensity;
        SetSemiTrans(poly, 1);
        SetShadeTex(poly, 0);
        x = g_debugchr_gfx_scaled_sprite_scale[i].x;
        x *= sprite->scale_x;
        y = g_debugchr_gfx_scaled_sprite_scale[i].y;
        y *= sprite->scale_y;
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
        debugchr_gfx_set_scaled_poly_ft4_geometry_and_uv(poly, &g_debugchr_gfx_scaled_sprite_texture_page,
            &g_debugchr_gfx_draw_area_template, &g_debugchr_gfx_scaled_sprite_uv_rects[i], &scale, sprite->offset);
        if (sprite->link_primitive != 0) {
            setaddr(poly, getaddr(sprite->ordering_tag));
            setaddr(sprite->ordering_tag, poly);
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
    setaddr(poly, getaddr(sprite->ordering_tag));
    setaddr(sprite->ordering_tag, poly);
}
