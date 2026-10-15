#include "fft/battle_unit.h"

/* SEQ script step: moves the sprite offset by delta across the unit's facing: +x
 * facing north, -x facing south, +z facing west, -z facing east. A diagonal
 * facing matches no case. */
void battle_unit_shift_right_or_left(battle_unit_t* unit, s8 delta) {
    s16* vector;
    switch (unit->facing & (BATTLE_FACING_EAST | BATTLE_FACING_NORTH)) {
    case BATTLE_FACING_NORTH:
        vector = &unit->seq_offset.vx;
        *vector = *vector + delta;
        return;
    case BATTLE_FACING_SOUTH:
        vector = &unit->seq_offset.vx;
        *vector = *vector - delta;
        return;
    case BATTLE_FACING_WEST:
        vector = &unit->seq_offset.vz;
        *vector = *vector + delta;
        return;
    case BATTLE_FACING_EAST:
        vector = &unit->seq_offset.vz;
        *vector = *vector - delta;
        return;
    }
}
