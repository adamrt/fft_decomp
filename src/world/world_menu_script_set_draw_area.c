#include "fft/world.h"

/* Apply a four-byte draw area from the menu script and advance the command. */
menu_instruction_t* world_menu_script_set_draw_area(menu_instruction_t* instruction) {
    RECT rect;

    if (g_gfx_otag_index_locked == 0) {
        rect.x = instruction->draw_area.x;
        rect.y = instruction->draw_area.y;
        rect.w = instruction->draw_area.width;
        rect.h = instruction->draw_area.height;
        world_gfx_add_draw_area_primitive(&rect, g_menu_gfx_otag_index - 1);
    }
    return menu_next_instruction(instruction);
}
