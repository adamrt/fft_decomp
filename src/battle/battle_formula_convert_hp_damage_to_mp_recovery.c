#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_convert_hp_damage_to_mp_recovery(void) {
    battle_action_data_t* action;
    u16 hp_damage;

    action = g_battle_action_target_data;
    hp_damage = action->hp_damage;
    action->hp_damage = 0;
    action->attack_type = BATTLE_ACTION_TYPE_MP_HEALING;
    action->mp_healing = hp_damage;
}
