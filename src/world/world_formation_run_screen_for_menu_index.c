#include "fft/world.h"
#include "psx/types.h"

void world_formation_run_screen_for_menu_index(s32 menu_index) {
    world_formation_reset_menu_context();
    world_formation_run_screen(menu_index + 1, 0);
    world_formation_reset_menu_context();
    g_world_menu_screen_open_request = 0;
    g_world_menu_thread_menu_data = g_world_menu_default_entries;
}
