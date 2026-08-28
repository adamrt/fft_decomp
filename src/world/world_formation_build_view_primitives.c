#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/*
 * Build the formation view's primitive list: one textured quad per graphic
 * entry, shaded with the view colour and placed at the entry's scaled
 * offset, followed by a black backdrop quad; the backdrop is always linked
 * into the ordering table and the quads only when the view asks for it.
 */
void world_formation_build_view_primitives(world_formation_view_t* view, POLY_FT4* poly) {
    world_formation_screen_point_t position;
    s32 i;
    s32 x;
    s32 y;

    for (i = 0; i < g_world_formation_view_graphic_count; i++) {
        SetPolyFT4(poly);
        poly->clut = GetClut(0, 0x1FD);
        poly->r0 = view->color;
        poly->g0 = view->color;
        poly->b0 = view->color;
        SetSemiTrans(poly, 1);
        SetShadeTex(poly, 0);
        x = g_world_formation_view_graphic_offsets[i].x;
        y = g_world_formation_view_graphic_offsets[i].y;
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
        position.x = x;
        position.y = y;
        world_formation_build_graphic_quad(poly, &g_world_formation_view_texture_page,
            (world_formation_screen_point_t*)&g_world_gfx_full_texture_window, &g_world_formation_view_graphic_rects[i],
            &position, (world_formation_sprite_origin_t*)view->origin);
        if (view->link_quads != 0) {
            setaddr(poly, getaddr(view->ordering_table));
            setaddr(view->ordering_table, poly);
            poly++;
        }
    }
    /* The backdrop keeps the POLY_FT4 slot layout; only its tag is F4. */
    SetPolyF4((POLY_F4*)poly);
    poly->r0 = 8;
    poly->g0 = 8;
    poly->b0 = 8;
    SetSemiTrans(poly, 1);
    SetShadeTex(poly, 0);
    poly->x0 = 0;
    poly->y0 = 0;
    poly->x1 = 0xFF;
    poly->y1 = 0;
    poly->x2 = 0;
    poly->y2 = 0xFF;
    poly->x3 = 0xFF;
    poly->y3 = 0xFF;
    setaddr(poly, getaddr(view->ordering_table));
    setaddr(view->ordering_table, poly);
}
