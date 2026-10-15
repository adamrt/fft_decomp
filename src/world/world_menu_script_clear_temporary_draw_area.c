#include "fft/world.h"

/* Remove the active temporary draw-area clip and advance the menu script. */
menu_instruction_t* world_menu_script_clear_temporary_draw_area(menu_instruction_t* instruction) {
    if (g_gfx_otag_index_locked != 0) {
        g_world_gfx_temporary_draw_area.y = 0;
        gfx_add_draw_area_primitive(&g_world_gfx_temporary_draw_area, g_menu_gfx_otag_index + 1);
        g_gfx_otag_index_locked = 0;
    }
    return menu_next_instruction(instruction);
}
