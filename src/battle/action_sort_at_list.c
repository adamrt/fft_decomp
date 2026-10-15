#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_status.h"
#include "psx/types.h"

/* Insert an entry into the 40-entry AT list ahead of the first one with a
 * larger key; the last entry drops off. Mode 0 is a unit's turn (the battle ID).
 * Mode 1 is a charged ability landing: battle ID + 0x40 with the ability id's
 * low byte in action_id_low, battle ID - 0x40 for an ability id of 0x100 or more, or
 * battle ID + 0x60 for Jump. Returns 1 when the entry was placed. */
s32 action_sort_at_list(s32 battle_id, s32 mode, s32 sort_key, battle_at_entry_t* list) {
    unit_stats_t* unit;
    s32 i;
    s32 j;
    s32 unit_code;
    s32 ability_id;
    s32 ability_code;
    s32 cmp;
    u16 key = sort_key;

    unit = &g_battle_unit_stats[battle_id];
    for (i = 0; i < BATTLE_AT_LIST_CAPACITY; i++) {
        cmp = key < list[i].sort_key;
        ability_code = battle_id + BATTLE_AT_ACTION_ENTRY;
        if (cmp) {
            for (j = BATTLE_AT_LIST_CAPACITY - 1; j > i; j--) {
                list[j] = list[j - 1];
            }
            unit_code = battle_id;
            list[i].sort_key = key;
            if (mode != 0) {
                unit_code = ability_code;
                if (unit->action_command.skillset_id == SKILLSET_ID_JUMP) {
                    unit_code = battle_id + (BATTLE_AT_ACTION_ENTRY | BATTLE_AT_JUMP);
                } else {
                    ability_id = unit->action_command.ability_id;
                    list[i].action_id_low = ability_id;
                    if (ability_id >= 0x100) {
                        unit_code = battle_id - BATTLE_AT_ACTION_ENTRY;
                    }
                }
            }
            list[i].unit_code = unit_code;
            break;
        }
    }
    return i != BATTLE_AT_LIST_CAPACITY;
}
