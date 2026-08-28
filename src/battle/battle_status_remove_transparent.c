#include "fft/battle.h"
#include "psx/types.h"

s32 battle_status_remove_transparent(battle_stats_t* unit) {
    battle_stats_t* target;

    if (battle_status_check_crystal_dead_jump_petrify_treasure(unit) != 0) {
        return 0;
    }
    battle_action_set_target_variables(unit);
    if (unit->action_taken != 0) {
        target = g_battle_action_target;
        if ((target->inflicted_status[2] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TRANSPARENT)) != 0) {
            if (target->transparent_removal_flag != 0) {
                g_battle_action_target_data->status_removal[2] = BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TRANSPARENT);
                g_battle_action_target_data->attack_type = BATTLE_ACTION_TYPE_STATUS_CHANGE;
            }
        }
    }
    return g_battle_action_target_data->attack_type;
}
