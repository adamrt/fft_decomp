#include "fft/event_bunit.h"
#include "psx/pad.h"

/*
 * Read Square-modified vertical input with the configured repeat timing.
 *
 * WAIT suppresses intermediate held frames without treating them as a direction.
 */
bunit_menu_vertical_scroll_direction_e bunit_input_read_page_scroll_direction(void) {
    bunit_menu_vertical_scroll_direction_e direction;
    s32 input;
    s32 initial_delay;
    s32 repeat_period;
    u16 held_frames;

    direction = BUNIT_VERTICAL_SCROLL_NONE;
    if (g_bunit_input_page_scroll_disabled == 0) {
        if (g_event_mode == 0) {
            input = PadRead(0);
            if ((input & PSX_PAD_UP) && (input & PSX_PAD_SQUARE)) {
                direction = BUNIT_VERTICAL_SCROLL_UP;
                g_bunit_input_page_scroll_hold_frames = g_bunit_input_page_scroll_hold_frames + 1;
            } else if ((input & PSX_PAD_DOWN) && (input & PSX_PAD_SQUARE)) {
                direction = BUNIT_VERTICAL_SCROLL_DOWN;
                g_bunit_input_page_scroll_hold_frames = g_bunit_input_page_scroll_hold_frames + 1;
            } else {
                g_bunit_input_page_scroll_hold_frames = 0;
            }
            /* One variable holds the input and then the speed, as in the target. */
            input = bunit_menu_get_event_speed();
            initial_delay = g_main_input_repeat_initial_delay / input;
            repeat_period = g_main_input_secondary_repeat_period / input;
            held_frames = g_bunit_input_page_scroll_hold_frames;
            if ((held_frames < initial_delay) || ((held_frames % repeat_period) == 0)) {
                if ((held_frames >= 2) && (held_frames < initial_delay)) {
                    return BUNIT_VERTICAL_SCROLL_WAIT;
                }
            } else {
                return BUNIT_VERTICAL_SCROLL_WAIT;
            }
        }
    }
    return direction;
}
