#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_damage_pa_times_y_damage_caster_pa_times_y_over_x(void) {
    s16 damage;
    battle_action_data_t* action;

    battle_formula_store_pa_and_y();
    battle_formula_apply_zodiac_compatibility();
    damage = g_current_ability.xa * g_current_ability.ya;
    /* Cast-pointer stores: as struct members GCC sinks them past the
     * following scalar global loads; the target keeps source order. */
    *(s16*)&g_battle_action_target_data->hp_damage = damage;
    damage = damage / g_current_ability.range_data.x;
    action = g_battle_action_attacker_data;
    action->hit = 1;
    *(s16*)&action->hp_damage = damage;
    g_battle_action_target_data->attack_type = BATTLE_ACTION_TYPE_HP_DAMAGE;
    g_battle_action_attacker_data->attack_type = BATTLE_ACTION_TYPE_HP_DAMAGE;
}
