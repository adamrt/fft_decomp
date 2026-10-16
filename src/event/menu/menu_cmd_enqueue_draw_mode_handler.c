/* The menu command that queues a draw mode with a blend rate. */

#include "fft/menu.h"
#include "fft/menu_script.h"
#include "psx/libgpu.h"

/* Queue a draw mode with the requested blend rate at the current OT entry. */
menu_instruction_t* menu_cmd_enqueue_draw_mode_handler(menu_instruction_t* data) {
    menu_gfx_enqueue_draw_mode(0, 0, GetTPage(0, data->blend.blend_rate, 0x100, 0), 0, g_menu_gfx_otag_index);
    return menu_next_instruction(data);
}
