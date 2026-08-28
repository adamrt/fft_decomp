#include "fft/battle.h"
#include "fft/data.h"
#include "psx/types.h"

void battle_formula_store_xa_plus_ya_status_damage(void) {
    u16 original_xa;

    if (g_current_ability.ability_id != ABILITY_ID_SUMMON_MAGIC_GOLEM) {
        original_xa = g_current_ability.xa;
        g_current_ability.xa = g_current_ability.ya;
        battle_formula_apply_zodiac_compatibility();
        g_current_ability.ya = g_current_ability.xa;
        g_current_ability.xa = original_xa;
    }
    g_battle_action_target_data->hp_damage = g_current_ability.xa + g_current_ability.ya;
}
