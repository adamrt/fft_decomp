#include "fft/battle_event.h"
#include "fft/battle_script.h"
#include "fft/main.h"
#include "fft/script.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Latch this frame's pad state for the event and menu code and derive the
 * pressed-button mask with key repeat.
 *
 * g_script_event_current_input and _previous_input hold the held buttons.
 * g_script_event_input receives the buttons that count as pressed this
 * frame: the newly pressed ones whenever the pad state changes or no repeatable
 * button (d-pad, L1, R1) is held. A state held unchanged counts frames: at the
 * initial delay the repeatable buttons fire once, and from then on the d-pad
 * fires every repeat period and L1/R1 every secondary period. While Square is
 * held the d-pad repeat is limited to Left/Right, and Up/Down fire with L1/R1 on
 * the secondary period. The three delays (g_main_input_repeat_*) are divided by
 * the event speed.
 *
 * No input at all, or a tutorial event that has Up or Down disabled, resets the
 * three repeat counters. world_script_update_event_input_state is the WORLD copy;
 * it takes no argument and fetches the buttons itself. */
void script_update_event_input_state_variant(u32 input) {
    u32 filtered_input;
    s32 initial_delay;
    s32 repeat_delay;
    s32 secondary_repeat_delay;
    u32 previous_input;
    u32 counter;

    if (input == 0
        || (script_is_tutorial_event_slot_variant() != 0
            && (script_get_variable(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS) & (PSX_PAD_UP | PSX_PAD_DOWN)) != 0)) {
        g_script_event_initial_repeat_counter = 0;
        g_script_event_repeat_counter = 0;
        g_script_event_secondary_repeat_counter = 0;
    }

    previous_input = g_script_event_current_input;
    g_script_event_current_input = input;
    g_script_event_input = 0;
    g_script_event_previous_input = previous_input;
    filtered_input = input & (PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT | PSX_PAD_L1 | PSX_PAD_R1);

    if (previous_input != input || filtered_input == 0) {
        g_script_event_input = ~previous_input & input;
        g_script_event_initial_repeat_counter = 0;
    } else {
        initial_delay = g_main_input_repeat_initial_delay / g_script_event_speed;
        counter = ++g_script_event_initial_repeat_counter;
        if (counter == initial_delay) {
            g_script_event_input = filtered_input;
        }
        if (counter >= initial_delay) {
            counter = ++g_script_event_repeat_counter;
            g_script_event_secondary_repeat_counter++;
            repeat_delay = g_main_input_repeat_period / g_script_event_speed;
            if (counter >= repeat_delay) {
                g_script_event_repeat_counter = 0;
                if ((input & PSX_PAD_SQUARE) != 0) {
                    g_script_event_input = input & (PSX_PAD_RIGHT | PSX_PAD_LEFT);
                } else {
                    g_script_event_input = input & (PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT);
                }
            }
            secondary_repeat_delay = g_main_input_secondary_repeat_period;
            {
                s32 event_speed;
                event_speed = g_script_event_speed;
                secondary_repeat_delay /= event_speed;
            }
            if (g_script_event_secondary_repeat_counter >= secondary_repeat_delay) {
                g_script_event_secondary_repeat_counter = 0;
                if ((g_script_event_current_input & PSX_PAD_SQUARE) != 0) {
                    g_script_event_input
                        |= g_script_event_current_input & (PSX_PAD_UP | PSX_PAD_DOWN | PSX_PAD_L1 | PSX_PAD_R1);
                } else {
                    g_script_event_input |= g_script_event_current_input & (PSX_PAD_L1 | PSX_PAD_R1);
                }
            }
        } else {
            g_script_event_repeat_counter = 0;
            g_script_event_secondary_repeat_counter = 0;
        }
    }
}
