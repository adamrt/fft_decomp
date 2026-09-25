#include "fft/battle.h"
#include "psx/etc.h"

/*
 * Simulate the current action and intervening events, then score unit state.
 *
 * Return -1 to suspend, 0 for a rejected or flag-update-only pass, and 1
 * after scoring. Save/restore helpers preserve selected unit data, not the
 * whole AI workspace; resumed phases skip fresh action preparation.
 */
s32 battle_ai_simulate_action_and_score(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    battle_stats_t* unit;
    battle_ai_unit_decision_t* decision;
    battle_action_command_prefix_t* action;
    battle_ai_data_t* clear_cursor;
    battle_ai_data_t* decision_base;
    s32 i;
    s32 status;
    s32 weight_offset;
    s32 weight;
    s32 score;
    u32 product;
    u32 below_cost;

    if (g_battle_ai_data_base.decision_state && g_battle_ai_data_base.progress_0eee) {
        battle_ai_restore_unit_state(1);
        /* Resume: re-enter the suspended turn-advance or scoring phase. */
        if (g_battle_ai_data_base.progress_0eee == 1)
            goto advance_turns;
        if (g_battle_ai_data_base.progress_0eee == 2)
            goto score_state;
    }
    if (VSync(1) >= 441) {
        ai->progress_0eee = 0;
        return -1;
    }
    battle_ai_save_unit_state(0);
    ai->ability_processing_done = 0;
    /* Starting at the last unit_acts_before_me member makes GCC retain an
     * extra addiu and breaks the exact match. Anchor the descending cursor at
     * the equivalent offset from ai while retaining the named field access. */
    i = BATTLE_UNIT_SLOT_COUNT - 1;
    clear_cursor = (battle_ai_data_t*)((u8*)ai + (BATTLE_UNIT_SLOT_COUNT - 1));
    do {
        clear_cursor->unit_acts_before_me[0] = 0;
        clear_cursor = (battle_ai_data_t*)((u8*)clear_cursor - 1);
        i--;
    } while (i >= 0);
    ai->current_action.priority = 0;
    ai->current_action.base_hit_percent = 0xff;
    if (ai->current_action.target_flags_set) {
        if (ai->current_action.skillset != 0 && ai->current_action.skillset != SKILLSET_ID_DEFEND) {
            action = (battle_action_command_prefix_t*)&ai->current_action.unit_id;
            if (battle_action_call_attack_preparation_at_preview((u8*)action) == 1) {
                if (battle_ai_call_ability_processing((battle_ai_command_action_t*)action) == 0)
                    goto reject;
            }
            ai->acting_unit->action_taken = 1;
            battle_ai_simulate_movement_and_pickup();
        }
    } else {
        battle_ai_simulate_movement_and_pickup();
        if (ai->current_action.skillset != 0 && ai->current_action.skillset != SKILLSET_ID_DEFEND) {
            action = (battle_action_command_prefix_t*)&ai->current_action.unit_id;
            if (battle_action_call_attack_preparation_at_preview((u8*)action) == 1) {
                if (battle_ai_call_ability_processing((battle_ai_command_action_t*)action) == 0)
                    goto reject;
            }
            ai->acting_unit->action_taken = 1;
        }
    }
    if (ai->current_action.skillset == SKILLSET_ID_DEFEND) {
        battle_status_inflict_defending_to_battle_id(ai->acting_unit_id);
        ai->current_action.base_hit_percent = 100;
        ai->acting_unit->action_taken = 1;
    }
    if (ai->current_action.skillset == 0)
        ai->current_action.base_hit_percent = 100;
    battle_action_end_turn(ai->acting_unit_id);
    ai->saved_turn_state_0 = g_battle_between_turn_state;
    ai->saved_turn_state_1 = g_battle_between_turn_resume_state;
    ai->saved_turn_state_2 = g_battle_turn_unit_id;
    ai->simulated_turn_events = 0;
    do {
    advance_turns:
        if (VSync(1) >= 313) {
            battle_ai_save_unit_state(1);
            ai->progress_0eee = 1;
            return -1;
        }
        ai->simulated_turn_events++;
        if (ai->simulated_turn_events >= 256)
            break;
        i = battle_action_run_between_turn_control(0);
        status = i & 0xff00;
        i &= 0xff;
        unit = &g_battle_unit_stats[i];
        if (status == 0xff00)
            break;
        if (status == 0x300) {
            battle_action_finalize_attack_and_flag_reactions(i);
        } else if (status == 0x200) {
            if (battle_ai_call_ability_processing((battle_ai_command_action_t*)&unit->action_actor_id) == 0) {
                g_battle_between_turn_state = ai->saved_turn_state_0;
                g_battle_between_turn_resume_state = ai->saved_turn_state_1;
                g_battle_turn_unit_id = ai->saved_turn_state_2;
                goto reject;
            }
            unit->charged_ability_ct = 0xff;
            unit->status_sets.current[0] &= ~(BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHARGING)
                | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PERFORMING));
            unit->inflicted_status[0] &= ~(BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHARGING)
                | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PERFORMING));
        } else if (status == 0x100) {
            if (battle_ai_decide_status_ct_based(0, i))
                ai->unit_acts_before_me[i] = 1;
            unit->has_turn = 0;
        }
        for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
            unit = &g_battle_unit_stats[i];
            if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE
                && (unit->status_sets.current[0]
                    & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHARGING)
                        | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PERFORMING))))
                break;
        }
    } while (i != BATTLE_UNIT_ID_NONE);
    g_battle_between_turn_state = ai->saved_turn_state_0;
    g_battle_between_turn_resume_state = ai->saved_turn_state_1;
    g_battle_turn_unit_id = ai->saved_turn_state_2;
score_state:
    if (VSync(1) >= 441) {
        battle_ai_save_unit_state(1);
        ai->progress_0eee = 2;
        return -1;
    }
    if (ai->targetability.live.target_setting_flags & BATTLE_AI_TARGET_SETTING_REFRESH_UNIT_STATUS_FLAGS) {
        for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
            decision = &ai->unit_decisions[i];
            unit = &g_battle_unit_stats[i];
            decision->flags &= ~(BATTLE_AI_DECISION_TRANSIENT_DEAD | BATTLE_AI_DECISION_TRANSIENT_PETRIFIED
                | BATTLE_AI_DECISION_TRANSIENT_JUMP);
            if ((unit->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEAD))
                && !(unit->status_sets.current[2] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_RERAISE)))
                decision->flags |= BATTLE_AI_DECISION_TRANSIENT_DEAD;
            if (unit->status_sets.current[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PETRIFY))
                decision->flags |= BATTLE_AI_DECISION_TRANSIENT_PETRIFIED;
            if (unit->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_JUMP))
                decision->flags |= BATTLE_AI_DECISION_TRANSIENT_JUMP;
            if (!(decision->flags & 0x80)
                && !(unit->status_sets.current[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PETRIFY))
                && (!(unit->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEAD))
                    || (unit->status_sets.current[2] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_RERAISE)))
                && !(unit->status_sets.current[4] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEATH_SENTENCE)))
                decision->flags |= 0x80;
        }
        battle_ai_restore_unit_state(0);
        ai->decision_state = 0;
        return 0;
    }
    battle_ai_clear_current_action_rank_byte();
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        status = ai->unit_battle_ids[i];
        if (status == 0xff)
            continue;
        unit = &g_battle_unit_stats[i];
        score = unit->hp;
        score <<= 7;
        score /= unit->max_hp;
        status = 0;
        /* Starting at &unit_decisions[i] emits an extra addiu and breaks the
         * exact match. Keeping the stride as a typed workspace base reproduces
         * the target's large field offsets; direct indexed fields also schedule
         * the inner-loop cursor after its zero initializer. */
        decision_base = (battle_ai_data_t*)((u8*)ai + i * sizeof(battle_ai_unit_decision_t));
        for (weight_offset = 0; status < BATTLE_STATUS_COUNT; status++, weight_offset += 2) {
            if ((unit->status_sets.current[status / 8] << (status % 8)) & 0x80) {
                /* The target advances this signed table as a byte cursor;
                 * indexing the s16 array changes invariant allocation. */
                weight = *(s16*)((u8*)g_battle_ai_status_priority_weights + weight_offset);
                if ((unit->status_sets.current[1]
                        & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CONFUSION)
                            | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_BLOOD_SUCK)))
                    || (unit->status_sets.current[4] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHARM))) {
                    if (weight >= 0)
                        continue;
                }
                switch (status) {
                case BATTLE_STATUS_ID_DARKNESS:
                    weight = weight * decision_base->unit_decisions[0].evade_mod / 4;
                    break;
                case BATTLE_STATUS_ID_SILENCE:
                    weight = weight * decision_base->unit_decisions[0].silence_mod / 4;
                    break;
                case BATTLE_STATUS_ID_CONFUSION:
                case BATTLE_STATUS_ID_BLOOD_SUCK:
                case BATTLE_STATUS_ID_CHARM:
                    if ((unit->status_sets.current[3]
                            & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_SLOW)
                                | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_STOP)))
                        || (unit->status_sets.current[4]
                            & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_SLEEP)
                                | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DONT_MOVE)
                                | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DONT_ACT))))
                        weight /= 2;
                    break;
                case BATTLE_STATUS_ID_SLOW:
                case BATTLE_STATUS_ID_STOP:
                case BATTLE_STATUS_ID_SLEEP:
                case BATTLE_STATUS_ID_DONT_MOVE:
                case BATTLE_STATUS_ID_DONT_ACT:
                    if ((unit->status_sets.current[1]
                            & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CONFUSION)
                                | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_BLOOD_SUCK)))
                        || (unit->status_sets.current[4] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHARM)))
                        continue;
                    break;
                }
                score += weight;
            }
        }
        if (!(unit->unit_flags & UNIT_FLAG_MONSTER)) {
            for (status = 0; status < 7; status++) {
                if (unit->equipment[status] == g_battle_ai_unit_snapshots[ai->unit_battle_ids[i]].equipment[status])
                    score += 51;
            }
        }
        if (unit->max_mp) {
            weight_offset = unit->mp;
            below_cost = weight_offset;
            weight_offset <<= 6;
            below_cost = below_cost < ai->unit_decisions[i].lowest_mp_cost;
            weight_offset /= unit->max_mp;
            if (below_cost)
                weight_offset = (u32)weight_offset >> 1;
            product = weight_offset * ai->unit_decisions[i].mp_ability_mod;
            weight_offset = (s32)product / 4;
            score += weight_offset;
        }
        {
            s32 golem;

            weight_offset = g_battle_team_golem[(unit->team_flags >> 4) & 3];
            golem = weight_offset;
            weight_offset <<= 7;
            score += weight_offset / ai->present_unit_average_max_hp;
            if ((u32)golem < ai->team_golem.by_team[(unit->team_flags >> 4) & 3])
                score--;
        }
        if (ai->unit_decisions[i].enemy_flag)
            score = -score;
        ai->current_action.priority += score;
    }
    if (g_main_action_menu_types_by_skillset[ai->considered_ability.skillset] == ACTION_MENU_TYPE_WEAPON_INVENTORY
        && ai->considered_ability.ability_id != ABILITY_ID_THROW_SHURIKEN
        && ai->considered_ability.ability_id != ABILITY_ID_THROW_BALL)
        ai->current_action.base_hit_percent >>= 1;
    battle_ai_restore_unit_state(0);
    ai->decision_state = 0;
    return 1;
reject: /* shared rejection tail */
    battle_ai_restore_unit_state(0);
    ai->decision_state = 0;
    return 0;
}
