#include "fft/wldcore.h"
#include "psx/pad.h"

/*
 * Per-frame scrollable text window handler.  State 0 waits for page, cancel or
 * confirm input, state 1 lets the player scroll one row at a time while confirm
 * is held, and state 2 auto-scrolls back to the page start.
 *
 * The state 2 page-start path repeats the update call instead of jumping to
 * the shared tail: its argument copy is then a no-op against the incoming $a0,
 * so only the other paths load $a0 (in their jump delay slots), as in the
 * target.
 */
s32 wldcore_text_is_window_finished(wldcore_text_scrollable_window_t* state) {
    if (state->scroll_state == WLDCORE_TEXT_SCROLL_RETURN) {
        if (state->scroll_row == state->page_start_row) {
            state->scroll_state = WLDCORE_TEXT_SCROLL_PAGING;
            wldcore_text_update_page_prompt_visibility(state);
            return 1;
        }
        state->scroll_timer += 1;
        if (state->scroll_timer <= 0) {
            return 1;
        }
        state->scroll_timer = 0;
        state->scroll_row += 1;
        wldcore_text_render_scrollable_window_page(state, state->scroll_row);
        return 1;
    }
    if (state->scroll_state == WLDCORE_TEXT_SCROLL_MANUAL) {
        if ((g_input_current_buttons & PSX_PAD_SQUARE) == 0) {
            state->scroll_state = WLDCORE_TEXT_SCROLL_RETURN;
            state->scroll_timer = 0;
            g_wldcore_window_records[state->middle_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_records[state->right_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_records[state->lower_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
            return 1;
        }
        if ((wldcore_input_update_repeat(PSX_PAD_UP) != 0) && (state->scroll_row != 0)) {
            state->scroll_row--;
        } else if (wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
            if (state->page_start_row > state->scroll_row) {
                state->scroll_row++;
            } else {
                return 1;
            }
        } else {
            return 1;
        }
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
        wldcore_text_render_scrollable_window_page(state, state->scroll_row);
        wldcore_text_update_scroll_indicators(state);
        return 1;
    }
    if (state->scroll_state == WLDCORE_TEXT_SCROLL_PAGING) {
        if (g_wldcore_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            if (g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) {
                state->page_start_row += state->rows_per_page;
            }
            if ((g_wldcore_input_new_button_presses & PSX_PAD_CROSS)
                || (state->page_start_row > state->last_page_row)) {
                g_wldcore_window_record_count -= 4;
                g_wldcore_window_render_record_count -= 1;
                g_wldcore_window_render_object_count -= 5;
                return 0;
            }
            wldcore_sound_play_effect(MAIN_SFX_TEXT_PAGE);
            wldcore_text_render_scrollable_window_page(state, state->page_start_row);
        } else if (g_wldcore_input_new_button_presses & PSX_PAD_SQUARE) {
            state->scroll_state = WLDCORE_TEXT_SCROLL_MANUAL;
            state->scroll_row = state->page_start_row;
            wldcore_text_update_scroll_indicators(state);
            g_wldcore_window_records[state->left_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
            return 1;
        }
        wldcore_text_update_page_prompt_visibility(state);
        return 1;
    }
    return 0;
}
