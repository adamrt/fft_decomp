/* Target list sort, store and copy with the acting-unit store. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_target.h"
#include "psx/types.h"

void target_sort_list(s32 target_battle_id) {
    s32 i;

    for (i = 0; i < BATTLE_UNIT_RUNTIME_SLOT_COUNT; i++) {
        if (g_target_ability_targets_list[i] == (target_battle_id & 0xFF)) {
            for (; i < BATTLE_UNIT_RUNTIME_SLOT_COUNT - 1; i++) {
                g_target_ability_targets_list[i] = g_target_ability_targets_list[i + 1];
            }
            g_target_ability_targets_list[BATTLE_UNIT_RUNTIME_SLOT_COUNT - 1] = 0xFF;
            return;
        }
    }
}

/* Copy 16 target ids into the current ability's target list. */
void action_store_target_ids(u8* src) {
    s32 i;

    for (i = 0; i < BATTLE_UNIT_RUNTIME_SLOT_COUNT; i++) {
        g_target_ability_targets_list[i] = src[i];
    }
}

/* Copy the current ability's 16-entry target list to `out` and return how many
 * entries hold a unit (anything but 0xff). */
s32 target_copy_ability_targets(u8* out) {
    s32 count = 0;
    s32 i;

    for (i = 0; i < BATTLE_UNIT_RUNTIME_SLOT_COUNT; i++) {
        u8 target_id = g_target_ability_targets_list[i];
        *out++ = target_id;
        if (target_id != 0xFF) {
            count++;
        }
    }
    return count;
}

/* Store value in D_8019387C when unit_id is the acting unit of a primary
 * (non-reaction) action. */
void action_store_8019387c_if_not_reacting(s32 unit_id, s8 value) {
    if (g_action_context == ACTION_CONTEXT_PRIMARY && unit_id == g_battle_acting_unit_id) {
        g_battle_unknown_8019387c = value;
    }
}
