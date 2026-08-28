#include "fft/battle.h"
#include "psx/types.h"

s32 battle_formula_calculate_attack_evaded(u16 numerator, u16 denominator, u8 miss_type, u8 item_lost) {
    if (main_util_roll_pass_fail(denominator, numerator) != 0) {
        return 0;
    }
    if (g_battle_action_state != BATTLE_ACTION_STATE_EXECUTE) {
        return 0;
    }
    g_battle_action_target_data->hit = 0;
    g_battle_action_target_data->miss_type = miss_type;
    g_battle_action_target_data->item_lost = item_lost;
    return 1;
}
