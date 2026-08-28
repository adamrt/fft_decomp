#include "fft/battle.h"
#include "psx/types.h"

/* Restore the target to full HP and MP (100% HP/MP healing). */
void battle_formula_apply_full_hp_mp_heal(void) {
    battle_stats_t* unit = g_battle_action_target;
    battle_action_data_t* action = g_battle_action_target_data;

    action->hp_damage = unit->max_hp;
    action->mp_healing = unit->max_mp;
    action->attack_type = BATTLE_ACTION_TYPE_HP_DAMAGE | BATTLE_ACTION_TYPE_MP_HEALING;
}
