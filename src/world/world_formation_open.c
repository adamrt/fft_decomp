#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

void world_formation_open(void) {
    g_world_formation_open_request_state = 1;
    g_world_formation_menu_new_buttons = 0;
    world_input_store_menu_state(&g_world_formation_menu_input_state);
    world_thread_exit_current();
}
