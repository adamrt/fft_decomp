#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "psx/types.h"

/* Stream command closing a zoomed window: when the zoom transition queued a
 * cropped draw area, queue the full draw area again one ordering-table entry
 * above the window's contents, move on to the next entry and clear the
 * lock. */
menu_instruction_t* menu_cmd_release_otag_lock_handler_variant(menu_instruction_t* data) {
    s16 otag_index;
    if (g_gfx_otag_index_locked != 0) {
        otag_index = g_menu_gfx_otag_index + 1;
        g_gfx_draw_area.y = 0;
        g_menu_gfx_otag_index = otag_index;
        menu_gfx_enqueue_draw_area(&g_gfx_draw_area, otag_index);
        g_gfx_otag_index_locked = 0;
        g_menu_gfx_otag_index = g_menu_gfx_otag_index + 1;
    }
    return menu_next_instruction(data);
}
