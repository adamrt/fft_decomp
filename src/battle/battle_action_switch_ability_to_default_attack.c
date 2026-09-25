#include "fft/battle.h"
#include "psx/types.h"

void battle_action_switch_ability_to_default_attack(void) {
    battle_stats_t* unit;
    battle_stats_t* u2;
    battle_stats_t* u3;
    u8* pc3;
    u8 saved_c3;
    u8 saved_skillset;
    u16 saved_ability;

    unit = g_battle_action_attacker;
    pc3 = &g_current_ability.strike_counter;
    saved_c3 = *pc3;
    saved_skillset = unit->last_skillset_id;
    unit->last_skillset_id = SKILLSET_ID_ATTACK;
    u2 = g_battle_action_attacker;
    saved_ability = *(u16*)&unit->last_ability_id;
    *(u16*)&u2->last_ability_id = 0;
    battle_action_run_pre_formula_setup(&u2->action_actor_id, g_current_ability.target_id);
    u3 = g_battle_action_attacker;
    *pc3 = saved_c3;
    *(u16*)&u3->last_ability_id = saved_ability;
    u3->last_skillset_id = saved_skillset;
    g_current_ability.elemental_flags = 0;
    g_current_ability.reaction_id = 0;
}
