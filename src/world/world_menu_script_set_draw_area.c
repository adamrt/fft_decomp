#include "fft/world.h"

/* Apply a four-byte draw area from the menu script and advance the command. */
u8* world_menu_script_set_draw_area(u8* script) {
    RECT rect;

    if (g_world_menu_temporary_draw_area_active == 0) {
        rect.x = script[2];
        rect.y = script[3];
        rect.w = script[4];
        rect.h = script[5];
        world_gfx_add_draw_area_primitive(&rect, g_world_menu_draw_priority - 1);
    }
    return script + script[1];
}
