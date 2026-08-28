#include "fft/battle.h"
#include "psx/types.h"

/* Dispatch the target's first active MP Switch, Distribute, or Damage Split reaction. */
void battle_action_check_mp_switch_distribute_and_damage_split_usability(void) {
    battle_stats_t* unit;
    u8 flags;

    if (g_current_ability.formula == 7)
        return;
    if (battle_action_can_unit_react_1(g_battle_action_target) != 0)
        return;
    unit = g_battle_action_target;
    flags = unit->reaction_abilities[2];
    if (flags & BATTLE_REACTION_SET_3_MP_SWITCH) {
        battle_action_check_mp_switch_usability();
        return;
    }
    if (flags & BATTLE_REACTION_SET_3_DISTRIBUTE) {
        battle_action_check_distribute_usability();
        return;
    }
    if (unit->reaction_abilities[3] & BATTLE_REACTION_SET_4_DAMAGE_SPLIT) {
        battle_action_check_damage_split_usability();
    }
}
