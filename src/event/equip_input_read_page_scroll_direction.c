#include "fft/battle.h"
#include "fft/equip.h"
#include "fft/main_runtime.h"
#include "psx/pad.h"

enum {
    EQUIP_SCROLL_REPEAT_WAIT = 2,
};

/* Read square-modified vertical input with the configured repeat timing. */
s32 equip_input_read_page_scroll_direction(void) {
    s32 direction;
    s32 input;
    s32 initial_delay;
    s32 repeat_period;
    u16 held_frames;

    direction = 0;
    if (g_equip_input_page_scroll_disabled == 0 && g_event_mode == 0) {
        input = PadRead(0);
        if ((input & PSX_PAD_UP) && (input & PSX_PAD_SQUARE)) {
            direction = -1;
            g_equip_input_page_scroll_hold_frames = g_equip_input_page_scroll_hold_frames + 1;
        } else if ((input & PSX_PAD_DOWN) && (input & PSX_PAD_SQUARE)) {
            direction = 1;
            g_equip_input_page_scroll_hold_frames = g_equip_input_page_scroll_hold_frames + 1;
        } else {
            g_equip_input_page_scroll_hold_frames = 0;
        }
        /* One variable holds the input and then the speed, as in the target. */
        input = g_equip_menu_event_speed;
        initial_delay = g_main_input_repeat_initial_delay / input;
        repeat_period = g_main_input_secondary_repeat_period / input;
        held_frames = g_equip_input_page_scroll_hold_frames;
        if ((held_frames < initial_delay) || ((held_frames % repeat_period) == 0)) {
            if ((held_frames >= 2) && (held_frames < initial_delay)) {
                return EQUIP_SCROLL_REPEAT_WAIT;
            }
        } else {
            return EQUIP_SCROLL_REPEAT_WAIT;
        }
    }
    return direction;
}
