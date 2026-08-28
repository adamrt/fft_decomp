#include "fft/event.h"
#include "fft/jobstts.h"
#include "fft/main_runtime.h"
#include "psx/pad.h"

/*
 * Returns a repeated page-scroll direction while Square and Up/Down are held.
 *
 * The first held frame is accepted immediately. Further input is suppressed
 * until the scaled initial delay, then accepted at the scaled repeat period.
 */
jobstts_menu_page_scroll_direction_e jobstts_input_read_page_scroll_direction(void) {
    jobstts_menu_page_scroll_direction_e page_direction;
    s32 input;
    s32 initial_delay;
    s32 repeat_period;
    u16 held_frames;

    page_direction = JOBSTTS_PAGE_SCROLL_NONE;
    input = PadRead(0);
    if ((input & PSX_PAD_UP) && (input & PSX_PAD_SQUARE)) {
        page_direction = JOBSTTS_PAGE_SCROLL_UP;
        g_jobstts_input_page_scroll_hold_frames = g_jobstts_input_page_scroll_hold_frames + 1;
    } else if ((input & PSX_PAD_DOWN) && (input & PSX_PAD_SQUARE)) {
        page_direction = JOBSTTS_PAGE_SCROLL_DOWN;
        g_jobstts_input_page_scroll_hold_frames = g_jobstts_input_page_scroll_hold_frames + 1;
    } else {
        g_jobstts_input_page_scroll_hold_frames = 0;
    }
    /* One variable holds the input and then the speed, as in the target. */
    input = g_jobstts_menu_event_speed;
    initial_delay = g_main_input_repeat_initial_delay / input;
    repeat_period = g_main_input_secondary_repeat_period / input;
    held_frames = g_jobstts_input_page_scroll_hold_frames;
    if ((held_frames >= initial_delay && (held_frames % repeat_period) != 0)
        || (held_frames >= 2 && held_frames < initial_delay)) {
        page_direction = JOBSTTS_PAGE_SCROLL_NONE;
    }
    return page_direction;
}
