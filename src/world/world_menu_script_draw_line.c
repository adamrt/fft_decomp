#include "fft/world.h"

/* Draw the command's two endpoints with its RGB color, then advance. */
menu_instruction_t* world_menu_script_draw_line(menu_instruction_t* instruction) {
    point16_t endpoints[2];

    endpoints[0].x = instruction->line.x0;
    endpoints[0].y = instruction->line.y0;
    endpoints[1].x = instruction->line.x1;
    endpoints[1].y = instruction->line.y1;
    gfx_enqueue_line_f2(endpoints, &instruction->line.color, g_menu_gfx_semitransparency, g_menu_gfx_otag_index);
    return menu_next_instruction(instruction);
}
