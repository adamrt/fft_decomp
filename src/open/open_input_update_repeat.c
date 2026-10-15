#include "fft/open.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Return the newly pressed buttons in `buttons`, or else Up or Down when that
 * held direction's auto-repeat fires, or 0.
 *
 * A new press clears the four direction counters. A held direction counts once
 * per call; past g_open_input_directional_repeat_initial_delay it fires and steps
 * back by g_open_input_directional_repeat_period. Unlike the WLDCORE twin only Up
 * and Down repeat: the other two counters are cleared and never counted. */
u32 open_input_update_repeat(u32 buttons) {
    s32 next;
    u32 pressed;

    pressed = g_open_input_new_button_presses;
    pressed &= buttons;
    if (pressed != 0) {
        g_open_input_right_repeat_counter = 0;
        g_open_input_left_repeat_counter = 0;
        g_open_input_down_repeat_counter = 0;
        g_open_input_up_repeat_counter = 0;
        return pressed;
    }

    if ((buttons & PSX_PAD_UP) != 0) {
        if ((g_input_current_buttons & PSX_PAD_UP) != 0) {
            s32* up_counter = &g_open_input_up_repeat_counter;

            if (g_open_input_directional_repeat_initial_delay < (*up_counter = next = *up_counter + 1)) {
                *up_counter = next - g_open_input_directional_repeat_period;
                return PSX_PAD_UP;
            }
        } else {
            g_open_input_up_repeat_counter = 0;
        }
    }

    if ((buttons & PSX_PAD_DOWN) != 0) {
        if ((g_input_current_buttons & PSX_PAD_DOWN) != 0) {
            s32* down_counter = &g_open_input_down_repeat_counter;
            s32 old_counter;
            s32 down_next;

            old_counter = *down_counter;
            if (g_open_input_directional_repeat_initial_delay < (*down_counter = down_next = old_counter + 1)) {
                old_counter = down_next - g_open_input_directional_repeat_period;
                *down_counter = old_counter;
                return PSX_PAD_DOWN;
            }
        } else {
            g_open_input_down_repeat_counter = 0;
        }
    }

    return 0;
}
