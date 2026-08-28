#include "fft/battle.h"
#include "psx/types.h"

/* Formula 0x34: heal (PA*Y) HP and (PA*Y/2) MP. */
void battle_formula_heal_pa_times_y_and_mp(void) {
    battle_action_data_t* action;
    u16 amount;

    battle_formula_store_pa_and_y();
    battle_formula_apply_attack_up_and_martial_arts();
    battle_formula_apply_zodiac_compatibility();
    battle_formula_store_xa_times_ya_damage();
    action = g_battle_action_target_data;
    amount = action->hp_damage;
    action->hp_damage = 0;
    action->hp_healing = amount;
    action->attack_type = BATTLE_ACTION_TYPE_HP_HEALING | BATTLE_ACTION_TYPE_MP_HEALING;
    /* hp_healing is re-read signed (lh) for the halving. */
    action->mp_healing = *(s16*)&action->hp_healing / 2;
}
