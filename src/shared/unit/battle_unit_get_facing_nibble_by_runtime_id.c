#include "fft/battle_unit.h"

/* The unit's facing in sixteenths of a turn (0..15), or -1 when no record has the
 * runtime id. */
s32 battle_unit_get_facing_nibble_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);
    if (unit != 0) {
        return (unit->facing / 0x100) % 0x10;
    }
    return -1;
}
