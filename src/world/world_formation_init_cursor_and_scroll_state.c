#include "fft/world.h"

void world_formation_init_cursor_and_scroll_state(s32 formation_index) {
    world_formation_init_selection_cursor(formation_index);
    g_world_formation_stat_display_mode = 0;
    g_world_formation_scroll_enabled_latch = g_world_formation_scroll_enabled;
    if (g_world_formation_scroll_enabled != 0) {
        g_world_status_display_thread_params.y = 0x90;
    } else {
        g_world_status_display_thread_params.y = 0;
    }
    g_world_formation_unit_banner_enabled = 1;
    g_world_formation_saved_banner_enabled = 1;
}
