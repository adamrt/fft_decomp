#include "fft/battle.h"

s32 battle_effect_set_secondary_teleport_by_misc_id(u32 misc_id) {
    battle_unit_misc_data_t* unit;

    unit = battle_unit_get_misc_data_by_misc_id(misc_id & 0xffff);
    if (unit != 0) {
        battle_effect_set_secondary_teleport(unit);
        return 1;
    }
    return 0;
}
