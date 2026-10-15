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
    g_wldcore_window_records[state->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
    g_wldcore_window_records[state->main_window].frame_index = g_wldcore_window_records[state->main_window].anim_counter
        = 0;
    if (state->mode == WLDCORE_LIST_WINDOW_MODE_FADE_OR_HELP) {
        g_wldcore_window_render_records[state->content_render].palette = 2;
        return;
    }
    if (state->entries[state->selected_entry] != WLDCORE_BRAVE_STORY_ROW_SET_SCRIPT_VARIABLES_03C0_03FF) {
        g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
        g_wldcore_window_render_records[state->content_render].flags |= WORLD_RENDER_RECORD_HIDDEN;
    }
}
