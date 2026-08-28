#include "fft/battle.h"

battle_unit_misc_data_t* battle_unit_get_casting_misc_data(void) {
    battle_unit_misc_data_t* unit;
    s32 casting_unit_id;

    unit = g_battle_unit_misc_list_head;
    if (unit != 0) {
        casting_unit_id = g_battle_casting_unit_id;
        do {
            if (unit->unit_id == casting_unit_id) {
                return unit;
            }
            unit = unit->previous;
        } while (unit != 0);
    }
    return 0;
}
