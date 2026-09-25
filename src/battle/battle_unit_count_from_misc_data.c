#include "fft/battle.h"
#include "psx/types.h"
s32 battle_unit_count_from_misc_data(void) {
    s32 count = 0;
    battle_unit_misc_data_t* unit = g_battle_unit_last_misc_data;
    if (unit != 0) {
        do {
            unit = unit->previous;
            count++;
        } while (unit != 0);
    }
    return count;
}
