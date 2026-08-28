#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_cause_action_miss(void) {
    g_battle_action_target_data->hit = 0;
    g_battle_action_target_data->miss_type = BATTLE_ACTION_MISS_TYPE_ACCURACY_MISS;
}
