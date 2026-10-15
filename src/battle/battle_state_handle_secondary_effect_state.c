#include "fft/battle.h"
#include "psx/types.h"

/* SecondaryEffect game-state handler: count the frame and enter the
 * action-execute state (action_set_item_throw_stone_ability_display). */
void battle_state_handle_secondary_effect_state(void) {
    battle_unit_t* unit;

    unit = battle_unit_get_casting();
    unit->state_frame_counter++;
    action_set_item_throw_stone_ability_display();
    battle_state_handle_free_cursor_input();
}
