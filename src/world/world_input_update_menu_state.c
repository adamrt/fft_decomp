#include "fft/script.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Menu twin of world_script_update_event_input_state: clears the new-press mask,
 * yields a frame, then reads the pad and derives the new-press mask with
 * the shared initial/repeat/secondary key-repeat delays. During a tutorial
 * with `EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS` bits 0x5000 set the
 * repeat counters restart. */
void world_input_update_menu_state(void) {
    u32 input;
    s32 initial_delay;
    s32 repeat_delay;
    s32 secondary_repeat_delay;
    u32 previous_input;
    u32 counter;

    g_script_event_input = 0;
    script_load_event_input_state(&g_script_event_input);
    thread_yield();
    input = PadRead(1);
    if (input == 0
        || (script_is_tutorial_event_slot_variant() != 0
            && (script_get_variable(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS) & 0x5000))) {
        g_script_event_initial_repeat_counter = 0;
        g_script_event_repeat_counter = 0;
        g_script_event_secondary_repeat_counter = 0;
    }

    previous_input = g_script_event_current_input;
    g_script_event_current_input = input;
    g_script_event_input = 0;
    g_script_event_previous_input = previous_input;

    if (previous_input != input || (input & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_DPAD_MASK)) == 0) {
        g_script_event_input = ~previous_input & input;
        g_script_event_initial_repeat_counter = 0;
    } else {
        initial_delay = g_main_input_repeat_initial_delay / g_script_event_speed;
        counter = ++g_script_event_initial_repeat_counter;
        if (counter == initial_delay) {
            g_script_event_input = input & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_DPAD_MASK);
        }
        if (counter >= initial_delay) {
            repeat_delay = g_main_input_repeat_period / g_script_event_speed;
            counter = ++g_script_event_repeat_counter;
            g_script_event_secondary_repeat_counter++;
            if (counter >= repeat_delay) {
                g_script_event_repeat_counter = 0;
                if ((input & PSX_PAD_SQUARE) != 0) {
                    g_script_event_input = input & (PSX_PAD_RIGHT | PSX_PAD_LEFT);
                } else {
                    g_script_event_input = input & PSX_PAD_DPAD_MASK;
                }
            }
            secondary_repeat_delay = g_main_input_secondary_repeat_period / g_script_event_speed;
            if (g_script_event_secondary_repeat_counter >= secondary_repeat_delay) {
                g_script_event_secondary_repeat_counter = 0;
                if ((g_script_event_current_input & PSX_PAD_SQUARE) != 0) {
                    g_script_event_input
                        |= g_script_event_current_input & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_UP | PSX_PAD_DOWN);
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
