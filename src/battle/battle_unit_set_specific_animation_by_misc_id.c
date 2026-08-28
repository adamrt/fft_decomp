#include "fft/battle.h"

void battle_unit_set_specific_animation_by_misc_id(u32 misc_id, u32 animation) {
    battle_unit_misc_data_t* unit = battle_unit_get_misc_data_by_misc_id(misc_id & 0xffff);
    if (unit != 0)
        battle_unit_store_animation_facing(animation, (s16)unit->facing, unit);
}
