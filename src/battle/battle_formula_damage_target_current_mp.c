#include "fft/battle.h"
#include "psx/types.h"

/* Formula 0x44: damage equal to the target's current MP. */
void battle_formula_damage_target_current_mp(void) {
    u16 mp = g_battle_action_target->mp;
    battle_action_data_t* action = g_battle_action_target_data;
    action->attack_type = BATTLE_ACTION_TYPE_HP_DAMAGE;
    action->hp_damage = mp;
}
