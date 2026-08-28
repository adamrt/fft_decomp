#include "fft/battle.h"

battle_unit_misc_data_t* battle_unit_get_source_misc_data(void) {
    battle_unit_misc_data_t* unit;
    s32 source_id;

    unit = g_battle_unit_misc_list_head;
    if (unit != 0) {
        source_id = g_source_unit_misc_id;
        do {
            if (unit->unit_id == source_id) {
                return unit;
            }
            unit = unit->previous;
        } while (unit != 0);
    }
    return 0;
}
