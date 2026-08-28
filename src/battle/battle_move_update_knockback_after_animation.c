#include "fft/battle_move.h"
#include "psx/types.h"

/* Runs the knockback step unless the unit is mid-way through animation 0x20. */
void battle_move_update_knockback_after_animation(battle_unit_misc_data_t* unit) {
    if (((unit->encoded_animation >> 1) != 0x20) || (unit->animation_countdown == 0)) {
        battle_move_advance_display_unit_step(unit);
    }
}
