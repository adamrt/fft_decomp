#include "fft/battle.h"

/*
 * Simulate strikes and eligible reactions, updating AI hit accounting.
 *
 * A nonzero reaction mode skips initial preparation and recursive reaction dispatch.
 * Hit accounting is disabled by counter 0xff; included target/strike pairs
 * can adjust priority and mark whether a valuable target was hit.
 */
void battle_ai_simulate_strikes(battle_ai_command_action_t* action, s32 reaction_mode) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    battle_strike_work_t work;
    s32 target_index;
    s32 target_id;
    battle_action_data_t* result;
    u8* behavior;
    s32 self_hit;
    s32 positive;
    s32 negative;

    if (reaction_mode == 0 && battle_action_init_current_data(action->unit_id) != 0) {
        return;
    }
    behavior = g_main_ability_data[(s16)action->ability_id].ai_flags.bytes;
    if (g_battle_ai_data_base.hit_counter != 0xff) {
        self_hit = 0;
        positive = 0;
        negative = 1;
        if (behavior[0] & BATTLE_AI_ABILITY_AFFECTS_STATS) {
            positive = 3;
            negative = 4;
        }
    }
    do {
        if (battle_action_resolve_ability_strike(action->unit_id, &work) == 0 && work.target_count != 0) {
            /* The target scans only for 0xff, without a count or 16-entry bound. */
            for (target_index = 0; work.target_list[target_index] != 0xff; target_index++) {
                target_id = work.target_list[target_index];
                result = &g_battle_unit_stats[target_id].action;
                if ((s16)result->attack_accuracy <= 0) {
                    continue;
                }
                if (!(behavior[1] & BATTLE_AI_ABILITY_FLAG_2_TARGET_MAP_TILES)) {
                    if (battle_ai_can_unit_be_targeted_cryst_trea_mount_trans(target_id) != 0) {
                        continue;
                    }
                    if (action->targeting_type == 6 && ai->unit_acts_before_me[action->target_id] != 0) {
                        if (action->target_id != target_id) {
                            continue;
                        }
                    } else if (!(behavior[0] & BATTLE_AI_ABILITY_TARGET_ALLIES)
                        && ai->unit_acts_before_me[target_id] != 0) {
                        continue;
                    }
                }
                battle_action_finalize_attack_and_flag_reactions(target_id);
                if (ai->hit_counter == 0xff) {
                    continue;
                }
                if (((behavior[0] & BATTLE_AI_ABILITY_TARGET_ENEMIES) && ai->unit_decisions[target_id].enemy_flag != 0)
                    || ((behavior[0] & BATTLE_AI_ABILITY_TARGET_ALLIES)
                        && ai->unit_decisions[target_id].enemy_flag == 0)) {
                    ai->current_action.priority += positive;
                } else if (((behavior[0] & BATTLE_AI_ABILITY_TARGET_ENEMIES)
                               && ai->unit_decisions[target_id].enemy_flag == 0)
                    || ((behavior[0] & BATTLE_AI_ABILITY_TARGET_ALLIES)
                        && ai->unit_decisions[target_id].enemy_flag != 0)) {
                    ai->current_action.priority -= negative;
                }
                ai->hit_counter++;
                ai->total_hit_percent += result->attack_accuracy;
                if (target_id == ai->acting_unit_id) {
                    self_hit = 1;
                }
                if (ai->targetability.live.unit_targetable[target_id] != 0) {
                    ai->valuable_target_hit = 1;
                }
            }
        }
        battle_action_store_target_stats_pointer_data(action->unit_id);
    } while (work.continue_attack != 0);
    if (ai->hit_counter != 0xff && ai->weapon_range_flag != 0) {
        if (ai->ability_effect_on_self == 1 && self_hit != 0) {
            ai->useful_on_caster_flag = 1;
        } else if (ai->ability_effect_on_self == 2 && self_hit == 0) {
            ai->useful_on_caster_flag = 1;
        }
    }
    battle_action_store_acting_unit_data(&g_battle_unit_stats[action->unit_id]);
    if (reaction_mode == 0) {
        /* Reuse the finished loop counter's low halfword for the reaction ID. */
        while ((target_id = battle_reaction_prepare_next((u16*)&target_index)) != -1) {
            if (ai->unit_acts_before_me[target_id] == 0) {
                battle_ai_simulate_strikes(
                    (battle_ai_command_action_t*)&g_battle_unit_stats[target_id].action_actor_id, 1);
            }
        }
    }
}
