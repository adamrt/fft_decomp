#include "fft/battle.h"

s32 battle_unit_get_battle_index_by_misc_id(u32 misc_id) {
    battle_unit_misc_data_t* unit = battle_unit_get_misc_data_by_misc_id(misc_id & 0xffff);

    if (unit != 0) {
        battle_stats_t* battle_data = unit->battle_data;

        if (battle_data != 0) {
            return battle_data->misc_unit_id;
        }
    } else {
        main_system_handle_pointer_exception(12);
    }
    return -1;
}
