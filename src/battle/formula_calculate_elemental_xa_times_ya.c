#include "fft/battle_action.h"
#include "fft/battle_formula.h"
#include "psx/types.h"

/* Store XA * YA as HP damage, then the weather and element steps. Returns 1
 * when the element canceled the action. */
s32 formula_calculate_elemental_xa_times_ya(void) {
    formula_store_xa_times_ya_damage();
    formula_apply_weather_elemental_effects();
    formula_apply_elemental();
    return g_action_target_data->hit == 0;
}
