#include "fft/battle_ai.h"
#include "fft/unit_slots.h"

/*
 * Prepare the acting unit's automatic targeting policy and focused target.
 *
 * Player-team units under direct control retain their configured automatic
 * behavior. Other units may abandon special focus behavior when its target is
 * no longer eligible, then select an appropriate nearby unit.
 */
void battle_ai_init_target_consideration(void) {
    battle_ai_unit_decision_t* decision;
    battle_ai_data_t* ai;
    battle_stats_t* acting;
    s32 flags;
    battle_ai_nearest_target_mode_e mode;
    s32 target;

    decision = g_battle_ai_acting_unit_decision_ptr;
    ai = &g_battle_ai_data_base;
    if (g_battle_ai_data_base.acting_unit_team == 0) {
        acting = g_battle_ai_temp_unit_data;
        if (acting->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) {
            ai->autobattle_setting = acting->auto_battle_setting;
            return;
        }
    }

    ai->autobattle_setting = ai->unit_behaviour[ai->acting_unit_id];
    flags = decision->flags;
    if (flags & BATTLE_AI_DECISION_SPECIAL_BEHAVIOR) {
        if (!(ai->acting_unit->status_sets.current[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_INVITE))) {
            if (decision->targeting_flags_2 & BATTLE_AI_TARGET_CONSERVE_CT)
                return;
            if (ai->autobattle_setting)
                return;
            if (flags & BATTLE_AI_DECISION_FOCUS_TARGET) {
                battle_ai_transfer_unit_coordinates(decision->main_target_id, &decision->target);
                target = battle_ai_check_target_type(decision->main_target_id);
            } else {
                target = ai->targetable_override;
            }
            if (target == 0)
                return;
        }
        decision->flags &= ~BATTLE_AI_DECISION_SPECIAL_BEHAVIOR;
    }

    if (ai->acting_unit_team == 0) {
        ai->autobattle_setting = 0;
        if (((s32)ai->acting_unit->hp << 7) / ai->acting_unit->max_hp < 45) {
            mode = BATTLE_AI_NEAREST_HEALER_OR_CRYSTAL;
        } else {
            for (target = 0; target < BATTLE_UNIT_SLOT_COUNT; target++)
                ai->targetability.live.unit_targetable[target] = 1;
            mode = BATTLE_AI_NEAREST_ELIGIBLE_ENEMY;
        }
    } else {
        /* Nested: a combined && folds both byte tests into one word test. */
        if (decision->targeting_flags_1 & 4) {
            if (!(decision->targeting_flags_2 & BATTLE_AI_TARGET_TEAM_SUPPORT_UNAVAILABLE))
                goto save_fading_life;
        }
        ai->autobattle_setting = 0;
        for (target = 0; target < BATTLE_UNIT_SLOT_COUNT; target++)
            ai->targetability.live.unit_targetable[target] = 1;
        mode = BATTLE_AI_NEAREST_ELIGIBLE_ENEMY;
    }
    target = battle_ai_find_nearest_target(mode);
    battle_ai_store_main_target_id_and_focus_on_target_flag(target);
    return;

save_fading_life: /* the target places this exit after the shared tail */
    ai->autobattle_setting = 0x10;
}
