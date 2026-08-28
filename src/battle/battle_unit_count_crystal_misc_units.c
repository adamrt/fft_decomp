#include "fft/battle.h"

s32 battle_unit_count_crystal_misc_units(void) {
    battle_unit_misc_data_t* unit;
    s32 count;

    count = 0;
    unit = g_battle_unit_misc_list_head;
    while (unit != 0) {
        if (unit->spritesheet_id == BATTLE_SPRITESHEET_ID_CRYSTAL) {
            count++;
        }
        unit = unit->previous;
    }
    return count;
}
