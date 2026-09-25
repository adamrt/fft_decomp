#include "fft/battle.h"

/*
 * Simulate an action and finalize its hit-percentage average when requested.
 *
 * Accounting starts only for the acting unit with an unset base hit rate.
 * The 0xff counter disables accounting; that path returns 1 without
 * requiring a valuable target. Counted entries are target/strike pairs.
 */
s32 battle_ai_call_ability_processing(battle_ai_command_action_t* action) {
    battle_ai_data_t* ai;
    s32 base_hit;
    s32 cnt;

    g_battle_ai_hit_counter = 0xFF;
    ai = &g_battle_ai_data_base;
    if (action->unit_id == g_battle_ai_data_base.acting_unit_id) {
        base_hit = g_battle_ai_current_action_base_hit_percent;
        g_battle_ai_data_base.ability_processing_done = 1;
        if (base_hit == 0xFF) {
            g_battle_ai_hit_counter = 0;
            g_battle_ai_data_base.total_hit_percent = 0;
            g_battle_ai_data_base.valuable_target_hit = 0;
        }
    }
    battle_ai_simulate_strikes(action, 0);
    cnt = ai->hit_counter;
    if (cnt != 0xFF) {
        if (ai->valuable_target_hit == 0 || cnt == 0) {
            return 0;
        }
        ai->current_action.base_hit_percent = ai->total_hit_percent / cnt;
    }
    return 1;
}
