#include "fft/wldcore.h"

/* Empty the menu stack, push the root level and place its window at the
 * projected location marker (4 pixels above it). */
void wldcore_menu_reset_window_list_and_store_origin(void) {
    g_wldcore_menu_stack_depth = 0;
    wldcore_menu_push_root_level();
    g_wldcore_window_records[g_wldcore_menu_stack_records[1].window_index].position.x
        = g_wldcore_map_projection_state.marker.render.position.x;
    g_wldcore_window_records[g_wldcore_menu_stack_records[1].window_index].position.y
        = g_wldcore_map_projection_state.marker.render.position.y - 4;
}
