#include "fft/battle_unit.h"

/* The integer world position of the unit in slot battle_id, or 0. No retail code calls
 * it. */
SVECTOR* battle_unit_get_world_coords_by_battle_id(u32 battle_id) {
    battle_unit_t* unit = battle_unit_get_by_battle_id(battle_id & 0xffff);

    if (unit == 0) {
        return 0;
    }
    return &unit->world_coords;
}
