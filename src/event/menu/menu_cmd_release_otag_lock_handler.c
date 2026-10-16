/* The menu command that restores the full draw area after a zoomed window and clears the lock. */

#include "fft/gfx_primitive.h"
#include "fft/menu_gfx.h"
#include "fft/menu_script.h"

/* Stream command closing a zoomed window: when the zoom transition queued a
 * cropped draw area, queue the full draw area again at the next
 * ordering-table entry and clear the lock. Unlike JOBSTTS's own copy, the
 * one BUNIT and EQUIP link does not advance the ordering-table index. */
menu_instruction_t* menu_cmd_release_otag_lock_handler(menu_instruction_t* data) {
    if (g_gfx_otag_index_locked != 0) {
        g_gfx_draw_area.y = 0;
        menu_gfx_enqueue_draw_area(&g_gfx_draw_area, g_menu_gfx_otag_index + 1);
        g_gfx_otag_index_locked = 0;
    }
    return menu_next_instruction(data);
}
