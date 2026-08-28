#include "fft/battle.h"

void battle_state_handle_secondary_effect_state(void) {
    battle_unit_misc_data_t* unit;

    unit = battle_unit_get_casting_misc_data();
    unit->state_frame_counter++;
    battle_action_set_item_throw_stone_ability_display();
    battle_state_handle_free_cursor_input();
}
