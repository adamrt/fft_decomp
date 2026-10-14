#include "fft/gfx.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * Build the formation view's primitive list: one textured quad per graphic
 * entry, shaded with the view color and placed at the entry's scaled
 * offset, followed by a black backdrop quad; the backdrop is always linked
 * into the ordering table and the quads only when the view asks for it.
 */
void world_gfx_build_zodiac_primitives(zodiac_draw_context_t* view, POLY_FT4* poly) {
    point16_t scale;
    s32 i;
    s32 x;
    s32 y;

    for (i = 0; i < g_world_formation_view_graphic_count; i++) {
        SetPolyFT4(poly);
        poly->clut = GetClut(0, 0x1FD);
        setRGB0(poly, view->brightness, view->brightness, view->brightness);
        SetSemiTrans(poly, 1);
        SetShadeTex(poly, 0);
        x = g_world_formation_view_graphic_scales[i].x;
        y = g_world_formation_view_graphic_scales[i].y;
        x *= view->scale_x;
        y *= view->scale_y;
        if (x - (x / ONE) * ONE >= 0x800) {
            x += 1;
        }
        if (y - (y / ONE) * ONE >= 0x800) {
            y += 1;
        }
        x /= ONE;
        y /= ONE;
        scale.x = x;
        scale.y = y;
        gfx_set_scaled_poly_ft4_8bpp(poly, &g_world_formation_view_texture_page, &g_world_gfx_full_texture_window,
            &g_world_formation_view_graphic_rects[i], &scale, &view->offset);
        if (view->link_primitives != 0) {
            addPrim(view->ot, poly);
            poly++;
        }
    }
    /* The backdrop keeps the POLY_FT4 slot layout; only its tag is F4. */
    SetPolyF4((POLY_F4*)poly);
    setRGB0(poly, 8, 8, 8);
    SetSemiTrans(poly, 1);
    SetShadeTex(poly, 0);
    setXY4(poly, 0, 0, 0xFF, 0, 0, 0xFF, 0xFF, 0xFF);
    addPrim(view->ot, poly);
}
