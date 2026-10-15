#include "fft/battle.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"

/* Ends a unit's command: the cursor returns to the unit, its display is updated
 * and the game enters the after-command state. */
void battle_unit_update_post_command_animation_display(battle_unit_t* unit) {
    target_move_cursor_to_unit(unit);
    battle_unit_update_display_by_runtime_id(unit->runtime_id);
    battle_state_enter_after_command();
}
