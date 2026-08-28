#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_menu_reset_window_list_and_store_origin(void) {
    g_wldcore_menu_stack_depth = 0;
    wldcore_menu_push_root_level();
    g_wldcore_window_records[g_wldcore_menu_stack_records[1].window_index].x
        = g_wldcore_map_projection_state.marker.screen_x;
    g_wldcore_window_records[g_wldcore_menu_stack_records[1].window_index].y
        = g_wldcore_map_projection_state.marker.screen_y - 4;
}
