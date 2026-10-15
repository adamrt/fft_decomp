#include "fft/battle_unit.h"

/* The unit's camera_facing_quadrant modulo 4, or -1 when no runtime record has the
 * battle id. */
s16 battle_unit_get_camera_facing_quadrant_by_battle_id(u32 battle_id) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_battle_id(battle_id & 0xffff);
    if (unit != 0) {
        return unit->camera_facing_quadrant % 4;
    }
    return -1;
}
