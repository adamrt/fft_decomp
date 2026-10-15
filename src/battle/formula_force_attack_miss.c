#include "fft/battle_action.h"
#include "fft/battle_formula.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "psx/types.h"

void formula_force_attack_miss(void) {
    g_action_target_data->hit = 0;
    g_action_target_data->miss_type = ACTION_MISS_TYPE_FORCED_FAILURE;
    /* The accuracy field is cleared as a 16-bit value (sh). */
    g_action_target_data->attack_accuracy = 0;
}
