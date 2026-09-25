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
 *
 * The empty asm keeps the scroll timer's return block from becoming a lone
 * jump after post-reload cross-jumping; otherwise the timer test is inverted
 * to branch straight to the tail and the scroll-down return is threaded past
 * it.
 */
s32 wldcore_text_is_window_finished(wldcore_text_scrollable_window_t* state) {
    if (state->scroll_state == 2) {
        if (state->scroll_row == state->page_start_row) {
            state->scroll_state = 0;
            wldcore_window_update_flag_from_owner_progress((wldcore_window_owner_t*)state);
            return 1;
        }
        state->scroll_timer += 1;
        if (state->scroll_timer <= 0) {
        return_one_from_scroll:
            /* Emits nothing; see the function comment. */
            __asm__ volatile("");
            return 1;
        }
        state->scroll_timer = 0;
        state->scroll_row += 1;
        wldcore_text_render_scrollable_window_page(state, state->scroll_row);
        return 1;
    }
    if (state->scroll_state == 1) {
        if ((g_wldcore_current_button_input & PSX_PAD_SQUARE) == 0) {
            state->scroll_state = 2;
            state->scroll_timer = 0;
            g_wldcore_window_records[state->middle_window_index].flags |= 0x10;
            g_wldcore_window_records[state->right_window_index].flags |= 0x10;
            g_wldcore_window_records[state->lower_window_index].flags |= 0x10;
            return 1;
        }
        if ((wldcore_input_check_repeating_directional(PSX_PAD_UP) != 0) && (state->scroll_row != 0)) {
            state->scroll_row = state->scroll_row - 1;
        } else if (wldcore_input_check_repeating_directional(PSX_PAD_DOWN) != 0) {
            if (state->page_start_row <= state->scroll_row) {
                /* Shared tail: the target reuses the state 2 timer's
                 * return block, which a plain return does not reproduce. */
                goto return_one_from_scroll;
            }
            state->scroll_row = state->scroll_row + 1;
        } else {
            return 1;
        }
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
        wldcore_text_render_scrollable_window_page(state, state->scroll_row);
        wldcore_text_update_scroll_indicators(state);
        return 1;
    }
    if (state->scroll_state != 0) {
        return 0;
    }
    if (g_wldcore_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
        if (g_wldcore_new_button_presses & PSX_PAD_CIRCLE) {
            state->page_start_row += state->rows_per_page;
        }
        if ((g_wldcore_new_button_presses & PSX_PAD_CROSS) || (state->page_start_row > state->last_page_row)) {
            g_wldcore_window_record_count -= 4;
            g_wldcore_window_render_record_count -= 1;
            g_wldcore_window_render_object_count -= 5;
            return 0;
        }
        wldcore_sound_play_effect(MAIN_SFX_TEXT_PAGE);
        wldcore_text_render_scrollable_window_page(state, state->page_start_row);
    } else if (g_wldcore_new_button_presses & PSX_PAD_SQUARE) {
        state->scroll_state = 1;
        state->scroll_row = state->page_start_row;
        wldcore_text_update_scroll_indicators(state);
        g_wldcore_window_records[state->left_window_index].flags |= 0x10;
        return 1;
    }
    wldcore_window_update_flag_from_owner_progress((wldcore_window_owner_t*)state);
    return 1;
}
