#include "fft/wldcore.h"

/* Closes the list window of the level beneath the current one: the main
 * window goes to palette 10 (sequence, frame_index and anim_counter cleared),
 * the side window to palette 6 and the frame render record to palette 2, each hidden with
 * flag 0x10. */
void wldcore_window_close_parent_list_frame_windows(void) {
    s32 index;

    index = g_wldcore_menu_stack_depth - 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].palette = 10;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].sequence = 0;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].flags |= 0x10;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].frame_index
        = g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].anim_counter = 0;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.side_window].palette = 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.side_window].flags |= 0x10;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[index].list_window.frame_render].palette = 2;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[index].list_window.frame_render].flags |= 0x10;
}
