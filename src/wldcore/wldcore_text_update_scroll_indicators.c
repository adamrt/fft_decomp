#include "fft/wldcore.h"
#include "psx/types.h"

/*
 * Position the scroll-thumb window between the up and down arrow windows and
 * hide the arrows that are not usable at the current scroll position.
 */
void wldcore_text_update_scroll_indicators(wldcore_text_scrollable_window_t* state) {
    wldcore_text_scrollable_window_t* state_a2 = state;
    s32 top;
    s32 bottom;
    s32 product;
    /* Pins: unpinned, page_start_row and the quotient take a3 and v1 instead of the target's shared a1. */
    register s32 quotient __asm__("$5");

    {
        register s32 page_start_row __asm__("$5") = state_a2->page_start_row;

        if (page_start_row == 0) {
            g_wldcore_window_records[state_a2->middle_window_index].flags |= 0x10;
            g_wldcore_window_records[state_a2->right_window_index].flags |= 0x10;
            g_wldcore_window_records[state_a2->lower_window_index].flags |= 0x10;
            return;
        }
    }

    top = g_wldcore_window_records[state_a2->right_window_index].y;
    bottom = g_wldcore_window_records[state_a2->middle_window_index].y;
    product = (top - bottom - 0x12) * state_a2->scroll_row;
    quotient = product / state_a2->page_start_row;
    g_wldcore_window_records[state_a2->lower_window_index].y = bottom + quotient + 0x12;
    g_wldcore_window_records[state_a2->lower_window_index].flags &= ~0x10;
    if (state_a2->scroll_row != 0) {
        g_wldcore_window_records[state_a2->middle_window_index].flags &= ~0x10;
    } else {
        g_wldcore_window_records[state_a2->middle_window_index].flags |= 0x10;
    }
    if (state_a2->scroll_row != state_a2->page_start_row) {
        g_wldcore_window_records[state_a2->right_window_index].flags &= ~0x10;
    } else {
        g_wldcore_window_records[state_a2->right_window_index].flags |= 0x10;
    }
}
