#include "fft/battle.h"

void battle_effect_set_secondary(battle_unit_misc_data_t* unit, s32 effect_type) {
    battle_effect_secondary_init_t data;

    battle_effect_build_secondary_init(unit, &data);
    if (unit->battle_data != 0) {
        battle_effect_init_secondary(effect_type, unit->battle_data->sprite_palette, &data);
    }
}
