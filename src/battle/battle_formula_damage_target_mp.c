#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_damage_target_mp(void) {
    battle_stats_t* target;
    battle_action_data_t* action;
    u16 mp;

    if (battle_formula_calculate_magical_evade() == 0) {
        if (battle_formula_calculate_magic_accuracy_no_elemental() == 0) {
            target = g_battle_action_target;
            action = g_battle_action_target_data;
            mp = target->mp;
            action->attack_type = BATTLE_ACTION_TYPE_MP_DAMAGE;
            action->mp_damage = mp;
        }
    }
}
