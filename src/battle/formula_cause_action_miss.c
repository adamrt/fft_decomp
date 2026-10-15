#include "fft/battle_action.h"
#include "fft/battle_formula.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "psx/types.h"

/* Miss decided by the accuracy roll. */
void formula_cause_action_miss(void) {
    g_action_target_data->hit = 0;
    g_action_target_data->miss_type = ACTION_MISS_TYPE_ACCURACY_MISS;
}
