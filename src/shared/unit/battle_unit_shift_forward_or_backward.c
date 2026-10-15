#include "fft/battle_unit.h"
#include "psx/types.h"

/* SEQ script step: moves the sprite offset by delta along the unit's facing
 * (north is +z, east is +x). A diagonal facing matches no case. */
void battle_unit_shift_forward_or_backward(battle_unit_t* unit, s8 delta) {
    switch (unit->facing & (BATTLE_FACING_EAST | BATTLE_FACING_NORTH)) {
    case BATTLE_FACING_NORTH:
        unit->seq_offset.vz += delta;
        break;
    case BATTLE_FACING_SOUTH:
        unit->seq_offset.vz -= delta;
        break;
    case BATTLE_FACING_WEST:
        unit->seq_offset.vx -= delta;
        break;
    case BATTLE_FACING_EAST:
        unit->seq_offset.vx += delta;
        break;
    }
}
