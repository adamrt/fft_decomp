#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/types.h"

/*
 * Submit a primitive for the active rendering mode.
 *
 * Immediate rendering draws directly. Deferred rendering links
 * the primitive into the current ordering-table entry.
 */
void gfx_draw_or_append_gpu_primitive(void* primitive) {
    if (g_gfx_draw_primitives_immediately != 0) {
        DrawPrim(primitive);
        return;
    }
    addPrim(g_gfx_current_otag_entry, primitive);
}

/* Link a GPU primitive into the current ordering table entry (libgpu addPrim). */
void gfx_append_gpu_primitive_to_secondary_otag(void* prim) {
    u32* entries = g_gfx_current_otag_entry;

    addPrim(&entries[1], prim);
}

/* Store the rectangle text variables $7E and $7F substitute. */
void text_set_substitution_values_7e_7f(s32 x, s32 y, s32 width, s32 height) {
    g_text_substitution_rect.x = x;
    g_text_substitution_rect.y = y;
    g_text_substitution_rect.w = width;
    g_text_substitution_rect.h = height;
}

/* Set the screen origin used by menu text glyphs. */
void menu_set_text_origin(s32 x, s32 y) {
    g_menu_text_state.pen.rect.x = x;
    g_menu_text_state.pen.rect.y = y;
}
