#include "fft/battle_action.h"
#include "fft/battle_formula.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "psx/types.h"

/* Dance/Song hit check: accuracy scaled by X%, then an X% roll to hit. */
s32 formula_calculate_dance_song_hit(void) {
    action_data_t* action = g_action_target_data;
    u8 hit_percent = g_action_current_ability.range_data.x;
    s32 action_state = g_action_state;

    action->attack_accuracy = action->attack_accuracy * hit_percent / 100;
    if (action_state != ACTION_STATE_EXECUTE || main_util_roll_pass_fail(100, hit_percent) == 0) {
        return 0;
    }
    formula_cause_action_miss();
    return 1;
}
