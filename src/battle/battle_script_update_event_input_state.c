#include "fft/battle.h"
#include "psx/pad.h"
#include "psx/types.h"

void battle_script_update_event_input_state(u32 input) {
    u32 filtered_input;
    s32 initial_delay;
    s32 repeat_delay;
    s32 secondary_repeat_delay;
    u32 previous_input;
    u32 counter;

    if (input == 0
        || (battle_script_is_tutorial_event_slot() != 0
            && (battle_script_get_variable(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS) & (PSX_PAD_UP | PSX_PAD_DOWN))
                != 0)) {
        g_battle_script_event_initial_repeat_counter = 0;
        g_battle_script_event_repeat_counter = 0;
        g_battle_script_event_secondary_repeat_counter = 0;
    }

    previous_input = g_battle_script_event_current_input;
    g_battle_script_event_current_input = input;
    g_battle_script_event_input = 0;
    g_battle_script_event_previous_input = previous_input;
    filtered_input = input & (PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT | PSX_PAD_L1 | PSX_PAD_R1);

    if (previous_input != input || filtered_input == 0) {
        g_battle_script_event_input = ~previous_input & input;
        g_battle_script_event_initial_repeat_counter = 0;
    } else {
        initial_delay = g_main_input_repeat_initial_delay / g_battle_event_speed;
        counter = ++g_battle_script_event_initial_repeat_counter;
        if (counter == initial_delay) {
            g_battle_script_event_input = filtered_input;
        }
        if (counter >= initial_delay) {
            counter = ++g_battle_script_event_repeat_counter;
            g_battle_script_event_secondary_repeat_counter++;
            repeat_delay = g_main_input_repeat_period / g_battle_event_speed;
            if (counter >= repeat_delay) {
                g_battle_script_event_repeat_counter = 0;
                if ((input & PSX_PAD_SQUARE) != 0) {
                    g_battle_script_event_input = input & (PSX_PAD_RIGHT | PSX_PAD_LEFT);
                } else {
                    g_battle_script_event_input = input & (PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT);
                }
            }
            secondary_repeat_delay = g_main_input_secondary_repeat_period;
            {
                s32 event_speed;
                event_speed = g_battle_event_speed;
                secondary_repeat_delay /= event_speed;
            }
            if (g_battle_script_event_secondary_repeat_counter >= secondary_repeat_delay) {
                g_battle_script_event_secondary_repeat_counter = 0;
                if ((g_battle_script_event_current_input & PSX_PAD_SQUARE) != 0) {
                    g_battle_script_event_input
                        |= g_battle_script_event_current_input & (PSX_PAD_UP | PSX_PAD_DOWN | PSX_PAD_L1 | PSX_PAD_R1);
                } else {
                    g_battle_script_event_input |= g_battle_script_event_current_input & (PSX_PAD_L1 | PSX_PAD_R1);
                }
            }
        } else {
            g_battle_script_event_repeat_counter = 0;
            g_battle_script_event_secondary_repeat_counter = 0;
        }
    }
}
