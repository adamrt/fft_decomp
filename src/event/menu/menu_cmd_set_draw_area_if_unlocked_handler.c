#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "psx/libgpu.h"

/* Queue the instruction draw area unless the zoom transition already queued one. */
menu_instruction_t* menu_cmd_set_draw_area_if_unlocked_handler(menu_instruction_t* data) {
    RECT area;

    if (g_gfx_otag_index_locked == 0) {
        area.x = data->draw_area.x;
        area.y = data->draw_area.y;
        area.w = data->draw_area.width;
        area.h = data->draw_area.height;
        menu_gfx_enqueue_draw_area(&area, g_menu_gfx_otag_index - 1);
    }
    return menu_next_instruction(data);
}
