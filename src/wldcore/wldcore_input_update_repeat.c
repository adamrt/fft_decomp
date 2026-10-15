#include "fft/wldcore.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Return the newly pressed buttons in `buttons`, or else the first held button
 * whose auto-repeat counter fires.
 *
 * The arms test RIGHT before LEFT, unlike the counters' address order, and a
 * new press clears only the four direction counters, not R1 and L1. */
u32 wldcore_input_update_repeat(u32 buttons) {
    s32 next;
    u32 pressed;

    pressed = g_wldcore_input_new_button_presses;
    pressed &= buttons;
    if (pressed != 0) {
        g_wldcore_input_repeat_counter_right = 0;
        g_wldcore_input_repeat_counter_left = 0;
        g_wldcore_input_repeat_counter_down = 0;
        g_wldcore_input_repeat_counter_up = 0;
        return pressed;
    }

    if ((buttons & PSX_PAD_UP) != 0) {
        if ((g_input_current_buttons & PSX_PAD_UP) != 0) {
            s32* counter = &g_wldcore_input_repeat_counter_up;

            if (g_wldcore_input_repeat_initial_delay < (*counter = next = *counter + 1)) {
                *counter = next - g_wldcore_input_repeat_period;
                return PSX_PAD_UP;
            }
        } else {
            g_wldcore_input_repeat_counter_up = 0;
        }
    }

    if ((buttons & PSX_PAD_DOWN) != 0) {
        if ((g_input_current_buttons & PSX_PAD_DOWN) != 0) {
            s32* counter = &g_wldcore_input_repeat_counter_down;

            if (g_wldcore_input_repeat_initial_delay < (*counter = next = *counter + 1)) {
                *counter = next - g_wldcore_input_repeat_period;
                return PSX_PAD_DOWN;
            }
        } else {
            g_wldcore_input_repeat_counter_down = 0;
        }
    }

    if ((buttons & PSX_PAD_RIGHT) != 0) {
        if ((g_input_current_buttons & PSX_PAD_RIGHT) != 0) {
            s32* counter = &g_wldcore_input_repeat_counter_right;

            if (g_wldcore_input_repeat_initial_delay < (*counter = next = *counter + 1)) {
                *counter = next - g_wldcore_input_repeat_period;
                return PSX_PAD_RIGHT;
            }
        } else {
            g_wldcore_input_repeat_counter_right = 0;
        }
    }

    if ((buttons & PSX_PAD_LEFT) != 0) {
        if ((g_input_current_buttons & PSX_PAD_LEFT) != 0) {
            s32* counter = &g_wldcore_input_repeat_counter_left;

            if (g_wldcore_input_repeat_initial_delay < (*counter = next = *counter + 1)) {
                *counter = next - g_wldcore_input_repeat_period;
                return PSX_PAD_LEFT;
            }
        } else {
            g_wldcore_input_repeat_counter_left = 0;
        }
    }

    if ((buttons & PSX_PAD_R1) != 0) {
        if ((g_input_current_buttons & PSX_PAD_R1) != 0) {
            s32* counter = &g_wldcore_input_repeat_counter_r1;

            if (g_wldcore_input_repeat_initial_delay < (*counter = next = *counter + 1)) {
                *counter = next - g_wldcore_input_secondary_repeat_period;
                return PSX_PAD_R1;
            }
        } else {
            g_wldcore_input_repeat_counter_r1 = 0;
        }
    }

    if ((buttons & PSX_PAD_L1) != 0) {
        if ((g_input_current_buttons & PSX_PAD_L1) != 0) {
            s32* counter = &g_wldcore_input_repeat_counter_l1;
            s32 old_counter;
            s32 last_next;

            old_counter = *counter;
            if (g_wldcore_input_repeat_initial_delay < (*counter = last_next = old_counter + 1)) {
                old_counter = last_next - g_wldcore_input_secondary_repeat_period;
                *counter = old_counter;
                return PSX_PAD_L1;
            }
        } else {
            g_wldcore_input_repeat_counter_l1 = 0;
        }
    }

    return 0;
}
