/* Scroll axis speed, input repeat and render object visibility. */

#include "fft/wldcore.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Advance one axis toward its signed limit, or decay it back to rest.
 *
 * The negative side mirrors the positive one through negation rather than a
 * signed division, so the exported speed truncates toward zero on both sides. */
void wldcore_map_update_scroll_axis_speed(wldcore_axis_state_t* axis, s32 direction) {
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

void wldcore_input_init_repeat_state(void) {
    g_wldcore_input_repeat_counter_l1 = 0;
    g_wldcore_input_repeat_counter_r1 = 0;
    g_wldcore_input_repeat_counter_right = 0;
    g_wldcore_input_repeat_counter_left = 0;
    g_wldcore_input_repeat_counter_down = 0;
    g_wldcore_input_repeat_counter_up = 0;
    g_wldcore_input_repeat_initial_delay = g_main_input_repeat_initial_delay;
    g_wldcore_input_repeat_period = g_main_input_repeat_period;
    g_wldcore_input_secondary_repeat_period = g_main_input_secondary_repeat_period;
}

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

/* Each queued record starts with the shared 32-bit display flags. */
static inline s32* wldcore_display_object_flags(void* object) {
    return object;
}

/* Clear (nonzero argument) or set the blank flag on both display-object queues. */
void wldcore_window_set_render_objects_visible(s32 clear) {
    s32 i;

    if (clear) {
        for (i = 0; i < g_wldcore_window_aux_render_object_count; i++) {
            *wldcore_display_object_flags(g_wldcore_window_aux_render_object_queue[i]) &= ~WORLD_RENDER_RECORD_BLANK;
        }
        for (i = 0; i < g_wldcore_window_render_object_count; i++) {
            *wldcore_display_object_flags(g_wldcore_window_render_object_queue[i]) &= ~WORLD_RENDER_RECORD_BLANK;
        }
    } else {
        for (i = 0; i < g_wldcore_window_aux_render_object_count; i++) {
            *wldcore_display_object_flags(g_wldcore_window_aux_render_object_queue[i]) |= WORLD_RENDER_RECORD_BLANK;
        }
        for (i = 0; i < g_wldcore_window_render_object_count; i++) {
            *wldcore_display_object_flags(g_wldcore_window_render_object_queue[i]) |= WORLD_RENDER_RECORD_BLANK;
        }
    }
}
