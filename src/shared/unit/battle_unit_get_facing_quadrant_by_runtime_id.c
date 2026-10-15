#include "fft/battle_unit.h"

/* The unit's facing in quarter turns (0..3), or -1 when no record has the runtime
 * id. */
s16 battle_unit_get_facing_quadrant_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);
    if (unit != 0) {
        return (unit->facing / BATTLE_FACING_QUARTER_TURN) % 4;
    }
    return -1;
}
