#include "fft/script_variables.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/types.h"

/* Pushes a two-window menu level: appends two window records, points the new
 * menu-stack record at both, resets both scroll states and records the
 * current location (+1) for the level.
 *
 * The level record is re-read into its own local after the script call; reusing
 * the window-level depth local changes the register allocation. */
void wldcore_menu_push_root_level(void) {
    wldcore_window_xy_t* xy;
    wldcore_window_record_t* records;
    s32 window_index;
    s32 depth;
    s32 level;

    window_index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);

    /* Taken from the record array: naming g_wldcore_window_record_positions here changes the
     * target's address form. */
    xy = (wldcore_window_xy_t*)&g_wldcore_window_records[0].x;
    records = g_wldcore_window_records;

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].window_pair.first_window = window_index;
    g_wldcore_window_records[window_index].sequence = 0;
    g_wldcore_window_records[window_index].priority = 12;
    xy[window_index].x = 0;
    xy[window_index].y = 0;
    records[window_index].anim_counter = 0;
    g_wldcore_window_records[window_index].frame_index = 0;

    window_index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_context_value_display_mode = 1;
    g_wldcore_menu_stack_records_next[depth].window_pair.second_window = window_index;
    g_wldcore_window_records[window_index].sequence = 13;
    g_wldcore_window_records[window_index].priority = 12;
    g_wldcore_window_records[window_index].flags |= 0x10;
    xy[window_index].x = 0;
    xy[window_index].y = 0;
    records[window_index].anim_counter = 0;
    g_wldcore_window_records[window_index].frame_index = 0;

    g_wldcore_map_cursor_scroll_y.limit = 0x400;
    g_wldcore_map_cursor_scroll_x.limit = 0x400;
    g_wldcore_map_cursor_scroll_y.value = 0;
    g_wldcore_map_cursor_scroll_x.value = 0;
    g_wldcore_map_cursor_scroll_y.output = 0;
    g_wldcore_map_cursor_scroll_x.output = 0;
    g_wldcore_map_cursor_scroll_y.decay = 0x80;
    g_wldcore_map_cursor_scroll_x.decay = 0x80;
    g_wldcore_map_cursor_scroll_y.step = 0x80;
    g_wldcore_map_cursor_scroll_x.step = 0x80;

    g_wldcore_displayed_numeric_value = world_script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
    level = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[level].map_cursor.wide_view = 0;
    g_wldcore_menu_stack_records_next[level].map_cursor.snap_hit = g_wldcore_map_projection_state.marker.kind + 1;
    g_wldcore_menu_stack_records_next[level].map_cursor.pending_mode = 0;
    g_wldcore_menu_stack_depth = level + 1;
    g_wldcore_menu_stack_types[level + 1] = WLDCORE_MENU_LEVEL_ROOT;
}
