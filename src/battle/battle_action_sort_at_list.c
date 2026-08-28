#include "fft/battle.h"
#include "fft/data.h"
#include "psx/types.h"

s32 battle_action_sort_at_list(s32 unit_id, s32 mode, u16 sort_key, battle_at_entry_t* list) {
    battle_stats_t* unit;
    s32 i;
    s32 j;
    s32 unit_code;
    s32 ability_id;
    s32 ability_code;
    s32 cmp;

    unit = &g_battle_unit_stats[unit_id];
    for (i = 0; i < 0x28; i++) {
        cmp = sort_key < list[i].sort_key;
        ability_code = unit_id + 0x40;
        if (cmp) {
            for (j = 0x27; j > i; j--) {
                list[j] = list[j - 1];
            }
            unit_code = unit_id;
            list[i].sort_key = sort_key;
            if (mode != 0) {
                unit_code = ability_code;
                if (unit->last_skillset_id == SKILLSET_ID_JUMP) {
                    unit_code = unit_id + 0x60;
                } else {
                    ability_id = unit->last_ability_id;
                    list[i].flags = ability_id;
                    if (ability_id >= 0x100) {
                        unit_code = unit_id - 0x40;
                    }
                }
            }
            list[i].unit = unit_code;
            break;
        }
    }
    return i != 0x28;
}
