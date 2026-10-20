/* Title menus repeat Up and Down after the resident input delays; Left and
 * Right have counters but never repeat in OPEN. */

#include "fft/input.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Directional repeat timing and counters. Left and Right are cleared only. */
typedef struct input_directional_repeat {
    s32 initial_delay;
    s32 period;
    s32 counter_up;
    s32 counter_down;
    s32 counter_left;
    s32 counter_right;
} input_directional_repeat_t;
typedef char input_directional_repeat_size_must_be_24[sizeof(input_directional_repeat_t) == 24 ? 1 : -1];

extern input_directional_repeat_t g_input_directional_repeat;

/* Clear the direction repeat counters and take the repeat delay and period from
 * MAIN's input timings. */
void input_init_directional_state(void) {
    s32 first_limit = g_input_repeat_initial_delay;
    s32 second_limit = g_input_repeat_period;

    g_input_directional_repeat.counter_right = 0;
    g_input_directional_repeat.counter_left = 0;
    g_input_directional_repeat.counter_down = 0;
    g_input_directional_repeat.counter_up = 0;
    g_input_directional_repeat.initial_delay = first_limit;
    g_input_directional_repeat.period = second_limit;
}

/* Return the newly pressed buttons in `buttons`, or else Up or Down when that
 * held direction's auto-repeat fires, or 0.
 *
 * A new press clears the four direction counters. A held direction counts once
 * per call; after the initial delay it fires and subtracts the repeat period.
 * Unlike WLDCORE, only Up
 * and Down repeat: the other two counters are cleared and never counted. */
u32 input_update_repeat(u32 buttons) {
    s32 next;
    u32 pressed = g_input_new_button_presses;

    pressed &= buttons;
    if (pressed != 0) {
        g_input_directional_repeat.counter_right = 0;
        g_input_directional_repeat.counter_left = 0;
        g_input_directional_repeat.counter_down = 0;
        g_input_directional_repeat.counter_up = 0;
        return pressed;
    }

    if ((buttons & PSX_PAD_UP) != 0) {
        if ((g_input_current_buttons & PSX_PAD_UP) != 0) {
            s32* up_counter = &g_input_directional_repeat.counter_up;

            next = *up_counter + 1;
            *up_counter = next;
            if (g_input_directional_repeat.initial_delay < next) {
                *up_counter = next - g_input_directional_repeat.period;
                return PSX_PAD_UP;
            }
        } else {
            g_input_directional_repeat.counter_up = 0;
        }
    }

    if ((buttons & PSX_PAD_DOWN) != 0) {
        if ((g_input_current_buttons & PSX_PAD_DOWN) != 0) {
            s32* down_counter = &g_input_directional_repeat.counter_down;
            s32 old_counter;
            s32 down_next;

            /* Spelled apart from the Up branch: separate old and next values give
             * this branch the retail register assignment. */
            old_counter = *down_counter;
            down_next = old_counter + 1;
            *down_counter = down_next;
            if (g_input_directional_repeat.initial_delay < down_next) {
                old_counter = down_next - g_input_directional_repeat.period;
                *down_counter = old_counter;
                return PSX_PAD_DOWN;
            }
        } else {
            g_input_directional_repeat.counter_down = 0;
        }
    }

    return 0;
}
