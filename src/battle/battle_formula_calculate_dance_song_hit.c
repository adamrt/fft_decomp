#include "fft/battle.h"
#include "psx/types.h"

/* Dance/Song hit check: accuracy scaled by X%, then an X% roll to hit. */
s32 battle_formula_calculate_dance_song_hit(void) {
    battle_action_data_t* action = g_battle_action_target_data;
    u8 hit_percent = g_current_ability.range_data.x;
    s32 action_state = g_battle_action_state;

    action->attack_accuracy = (s16)action->attack_accuracy * hit_percent / 100;
    if (action_state != BATTLE_ACTION_STATE_EXECUTE || main_util_roll_pass_fail(100, hit_percent) == 0) {
        return 0;
    }
    battle_formula_cause_action_miss();
    return 1;
}
