#include "fft/world.h"

/* a0 is forwarded unchanged to the selected palette helper. */
void world_menu_select_primitive_color_palette(world_menu_palette_primitives_t* menu, world_menu_color_input_t* input) {
    if (input->style == 1 || g_world_thread_task_active == 1) {
        world_menu_init_primitive_colors_palette_bank_1(menu);
        return;
    }
    world_menu_init_primitive_colors_palette_bank_0(menu);
}
