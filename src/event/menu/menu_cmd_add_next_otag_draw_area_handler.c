/* The menu command that queues the full draw area at the next ordering-table entry. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "psx/types.h"

/* Queue the full draw area at the next ordering-table slot when unlocked. */
menu_instruction_t* menu_cmd_add_next_otag_draw_area_handler(menu_instruction_t* instruction) {
    if (g_gfx_otag_index_locked == 0) {
        s32 otag_index = g_menu_gfx_otag_index;
        g_gfx_draw_area.y = 0;
        menu_gfx_enqueue_draw_area(&g_gfx_draw_area, otag_index + 1);
    }
    return menu_next_instruction(instruction);
}
