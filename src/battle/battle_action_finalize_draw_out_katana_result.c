#include "fft/battle.h"

/* Finalize Draw Out's katana-consumption result after target processing.
 *
 * hit_count is the number of units the strike hit; with none, or when the
 * katana did not break, the action becomes a pseudo-status hit. When the
 * katana broke, retail copies can_earn_experience onto itself (lbu/sb of the
 * same byte): a genuine no-op kept for the match. */
void battle_action_finalize_draw_out_katana_result(
    battle_stats_t* attacker, battle_strike_work_t* work, s32 hit_count) {
    u16 flags;

    if (g_main_action_menu_types_by_skillset[attacker->last_skillset_id] != ACTION_MENU_TYPE_KATANA_INVENTORY)
        return;
    if (attacker->job_id == JOB_ID_MIME)
        return;
    if ((hit_count & 0xFF) == 0
        || ((flags = g_current_action_data.special_effect) & BATTLE_ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_NOT_BROKEN)
            != 0) {
        g_current_action_data.special_effect = BATTLE_ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_NOT_BROKEN;
        g_current_action_data.attack_type = BATTLE_ACTION_TYPE_PSEUDO_STATUS;
        g_current_action_data.hit = 1;
        return;
    }
    if (flags & BATTLE_ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_BROKEN) {
        work->can_earn_experience = work->can_earn_experience;
    }
}
