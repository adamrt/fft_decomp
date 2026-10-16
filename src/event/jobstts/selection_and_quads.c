/* Wrapped horizontal selection and textured quad rects. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/status_screen.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

static u16 g_status_screen_menu_selection_values[8] = { 0 };

/* Step selection value index left or right on the D-pad, wrapping within count, and return it. */
s32 status_screen_update_wrapped_horizontal_selection(u16 count, u8 index, s32 buttons) {
    if (buttons & PSX_PAD_LEFT) {
        g_status_screen_menu_selection_values[index] = (g_status_screen_menu_selection_values[index] == 0)
            ? count - 1
            : g_status_screen_menu_selection_values[index] - 1;
    } else if (buttons & PSX_PAD_RIGHT) {
        g_status_screen_menu_selection_values[index] = (g_status_screen_menu_selection_values[index] < count - 1)
            ? g_status_screen_menu_selection_values[index] + 1
            : 0;
    }
    return g_status_screen_menu_selection_values[index];
}

/* Takes one POLY_FT4 from the pool, fills it as the rect->w x rect->h quad at
 * (rect->x + 0x80, rect->y) textured with the icon source's cell, CLUT and
 * texture page, and links it into ordering table entry ot_index. With
 * `color` the quad is shaded with it. */
void menu_gfx_enqueue_textured_quad_rect(
    RECT* rect, menu_icon_source_t* tex, rgb24_t* color, s32 semitrans, s32 ot_index) {
    POLY_FT4* poly;

    poly = &g_menu_gfx_context->textured_quads[g_menu_gfx_poly_ft4_count++];

    if (color != 0) {
        SetShadeTex(poly, 0);
        setRGB0(poly, color->r, color->g, color->b);
    } else {
        SetShadeTex(poly, 1);
    }
    SetSemiTrans(poly, semitrans);

    setXYWH(poly, rect->x + 0x80, rect->y, rect->w, rect->h);
    setUVWH(poly, tex->u, tex->v, tex->w, tex->h);
    poly->clut = tex->clut;
    poly->tpage = tex->tpage;

    setaddr(poly, g_menu_gfx_context->otag[ot_index]);
    catPrim(&g_menu_gfx_context->otag[ot_index], poly);
}
