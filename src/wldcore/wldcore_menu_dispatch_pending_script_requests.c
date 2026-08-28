#include "fft/wldcore.h"

s32 wldcore_script_process_conditional_set(s32 index, s32 mask);
void wldcore_menu_push_focus_location_level(s32 id);
void wldcore_menu_push_map_path_level(s32 value, s32 value_hi, s32 flag);
void wldcore_push_map_location_visibility_level(s32 value, s32 flag);

/* Once the current location's script yields a map-menu action, reset the
 * current level's window and dispatch its path, location-visibility or focus
 * request. A focus location of 0xff means the current location.
 *
 * The flag tests are signed `> 0` comparisons; `!= 0` folds to srl/andi. */
void wldcore_menu_dispatch_pending_script_requests(void) {
    s32* location = &g_wldcore_map_projection_state.marker.kind;
    s32* flags;
    s32 id;

    if (wldcore_script_process_conditional_set(*location, WLDCORE_SCRIPT_ACTION_MAP_MENU_MASK) == 0) {
        return;
    }
    flags = &g_wldcore_script_state.flags;
    if (*flags & WLDCORE_SCRIPT_ACTION_PATH_MASK) {
        wldcore_window_reset_state_and_scroll(&g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth]);
        wldcore_menu_push_map_path_level(g_wldcore_script_state.args[0], g_wldcore_script_state.args[1],
            (*flags & WLDCORE_SCRIPT_ACTION_ERASE_PATH) > 0);
    } else if (*flags & WLDCORE_SCRIPT_ACTION_LOCATION_VISIBILITY_MASK) {
        wldcore_window_reset_state_and_scroll(&g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth]);
        wldcore_push_map_location_visibility_level(
            g_wldcore_script_state.args[0], (*flags & WLDCORE_SCRIPT_ACTION_ERASE_LOCATION) > 0);
    } else if (*flags & WLDCORE_SCRIPT_ACTION_FOCUS_LOCATION) {
        wldcore_window_reset_state_and_scroll(&g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth]);
        id = g_wldcore_script_state.args[0];
        if (id == 0xFF) {
            id = *location;
        }
        wldcore_menu_push_focus_location_level(id);
    }
}
