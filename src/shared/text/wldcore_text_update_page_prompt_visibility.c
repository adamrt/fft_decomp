#include "fft/wldcore.h"

/* Show the text-page prompt while more pages remain. */
void wldcore_text_update_page_prompt_visibility(wldcore_text_scrollable_window_t* state) {
    if (state->page_start_row < state->last_page_row) {
        g_wldcore_window_records[state->left_window_index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    } else {
        g_wldcore_window_records[state->left_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    }
}
