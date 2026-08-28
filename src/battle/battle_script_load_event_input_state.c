#include "fft/battle.h"
#include "psx/types.h"

/* Saved copy of the event input state globals, in global order. */
typedef struct battle_event_input_state {
    s32 input;
    s32 current_input;
    s32 previous_input;
    s32 initial_repeat_counter;
    s32 repeat_counter;
    s32 secondary_repeat_counter;
} battle_event_input_state_t;

void battle_script_load_event_input_state(const battle_event_input_state_t* saved) {
    g_battle_script_event_input = saved->input;
    g_battle_script_event_current_input = saved->current_input;
    g_battle_script_event_previous_input = saved->previous_input;
    g_battle_script_event_initial_repeat_counter = saved->initial_repeat_counter;
    g_battle_script_event_repeat_counter = saved->repeat_counter;
    g_battle_script_event_secondary_repeat_counter = saved->secondary_repeat_counter;
}
