#include "fft/wldcore.h"

/* Start drawing or erasing a map path and hide the parent window until the
 * animation finishes. Indexed stores keep their named globals to preserve
 * the original address sequences. */
void wldcore_menu_push_map_path_level(s32 path_id, s32 path_id_hi, s32 erase) {
    s32 depth;
    s32 window_index;

    wldcore_map_prepare_path_animation(path_id, path_id_hi);
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].path.path_id = path_id;
    g_wldcore_menu_stack_records_next[depth].path.path_id_hi = path_id_hi;
    g_wldcore_menu_stack_records_next[depth].path.erase = erase;
    g_wldcore_menu_stack_records_next[depth].path.pending_sound = 1;
    g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
    window_index = g_wldcore_menu_stack_records_next[depth - 1].window_index;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_MAP_PATH;
    g_wldcore_window_records[window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_depth = depth + 1;
}
