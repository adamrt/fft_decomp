#include "fft/battle.h"

s16 battle_unit_get_facing_quadrant_by_misc_id(u32 misc_id) {
    battle_unit_misc_data_t* unit;

    unit = battle_unit_get_misc_data_by_misc_id(misc_id & 0xffff);
    if (unit != 0) {
        return ((s16)unit->facing / 0x400) % 4;
    }
    return -1;
}
