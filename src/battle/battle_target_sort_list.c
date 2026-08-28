#include "fft/battle.h"
#include "psx/types.h"

/* Remove target_battle_id from the pending-target ring (g_battle_target_ability_targets_list), shift
   the trailing entries down, and terminate the list with 0xFF. No-op if
   the id is not present. */
void battle_target_sort_list(s32 target_battle_id) {
    s32 i;

    for (i = 0; i < 16; i++) {
        if (g_battle_target_ability_targets_list[i] == (target_battle_id & 0xFF)) {
            for (; i < 15; i++) {
                g_battle_target_ability_targets_list[i] = g_battle_target_ability_targets_list[i + 1];
            }
            g_battle_target_ability_targets_list[15] = 0xFF;
            return;
        }
    }
}
