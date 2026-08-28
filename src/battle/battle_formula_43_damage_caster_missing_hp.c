#include "fft/battle.h"
#include "psx/types.h"

/* Formula 0x43: damage equal to the caster's missing HP (MaxHP - CurHP). */
void battle_formula_43_damage_caster_missing_hp(void) {
    battle_stats_t* unit = g_battle_action_attacker;
    battle_action_data_t* action = g_battle_action_target_data;
    u16 max_hp = unit->max_hp;
    u16 cur_hp = unit->hp;
    action->attack_type = BATTLE_ACTION_TYPE_HP_DAMAGE;
    action->hp_damage = max_hp - cur_hp;
}
