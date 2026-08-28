#include "fft/battle.h"

void battle_unit_update_post_command_animation_display(battle_unit_misc_data_t* unit) {
    battle_target_move_cursor_to_unit(unit);
    battle_unit_update_display_by_misc_id(unit->unit_id);
    battle_state_enter_after_command();
}
