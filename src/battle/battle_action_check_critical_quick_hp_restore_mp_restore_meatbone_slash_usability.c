#include "fft/battle.h"
#include "psx/types.h"

void battle_action_check_critical_quick_hp_restore_mp_restore_meatbone_slash_usability(s16 reaction_id) {
    battle_stats_t* unit;

    unit = g_battle_action_target;
    if ((unit->status_sets.current[2] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRITICAL))
        && (g_battle_action_target_data->attack_type & BATTLE_ACTION_TYPE_HP_DAMAGE)
        && battle_action_calculate_chance_to_react(unit) == 0) {
        g_battle_action_target_data->reaction_id = reaction_id;
    }
}
