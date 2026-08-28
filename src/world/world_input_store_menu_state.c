#include "fft/world.h"

/* Copy a six-word WORLD menu-controller state snapshot. */
void world_input_store_menu_state(const world_menu_input_state_t* state) {
    g_world_menu_new_button_input = state->new_buttons;
    g_world_menu_current_button_input = state->current_buttons;
    g_world_menu_previous_button_input = state->previous_buttons;
    g_world_menu_input_repeat_counter_0 = state->repeat_counter_0;
    g_world_menu_input_repeat_counter_1 = state->repeat_counter_1;
    g_world_menu_input_repeat_counter_2 = state->repeat_counter_2;
}
