#include "fft/battle.h"
#include "psx/types.h"

s32 battle_formula_calculate_elemental_xa_times_ya(void) {
    battle_formula_store_xa_times_ya_damage();
    battle_formula_apply_weather_elemental_effects();
    battle_formula_apply_elemental();
    return g_battle_action_target_data->hit == 0;
}
