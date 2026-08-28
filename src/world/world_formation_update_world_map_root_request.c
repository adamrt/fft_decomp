#include "fft/main_runtime.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/types.h"

/* Sibling of world_formation_update_tutorial_categories_request driven by g_world_map_root_request_state. */
void world_formation_update_world_map_root_request(void) {
    s16 state = g_world_map_root_request_state;

    if (state == 0) {
        return;
    }
    if (state == 1) {
        g_world_menu_screen_open_request = 1;
        wldcore_fade_start_screen(0x12, 0x10);
        g_world_map_root_request_state++;
        return;
    }
    if (state == 2) {
        if (!(g_main_system_flags & 8)) {
            g_world_map_root_request_state = state + 1;
        }
        return;
    }
    if (state == 3) {
        world_formation_reset_menu_context();
        g_world_map_root_request_state = 0;
        g_main_system_flags |= 0x100;
    }
}
