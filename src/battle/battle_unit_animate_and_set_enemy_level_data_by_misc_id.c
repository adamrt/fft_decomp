#include "fft/battle.h"
#include "psx/types.h"

s32 battle_unit_animate_and_set_enemy_level_data_by_misc_id(u32 misc_id) {
    battle_unit_misc_data_t* unit;
    battle_stats_t* battle_data;

    unit = battle_unit_get_misc_data_by_misc_id(misc_id & 0xffff);
    if (unit != 0) {
        battle_data = unit->battle_data;
        unit->ability_in_use = 1;
        unit->sprite_graphic_trigger = 1;
        if (battle_data != 0) {
            battle_unit_set_enemy_level_data_by_battle_id(battle_data->misc_unit_id);
        }
        return 1;
    }
    return 0;
}
