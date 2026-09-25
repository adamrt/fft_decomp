#include "fft/world.h"
#include "psx/types.h"

void world_formation_update_debug_variable_list_request(void) {
    s32 state = g_world_debug_variable_list_request_state;

    if (state != 0 && state == 1) {
        g_world_menu_screen_open_request = 1;
        world_formation_reset_menu_context();
        world_menu_set_brightness(0x80, 0x80, 0x80);
        g_world_debug_variable_list_request_state = 0;
        g_main_system_flags |= 0x800000;
    }
}
