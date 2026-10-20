/* World-map pad polling, accelerated map scrolling and menu auto-repeat.
 * The frame poll publishes held/new buttons; menu handlers advance the private
 * direction and shoulder-button repeat counters using MAIN's timing settings. */

#include "fft/input.h"
#include "fft/suzuki.h"
#include "fft/system.h"
#include "fft/world_map.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Repeat timing and the six button counters are reset and advanced together. */
typedef struct world_map_input_repeat {
    s32 initial_delay;
    s32 period;
    s32 secondary_period;
    s32 counter_up;
    s32 counter_down;
    s32 counter_left;
    s32 counter_right;
    s32 counter_r1;
    s32 counter_l1;
} world_map_input_repeat_t;
typedef char world_map_input_repeat_size_must_be_36[sizeof(world_map_input_repeat_t) == 36 ? 1 : -1];

static world_map_input_repeat_t g_world_map_input_repeat = { 0 };

/* WLDCORE's per-frame pad poll: reads pad 0 and publishes the held buttons
 * and the newly pressed ones (both 0 while SYSTEM_FLAG_INPUT_BLOCKED is
 * set). Exactly L1 + R1 + Select + Start restores the sound volumes and
 * resets the game. */
void world_map_pad_read(void) {
    u32 input;
    u32 previous;

    g_input_previous_buttons = g_input_polled_buttons;
    input = PadRead(0);
    g_input_polled_buttons = input;

    if (g_system_flags & SYSTEM_FLAG_INPUT_BLOCKED) {
        g_world_map_input_new_button_presses = 0;
        g_input_current_buttons = 0;
    } else {
        previous = g_input_previous_buttons;
        g_input_current_buttons = input;
        g_world_map_input_new_button_presses = input & ~previous;
    }

    if (g_input_polled_buttons == (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_SELECT | PSX_PAD_START)) {
        DrawSync(0);
        suzuki_sound_set_master_volume(0x3fff, 1);
        suzuki_sound_put_type(0xc0);
        suzuki_sound_set_cd_volume(0x74ff, 1);
        system_game_reset();
    }
}

/* Step one map scroll axis: direction 1 or -1 accelerates `value` by `step` up to +/-`limit`,
 * and direction 0 decays it toward zero by `decay`. Converting Q8 speed to whole pixels rounds
 * down for +1 and up for -1, even while reversing; decay truncates toward zero. */
void world_map_scroll_axis_update(world_map_scroll_axis_t* axis, s32 direction) {
    switch (direction) {
    case 1:
        if (axis->value < axis->limit) {
            axis->value = axis->value + axis->step;
        }
        if (axis->value > axis->limit) {
            axis->value = axis->limit;
        }
        axis->output = axis->value >> 8;
        break;
    case 0:
        if (axis->value == 0) {
            break;
        }
        if (axis->value > 0) {
            axis->value = axis->value - axis->decay;
            if (axis->value <= 0) {
                axis->value = 0;
            }
            axis->output = axis->value >> 8;
        } else {
            axis->value = axis->value + axis->decay;
            if (axis->value >= 0) {
                axis->value = 0;
            }
            axis->output = -((-axis->value) >> 8);
        }
        break;
    case -1:
        if (-axis->value < axis->limit) {
            axis->value = axis->value - axis->step;
        }
        if (-axis->value > axis->limit) {
            axis->value = -axis->limit;
        }
        axis->output = -((-axis->value) >> 8);
        break;
    }
}

/* Clear the six auto-repeat counters and copy the repeat delay and periods from MAIN's input
 * settings. */
void world_map_input_repeat_init(void) {
    g_world_map_input_repeat.counter_l1 = 0;
    g_world_map_input_repeat.counter_r1 = 0;
    g_world_map_input_repeat.counter_right = 0;
    g_world_map_input_repeat.counter_left = 0;
    g_world_map_input_repeat.counter_down = 0;
    g_world_map_input_repeat.counter_up = 0;
    g_world_map_input_repeat.initial_delay = g_input_repeat_initial_delay;
    g_world_map_input_repeat.period = g_input_repeat_period;
    g_world_map_input_repeat.secondary_period = g_input_secondary_repeat_period;
}

/* Return the newly pressed buttons in `buttons`, or else the first held button
 * whose auto-repeat counter fires.
 *
 * The arms test RIGHT before LEFT, unlike the counters' address order, and a
 * new press clears only the four direction counters, not R1 and L1. */
u32 world_map_input_repeat_update(u32 buttons) {
    s32 next;
    u32 pressed = g_world_map_input_new_button_presses;

    pressed &= buttons;
    if (pressed != 0) {
        g_world_map_input_repeat.counter_right = 0;
        g_world_map_input_repeat.counter_left = 0;
        g_world_map_input_repeat.counter_down = 0;
        g_world_map_input_repeat.counter_up = 0;
        return pressed;
    }

    if ((buttons & PSX_PAD_UP) != 0) {
        if ((g_input_current_buttons & PSX_PAD_UP) != 0) {
            s32* counter = &g_world_map_input_repeat.counter_up;

            next = *counter + 1;
            *counter = next;
            if (g_world_map_input_repeat.initial_delay < next) {
                *counter = next - g_world_map_input_repeat.period;
                return PSX_PAD_UP;
            }
        } else {
            g_world_map_input_repeat.counter_up = 0;
        }
    }

    if ((buttons & PSX_PAD_DOWN) != 0) {
        if ((g_input_current_buttons & PSX_PAD_DOWN) != 0) {
            s32* counter = &g_world_map_input_repeat.counter_down;

            next = *counter + 1;
            *counter = next;
            if (g_world_map_input_repeat.initial_delay < next) {
                *counter = next - g_world_map_input_repeat.period;
                return PSX_PAD_DOWN;
            }
        } else {
            g_world_map_input_repeat.counter_down = 0;
        }
    }

    if ((buttons & PSX_PAD_RIGHT) != 0) {
        if ((g_input_current_buttons & PSX_PAD_RIGHT) != 0) {
            s32* counter = &g_world_map_input_repeat.counter_right;

            next = *counter + 1;
            *counter = next;
            if (g_world_map_input_repeat.initial_delay < next) {
                *counter = next - g_world_map_input_repeat.period;
                return PSX_PAD_RIGHT;
            }
        } else {
            g_world_map_input_repeat.counter_right = 0;
        }
    }

    if ((buttons & PSX_PAD_LEFT) != 0) {
        if ((g_input_current_buttons & PSX_PAD_LEFT) != 0) {
            s32* counter = &g_world_map_input_repeat.counter_left;

            next = *counter + 1;
            *counter = next;
            if (g_world_map_input_repeat.initial_delay < next) {
                *counter = next - g_world_map_input_repeat.period;
                return PSX_PAD_LEFT;
            }
        } else {
            g_world_map_input_repeat.counter_left = 0;
        }
    }

    if ((buttons & PSX_PAD_R1) != 0) {
        if ((g_input_current_buttons & PSX_PAD_R1) != 0) {
            s32* counter = &g_world_map_input_repeat.counter_r1;

            next = *counter + 1;
            *counter = next;
            if (g_world_map_input_repeat.initial_delay < next) {
                *counter = next - g_world_map_input_repeat.secondary_period;
                return PSX_PAD_R1;
            }
        } else {
            g_world_map_input_repeat.counter_r1 = 0;
        }
    }

    if ((buttons & PSX_PAD_L1) != 0) {
        if ((g_input_current_buttons & PSX_PAD_L1) != 0) {
            s32* counter = &g_world_map_input_repeat.counter_l1;
            s32 old_counter;
            s32 last_next;

            /* Spelled apart from the other arms: separate old and next values
             * give this last arm the retail register assignment. */
            old_counter = *counter;
            if (g_world_map_input_repeat.initial_delay < (*counter = last_next = old_counter + 1)) {
                old_counter = last_next - g_world_map_input_repeat.secondary_period;
                *counter = old_counter;
                return PSX_PAD_L1;
            }
        } else {
            g_world_map_input_repeat.counter_l1 = 0;
        }
    }

    return 0;
}
