#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_draw_out_mp_damage(void) {
    u32 damage;
    battle_action_data_t* action;

    battle_formula_calculate_katana_break_chance();
    battle_formula_store_ma_and_y();
    battle_formula_apply_magical_support_status_compatibility();
    damage = g_current_ability.xa * g_current_ability.ya;
    action = g_battle_action_target_data;
    action->attack_type = BATTLE_ACTION_TYPE_MP_DAMAGE;
    action->mp_damage = damage;
}
