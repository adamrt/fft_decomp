#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

void world_card_open_menu(void) {
    g_world_card_menu_request_state = 1;
    g_world_formation_menu_new_buttons = 0;
    world_input_store_menu_state(&g_world_formation_menu_input_state);
    world_thread_exit_current();
}
