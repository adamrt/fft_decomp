#include "fft/wldcore.h"

/* Scrollable text window also stepped by wldcore_menu_wait_tutorial_topic_text_then_pop_level. */

/* Steps the text window until 0x800903e4 reports it finished, then pops the
 * level, restores the parent list window's windows and frame render record,
 * and reopens the script-variable list of 0x80092208. */
void wldcore_menu_close_script_flag_01a4_detail_level(void) {
    s32 depth;

    if (!(g_wldcore_window_render_records[g_wldcore_scrollable_text_window.render_record_index].flags & 0x100)
        && wldcore_text_is_window_finished(&g_wldcore_scrollable_text_window) == 0) {
        depth = g_wldcore_menu_stack_depth;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.frame_render].flags
            |= 0x100;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.main_window].flags &= ~0x10;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.side_window].flags &= ~0x10;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.frame_render].flags
            &= ~0x10;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.upper_window].flags &= ~0x10;
        g_wldcore_menu_stack_depth = depth - 1;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.lower_window].flags &= ~0x10;
        wldcore_list_open_script_flags_01a4_01bb();
    }
}
