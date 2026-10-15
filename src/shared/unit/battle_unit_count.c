#include "fft/battle_unit.h"
#include "psx/types.h"
/* The number of records in the runtime unit list. No retail code calls it. */
s32 battle_unit_count(void) {
    s32 count = 0;
    battle_unit_t* unit = g_battle_unit_list_head;
    while (unit != 0) {
        unit = unit->previous;
        count++;
    }
    return count;
}
