#include "fft/wldcore.h"

/* Closes one menu level. While a projection move is active it advances the
 * move, sets projection-state flag 1 and system flag 2, and returns;
 * otherwise it pops the level, moves the parent level's window to the focused
 * dot's position (y - 4), clears system flag 0x2000, and dispatches the
 * parent level's handler.
 *
 * The entry must be a struct pointer: the target hoists its reload above the
 * scalar g_main_system_flags store, which GCC only allows for an in-struct
 * access. */
void wldcore_map_step_projection_motion_then_pop_level(wldcore_menu_stack_record_t* entry) {
    wldcore_projection_state_t* state;
    s32 level;

    if (g_wldcore_map_projection_motion.flags & WLDCORE_PROJECTION_MOTION_ACTIVE) {
        wldcore_map_step_projection_motion();
        state = &g_wldcore_map_projection_state;
        state->flags |= WLDCORE_PROJECTION_FLAG_UPDATE_POSITION;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_DRAW_MAP;
        return;
    }
    level = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_depth = level - 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[level - 2].window_index].position.x
        = g_wldcore_map_dots[entry->focus_dot_index].render.position.x;
    g_main_system_flags &= ~MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[level - 2].window_index].position.y
        = g_wldcore_map_dots[entry->focus_dot_index].render.position.y - 4;
    wldcore_menu_dispatch_resume_handler();
}
