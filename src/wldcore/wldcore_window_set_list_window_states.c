#include "fft/main_runtime.h"
#include "fft/wldcore.h"

/* Set the window palettes of a list window: main window 10 (with sequence
 * set and frame_index/anim_counter cleared), side window 6, the frame render
 * record 2 and both auxiliary windows 5. Mode 2 also sets the content render
 * palette to 2; otherwise a selected entry of kind other than 2 hides the content
 * render record (flag 0x10) and sets system flag 0x800.
 *
 * The chained store keeps one main-window index for both cleared fields. */
void wldcore_window_set_list_window_states(wldcore_menu_list_window_level_t* state) {
    g_wldcore_window_records[state->main_window].palette = 10;
    g_wldcore_window_records[state->side_window].palette = 6;
    g_wldcore_window_render_records[state->frame_render].palette = 2;
    g_wldcore_window_records[state->upper_window].palette = 5;
    g_wldcore_window_records[state->lower_window].palette = 5;
    g_wldcore_window_records[state->main_window].sequence = 1;
    g_wldcore_window_records[state->main_window].frame_index = g_wldcore_window_records[state->main_window].anim_counter
        = 0;
    if (state->mode == 2) {
        g_wldcore_window_render_records[state->content_render].palette = 2;
        return;
    }
    if (state->entries[state->selected_entry] != 2) {
        g_main_system_flags |= 0x800;
        g_wldcore_window_render_records[state->content_render].flags |= 0x10;
    }
}
