#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_use_hp_damage_as_action_hit_percent(void) {
    s32 pct;
    s32 amount;
    s32 state;
    s32 xa;
    battle_action_data_t* act;

    act = g_battle_action_target_data;
    pct = (s16)act->hp_damage;
    if (pct >= 100) {
        act->hp_damage = 0;
        return;
    }
    xa = (s16)act->attack_accuracy;
    state = g_battle_action_state;
    amount = pct * xa / 100;
    act->hp_damage = 0;
    act->attack_accuracy = amount;
    if (state != BATTLE_ACTION_STATE_EXECUTE && pct != 0) {
        return;
    }
    if (main_util_roll_pass_fail(100, pct) != 0) {
        battle_formula_cause_action_miss();
        g_battle_action_target_data->hp_damage = 0;
    }
}
