#include "fft/battle.h"
#include "psx/types.h"

void battle_move_accelerate_unit_to_destination_no_height_change(void* unit) {
    battle_unit_move_toward_action_target(unit, 0);
}
