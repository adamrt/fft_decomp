#include "fft/battle.h"

/* Battle Stats 0x91 bit 0x04 is the Maintenance support. */
void battle_formula_apply_maintenance(void) {
    if (g_battle_action_target->support_abilities[2] & BATTLE_SUPPORT_SET_3_MAINTENANCE) {
        g_battle_action_target_data->hit = 0;
        g_battle_action_target_data->miss_type = BATTLE_ACTION_MISS_TYPE_FORCED_FAILURE;
        g_battle_action_target_data->attack_accuracy = 0;
    }
}
