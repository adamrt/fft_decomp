#include "fft/wldcore.h"
#include "psx/types.h"

/* Push a map-dot focus level and hide its parent window during the projection move.
 * Keeping the render pointer as the base preserves the two related call addresses. */
void wldcore_menu_push_focus_location_level(s32 dot_index) {
    world_animated_render_record_t* render;

    render = &g_wldcore_map_dots[dot_index].render;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].focus_dot_index = dot_index;
    wldcore_map_start_projection_motion_if_outside_bounds(&render->position, &g_wldcore_map_dots[dot_index].position);
    if (g_wldcore_map_projection_motion.flags & WLDCORE_PROJECTION_MOTION_ACTIVE) {
        g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].window_index].flags
            |= WORLD_RENDER_RECORD_HIDDEN;
    }
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_FOCUS_LOCATION;
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth + 1;
}
