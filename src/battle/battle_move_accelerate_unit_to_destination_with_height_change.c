#include "fft/battle.h"
#include "psx/types.h"

/* The flag also advances height toward the destination. */
/* Target 0x80089da0. */
void battle_move_accelerate_unit_to_destination_with_height_change(void* unit) {
    battle_unit_move_toward_action_target(unit, 1);
}
