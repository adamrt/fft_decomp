#include "fft/world.h"
#include "psx/types.h"

void world_formation_request_debug_variable_list(void) {
    g_world_debug_variable_list_request_state = 1;
    g_world_formation_menu_new_buttons = 0;
    world_input_store_menu_state(&g_world_formation_menu_input_state);
    world_thread_exit_current();
}
