#include "fft/event_jobstts.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Stream command closing a zoomed window: when the zoom transition queued a
 * cropped draw area, queue the full draw area again one ordering-table entry
 * above the window's contents, move on to the next entry and clear the
 * lock. */
menu_instruction_t* jobstts_cmd_release_otag_lock_handler(menu_instruction_t* data) {
    s16 otag_index;
    if (g_jobstts_gfx_otag_index_locked != 0) {
        otag_index = g_menu_gfx_otag_index + 1;
        g_jobstts_gfx_draw_area.y = 0;
        g_menu_gfx_otag_index = otag_index;
        menu_gfx_enqueue_draw_area(&g_jobstts_gfx_draw_area, otag_index);
        g_jobstts_gfx_otag_index_locked = 0;
        g_menu_gfx_otag_index = g_menu_gfx_otag_index + 1;
    }
    return menu_next_instruction(data);
}
