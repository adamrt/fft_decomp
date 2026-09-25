#include "fft/wldcore.h"
#include "psx/gte.h"

/* Provisional: the menu entry handed in; only its record index is read. */
typedef struct {
    s32 record_index;
} wldcore_map_pop_level_entry_t;

/* Closes one menu level. While a projection move is active it advances the
 * move, sets projection-state flag 1 and system flag 2, and returns;
 * otherwise it pops the level, moves the parent level's window to the entry's
 * record position (y - 4), clears system flag 0x2000, and dispatches the
 * parent level's handler.
 *
 * The entry must be a struct pointer: the target hoists its reload above the
 * scalar g_main_system_flags store, which GCC only allows for an in-struct
 * access. */
void wldcore_map_step_projection_motion_then_pop_level(wldcore_map_pop_level_entry_t* entry) {
    wldcore_projection_state_t* state;
    s32 level;

    if (g_wldcore_map_projection_motion.flags & 1) {
        wldcore_map_step_projection_motion();
        state = &g_wldcore_map_projection_state;
        state->flags |= 1;
        g_main_system_flags |= 2;
        return;
    }
    level = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_depth = level - 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[level - 2].window_index].x
        = g_wldcore_map_dot_screen_x[entry->record_index].value;
    g_main_system_flags &= ~0x2000;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[level - 2].window_index].y
        = g_wldcore_map_dot_screen_x[entry->record_index].y - 4;
    wldcore_menu_dispatch_resume_handler();
}
