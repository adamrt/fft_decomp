#include "fft/wldcore.h"
#include "psx/types.h"

/*
 * Position the scroll-thumb window between the up and down arrow windows and
 * hide the arrows that are not usable at the current scroll position.
 */
void wldcore_text_update_scroll_indicators(wldcore_text_scrollable_window_t* state) {
    s32 top;
    s32 bottom;

    if (state->page_start_row == 0) {
        g_wldcore_window_records[state->middle_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[state->right_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[state->lower_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
        return;
    }

    top = g_wldcore_window_records[state->right_window_index].position.y;
    bottom = g_wldcore_window_records[state->middle_window_index].position.y;
    g_wldcore_window_records[state->lower_window_index].position.y
        = bottom + (top - bottom - 0x12) * state->scroll_row / state->page_start_row + 0x12;
    g_wldcore_window_records[state->lower_window_index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    if (state->scroll_row != 0) {
        g_wldcore_window_records[state->middle_window_index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    } else {
        g_wldcore_window_records[state->middle_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    }
    if (state->scroll_row != state->page_start_row) {
        g_wldcore_window_records[state->right_window_index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    } else {
        g_wldcore_window_records[state->right_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    }
}
