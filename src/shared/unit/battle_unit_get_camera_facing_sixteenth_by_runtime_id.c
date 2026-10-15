#include "fft/battle_unit.h"

/* The unit's camera_facing_sixteenth modulo 16, or -1 when no record has the runtime
 * id. No retail code calls it. */
s16 battle_unit_get_camera_facing_sixteenth_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);
    if (unit != 0) {
        return unit->camera_facing_sixteenth % 0x10;
    }
    return -1;
}
