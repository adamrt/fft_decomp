#include "fft/battle.h"

s32 battle_gfx_reset_unit_graphic_trigger(u32 misc_id) {
    battle_unit_misc_data_t* unit = battle_unit_get_misc_data_by_misc_id(misc_id & 0xffff);

    if (unit != 0) {
        unit->ability_in_use = 0;
        unit->sprite_graphic_trigger = 0;
        return 1;
    }
    return 0;
}
