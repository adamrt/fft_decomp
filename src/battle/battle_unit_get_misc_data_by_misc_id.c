#include "fft/battle.h"

battle_unit_misc_data_t* battle_unit_get_misc_data_by_misc_id(u32 misc_id) {
    battle_unit_misc_data_t* unit;

    unit = g_battle_unit_misc_list_head;
    while (unit != 0) {
        if (unit->unit_id == (misc_id & 0xFFFF)) {
            return unit;
        }
        unit = unit->previous;
    }
    return 0;
}
