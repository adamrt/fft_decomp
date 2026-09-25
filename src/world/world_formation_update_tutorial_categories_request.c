#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/types.h"

void world_formation_update_tutorial_categories_request(void) {
    s16 state = g_world_tutorial_categories_request_state;

    if (state == 0) {
        return;
    }
    if (state == 1) {
        g_world_menu_screen_open_request = 1;
        wldcore_fade_start_screen(0x12, 0x10);
        g_world_tutorial_categories_request_state++;
        return;
    }
    if (state == 2) {
        if (!(g_main_system_flags & 8)) {
            g_world_tutorial_categories_request_state = state + 1;
        }
        return;
    }
    world_formation_reset_menu_context();
    world_menu_set_brightness(0x80, 0x80, 0x80);
    g_main_system_flags |= 0x100000;
    g_world_tutorial_categories_request_state = 0;
}
