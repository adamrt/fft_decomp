#include "fft/wldcore.h"
#include "psx/types.h"

/* Appends a window-list record: saves g_wldcore_menu_ordering_table_offset into it, allocates a window
 * record from the render-object queue, initialises that window's delay and
 * counters, and applies layout 0 to it before advancing the list count.
 *
 * Writing the window columns as fields of g_wldcore_window_records (not separate stride
 * arrays) keeps the scheduler from sinking the record's window_index store
 * below them. */
void wldcore_menu_push_layout_window_level(void) {
    s32 window;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].layout_window.saved_state
        = g_wldcore_menu_ordering_table_offset;
    g_wldcore_menu_ordering_table_offset = 1;
    window = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].layout_window.window_index = window;
    g_wldcore_window_records[window].sequence = 0x78;
    g_wldcore_window_records[window].priority = 2;
    g_wldcore_window_records[window].anim_counter = 0;
    g_wldcore_window_records[window].frame_index = 0;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].layout_window.layout = 0;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].layout_window.cached_layout_kind = 1;
    wldcore_window_apply_layout(&g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].layout_window);
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_LAYOUT_WINDOW;
    g_wldcore_menu_stack_depth++;
}
