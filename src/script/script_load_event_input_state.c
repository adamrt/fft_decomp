#include "fft/script.h"

/* Copy a saved input state of six 32-bit values (pressed, held and previous buttons and the
 * three repeat counters) into the event input globals. The BUNIT and CARD
 * overlays call it after computing the state themselves. */
void script_load_event_input_state(const menu_input_state_t* saved) {
    g_script_event_input = saved->new_buttons;
    g_script_event_current_input = saved->current_buttons;
    g_script_event_previous_input = saved->previous_buttons;
    g_script_event_initial_repeat_counter = saved->initial_repeat_counter;
    g_script_event_repeat_counter = saved->repeat_counter;
    g_script_event_secondary_repeat_counter = saved->secondary_repeat_counter;
}
