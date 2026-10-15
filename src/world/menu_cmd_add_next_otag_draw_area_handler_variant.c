#include "fft/world.h"

/* Menu-script opcode handler: unless the flag is set, clears the y of
 * g_world_gfx_temporary_draw_area and re-registers the draw area at draw
 * priority + 1. Returns the address of the next instruction. */
menu_instruction_t* menu_cmd_add_next_otag_draw_area_handler_variant(menu_instruction_t* instruction) {
    if (g_gfx_otag_index_locked == 0) {
        g_world_gfx_temporary_draw_area.y = 0;
        world_gfx_add_draw_area_primitive(&g_world_gfx_temporary_draw_area, g_menu_gfx_otag_index + 1);
    }
    return menu_next_instruction(instruction);
}
