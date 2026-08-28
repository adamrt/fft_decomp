#include "fft/battle.h"

s32 battle_unit_get_facing_nibble_by_misc_id(u32 misc_id) {
    battle_unit_misc_data_t* unit;

    unit = battle_unit_get_misc_data_by_misc_id(misc_id & 0xffff);
    if (unit != 0) {
        return ((s16)unit->facing / 0x100) % 0x10;
    }
    return -1;
}
