#include "fft/battle_action.h"
#include "fft/battle_formula.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "psx/types.h"

/* One evade roll, made only when executing: evaded with numerator in
 * denominator odds. Records the miss type and the item that blocked; returns 1
 * when evaded. */
s32 formula_calculate_attack_evaded(u16 numerator, u16 denominator, u8 miss_type, u8 item_lost) {
    if (main_util_roll_pass_fail(denominator, numerator) != 0) {
        return 0;
    }
    if (g_action_state != ACTION_STATE_EXECUTE) {
        return 0;
    }
    g_action_target_data->hit = 0;
    g_action_target_data->miss_type = miss_type;
    g_action_target_data->item_lost = item_lost;
    return 1;
}
