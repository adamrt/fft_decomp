/* Ai action scoring. Its start is a cut chosen by topic; its end is a boundary the binary proves. */

#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_status.h"
#include "fft/battle_unit.h"
#include "fft/memory.h"
#include "psx/libetc.h"
#include "psx/types.h"

/* Unit-state utility at full HP/MP, unchanged equipment, or one average HP of Golem. */
enum {
    AI_HP_SCORE_SCALE = 128,
    AI_MP_SCORE_SCALE = 64,
    AI_EQUIPMENT_SCORE = 51,
    AI_GOLEM_SCORE_SCALE = 128,
};

/*
 * Simulate the current action and intervening events, then score unit state.
 *
 * Return -1 to suspend, 0 for a rejected or flag-update-only pass, and 1
 * after scoring. Save/restore helpers preserve selected unit data, not the
 * whole AI workspace; resumed phases skip fresh action preparation.
 */
s32 ai_simulate_action_and_score(void) {
    ai_data_t* ai = &g_ai_data_base;
    unit_stats_t* unit;
    ai_unit_decision_t* decision;
    action_command_t* action;
    ai_data_t* clear_cursor;
    ai_data_t* decision_base;
    s32 unit_index;
    s32 status;
    s32 equipment_slot;
    s32 weight_offset;
    s32 weight;
    s32 score;
    u32 below_cost;

    if (g_ai_data_base.resume_pending && g_ai_data_base.action_simulation_phase) {
        ai_restore_unit_state(AI_SNAPSHOT_SIMULATED_STATUS);
        /* Resume: re-enter the suspended turn-advance or scoring phase. */
        if (g_ai_data_base.action_simulation_phase == AI_SIMULATION_ADVANCE_TURNS)
            goto advance_turns;
        if (g_ai_data_base.action_simulation_phase == AI_SIMULATION_SCORE_STATE)
            goto score_state;
    }
    if (VSync(1) >= AI_DECISION_SCANLINE_LIMIT) {
        ai->action_simulation_phase = AI_SIMULATION_PREPARE_ACTION;
        return -1;
    }
    ai_save_unit_state(AI_SNAPSHOT_BASELINE);
    ai->ability_processing_done = 0;
    /* Direct array iteration changes the bytes; retain a workspace-relative cursor. */
    for (unit_index = BATTLE_UNIT_SLOT_COUNT - 1, clear_cursor = (ai_data_t*)((u8*)ai + (BATTLE_UNIT_SLOT_COUNT - 1));
        unit_index >= 0; unit_index--, clear_cursor = (ai_data_t*)((u8*)clear_cursor - 1)) {
        clear_cursor->unit_acts_before_me[0] = 0;
    }
    ai->current_action.rank.priority = 0;
    ai->current_action.rank.base_hit_percent = 0xff;
    if (ai->current_action.act_before_move) {
        if (ai->current_action.command.skillset_id != 0
            && ai->current_action.command.skillset_id != SKILLSET_ID_DEFEND) {
            action = &ai->current_action.command;
            if (action_call_attack_preparation_at_preview(action) == BATTLE_ATTACK_PREPARATION_IMMEDIATE
                && ai_simulate_action_hits(action) == 0) {
                ai_restore_unit_state(AI_SNAPSHOT_BASELINE);
                ai->resume_pending = 0;
                return 0;
            }
            ai->acting_unit->action_taken = 1;
            ai_simulate_movement_and_pickup();
        }
    } else {
        ai_simulate_movement_and_pickup();
        if (ai->current_action.command.skillset_id != 0
            && ai->current_action.command.skillset_id != SKILLSET_ID_DEFEND) {
            action = &ai->current_action.command;
            if (action_call_attack_preparation_at_preview(action) == BATTLE_ATTACK_PREPARATION_IMMEDIATE
                && ai_simulate_action_hits(action) == 0) {
                ai_restore_unit_state(AI_SNAPSHOT_BASELINE);
                ai->resume_pending = 0;
                return 0;
            }
            ai->acting_unit->action_taken = 1;
        }
    }
    if (ai->current_action.command.skillset_id == SKILLSET_ID_DEFEND) {
        status_inflict_defending_to_battle_id(ai->acting_unit_id);
        ai->current_action.rank.base_hit_percent = 100;
        ai->acting_unit->action_taken = 1;
    }
    if (ai->current_action.command.skillset_id == 0)
        ai->current_action.rank.base_hit_percent = 100;
    action_end_turn(ai->acting_unit_id);
    ai->saved_between_turn_state = g_battle_between_turn_state;
    ai->saved_between_turn_resume_state = g_battle_between_turn_resume_state;
    ai->saved_turn_unit_id = g_battle_turn_unit_id;
    ai->simulated_turn_events = 0;
    for (;;) {
    advance_turns:
        if (VSync(1) >= AI_TURN_SIMULATION_SCANLINE_LIMIT) {
            ai_save_unit_state(AI_SNAPSHOT_SIMULATED_STATUS);
            ai->action_simulation_phase = AI_SIMULATION_ADVANCE_TURNS;
            return -1;
        }
        ai->simulated_turn_events++;
        if (ai->simulated_turn_events >= AI_SIMULATED_TURN_LIMIT)
            break;
        unit_index = action_run_between_turn_control(BATTLE_BETWEEN_TURN_MODE_RUN);
        status = unit_index & 0xff00;
        unit_index &= 0xff;
        unit = &g_battle_unit_stats[unit_index];
        if (status == BATTLE_TURN_EVENT_NONE)
            break;
        if (status == BATTLE_TURN_EVENT_ACTION_RESULT) {
            action_finalize_attack_and_flag_reactions(unit_index);
        } else if (status == BATTLE_TURN_EVENT_ABILITY_READY) {
            if (ai_simulate_action_hits(&unit->action_command) == 0) {
                g_battle_between_turn_state = ai->saved_between_turn_state;
                g_battle_between_turn_resume_state = ai->saved_between_turn_resume_state;
                g_battle_turn_unit_id = ai->saved_turn_unit_id;
                /* An early return duplicates this shared cleanup tail. */
                goto finish_without_scoring;
            }
            unit->charged_ability_ct = 0xff;
            unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
                &= ~(STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_PERFORMING));
            unit->inflicted_status[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
                &= ~(STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_PERFORMING));
        } else if (status == BATTLE_TURN_EVENT_UNIT_READY) {
            if (ai_is_unit_active_after_delay(0, unit_index))
                ai->unit_acts_before_me[unit_index] = 1;
            unit->has_turn = 0;
        }
        for (unit_index = 0; unit_index < BATTLE_UNIT_SLOT_COUNT; unit_index++) {
            unit = &g_battle_unit_stats[unit_index];
            if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE
                && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
                    & (STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_PERFORMING))))
                break;
        }
        if (unit_index == BATTLE_UNIT_ID_NONE)
            break;
    }
    g_battle_between_turn_state = ai->saved_between_turn_state;
    g_battle_between_turn_resume_state = ai->saved_between_turn_resume_state;
    g_battle_turn_unit_id = ai->saved_turn_unit_id;
score_state:
    if (VSync(1) >= AI_DECISION_SCANLINE_LIMIT) {
        ai_save_unit_state(AI_SNAPSHOT_SIMULATED_STATUS);
        ai->action_simulation_phase = AI_SIMULATION_SCORE_STATE;
        return -1;
    }
    if (ai->targetability.live.target_setting_flags & AI_TARGET_SETTING_REFRESH_UNIT_STATUS_FLAGS) {
        for (unit_index = 0; unit_index < BATTLE_UNIT_SLOT_COUNT; unit_index++) {
            decision = &ai->unit_decisions[unit_index];
            unit = &g_battle_unit_stats[unit_index];
            decision->flags
                &= ~(AI_DECISION_TRANSIENT_DEAD | AI_DECISION_TRANSIENT_PETRIFIED | AI_DECISION_TRANSIENT_JUMP);
            if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD))
                && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_RERAISE)]
                    & STATUS_BYTE_MASK(STATUS_ID_RERAISE)))
                decision->flags |= AI_DECISION_TRANSIENT_DEAD;
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)] & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))
                decision->flags |= AI_DECISION_TRANSIENT_PETRIFIED;
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_JUMP)] & STATUS_BYTE_MASK(STATUS_ID_JUMP))
                decision->flags |= AI_DECISION_TRANSIENT_JUMP;
            if (!(decision->flags & AI_DECISION_CONSIDERED)
                && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
                    & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))
                && (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD))
                    || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_RERAISE)]
                        & STATUS_BYTE_MASK(STATUS_ID_RERAISE)))
                && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEATH_SENTENCE)]
                    & STATUS_BYTE_MASK(STATUS_ID_DEATH_SENTENCE)))
                decision->flags |= AI_DECISION_CONSIDERED;
        }
    } else {
        ai_clear_current_action_decision();
        for (unit_index = 0; unit_index < BATTLE_UNIT_SLOT_COUNT; unit_index++) {
            status = ai->unit_compact_indices[unit_index];
            if (status == 0xff)
                continue;
            unit = &g_battle_unit_stats[unit_index];
            score = unit->hp;
            score *= AI_HP_SCORE_SCALE;
            score /= unit->max_hp;
            status = 0;
            /* A pointer to unit_decisions adds an addiu and changes scheduling;
             * retain the workspace-relative stride. */
            decision_base = (ai_data_t*)((u8*)ai + unit_index * sizeof(ai_unit_decision_t));
            for (weight_offset = 0; status < STATUS_COUNT; status++, weight_offset += 2) {
                if (!((unit->status_sets.current[status / 8] << (status % 8)) & 0x80))
                    continue;
                /* Indexing the s16 table changes register allocation; retain the byte cursor. */
                weight = *(s16*)((u8*)g_ai_status_priority_weights + weight_offset);
                if (((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CONFUSION)]
                         & (STATUS_BYTE_MASK(STATUS_ID_CONFUSION) | STATUS_BYTE_MASK(STATUS_ID_BLOOD_SUCK)))
                        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARM)]
                            & STATUS_BYTE_MASK(STATUS_ID_CHARM)))
                    && weight >= 0)
                    continue;
                switch (status) {
                case STATUS_ID_DARKNESS:
                    weight = weight * decision_base->unit_decisions[0].evade_mod / 4;
                    break;
                case STATUS_ID_SILENCE:
                    weight = weight * decision_base->unit_decisions[0].silence_mod / 4;
                    break;
                case STATUS_ID_CONFUSION:
                case STATUS_ID_BLOOD_SUCK:
                case STATUS_ID_CHARM:
                    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SLOW)]
                            & (STATUS_BYTE_MASK(STATUS_ID_SLOW) | STATUS_BYTE_MASK(STATUS_ID_STOP)))
                        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SLEEP)]
                            & (STATUS_BYTE_MASK(STATUS_ID_SLEEP) | STATUS_BYTE_MASK(STATUS_ID_DONT_MOVE)
                                | STATUS_BYTE_MASK(STATUS_ID_DONT_ACT))))
                        weight /= 2;
                    break;
                case STATUS_ID_SLOW:
                case STATUS_ID_STOP:
                case STATUS_ID_SLEEP:
                case STATUS_ID_DONT_MOVE:
                case STATUS_ID_DONT_ACT:
                    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CONFUSION)]
                            & (STATUS_BYTE_MASK(STATUS_ID_CONFUSION) | STATUS_BYTE_MASK(STATUS_ID_BLOOD_SUCK)))
                        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARM)]
                            & STATUS_BYTE_MASK(STATUS_ID_CHARM)))
                        continue;
                    break;
                }
                score += weight;
            }
            if (!(unit->unit_flags & UNIT_FLAG_MONSTER)) {
                for (equipment_slot = 0; equipment_slot < UNIT_EQUIPMENT_SLOT_COUNT; equipment_slot++) {
                    if (unit->equipment[equipment_slot]
                        == g_ai_unit_snapshots[ai->unit_compact_indices[unit_index]].equipment[equipment_slot])
                        score += AI_EQUIPMENT_SCORE;
                }
            }
            if (unit->max_mp) {
                weight_offset = unit->mp;
                below_cost = weight_offset;
                weight_offset *= AI_MP_SCORE_SCALE;
                below_cost = below_cost < ai->unit_decisions[unit_index].lowest_mp_cost;
                weight_offset /= unit->max_mp;
                if (below_cost)
                    weight_offset = (u32)weight_offset >> 1;
                weight_offset = weight_offset * ai->unit_decisions[unit_index].mp_ability_mod / 4;
                score += weight_offset;
            }
            {
                u32 golem;

                weight_offset = g_battle_team_golem[(unit->team_flags >> BATTLE_TEAM_SHIFT) & BATTLE_TEAM_INDEX_MASK];
                golem = weight_offset;
                weight_offset *= AI_GOLEM_SCORE_SCALE;
                score += weight_offset / ai->present_unit_average_max_hp;
                if (golem < ai->team_golem[(unit->team_flags >> BATTLE_TEAM_SHIFT) & BATTLE_TEAM_INDEX_MASK])
                    score--;
            }
            if (ai->unit_decisions[unit_index].enemy_flag)
                score = -score;
            ai->current_action.rank.priority += score;
        }
        if (g_main_action_menu_types_by_skillset[ai->considered_ability.skillset] == ACTION_MENU_TYPE_WEAPON_INVENTORY
            && ai->considered_ability.ability_id != ABILITY_ID_THROW_SHURIKEN
            && ai->considered_ability.ability_id != ABILITY_ID_THROW_BALL)
            ai->current_action.rank.base_hit_percent >>= 1;
        ai_restore_unit_state(AI_SNAPSHOT_BASELINE);
        ai->resume_pending = 0;
        return 1;
    }
finish_without_scoring:
    ai_restore_unit_state(AI_SNAPSHOT_BASELINE);
    ai->resume_pending = 0;
    return 0;
}

/*
 * Simulate movement and its crystal or treasure benefit.
 *
 * An unchanged 32-bit coordinate value skips all updates. Crystal recovery clears
 * Critical; a noncritical pickup adds priority unless the policy is Retreat.
 */
void ai_simulate_movement_and_pickup(void) {
    ai_data_t* ai = &g_ai_data_base;

    if (ai->current_action.coords.word != ai->acting_unit_coords.word) {
        ai_move_temp_unit_to_coords(&ai->current_action.coords);
        if (ai->tile_pickup_kind == AI_TILE_PICKUP_CRYSTAL) {
            ai->acting_unit->hp = ai->acting_unit->max_hp;
            ai->acting_unit->mp = ai->acting_unit->max_mp;
            ai->acting_unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRITICAL)]
                &= ~STATUS_BYTE_MASK(STATUS_ID_CRITICAL);
            ai->acting_unit->inflicted_status[STATUS_BYTE_INDEX(STATUS_ID_CRITICAL)]
                &= ~STATUS_BYTE_MASK(STATUS_ID_CRITICAL);
        }
        if (ai->tile_pickup_kind != AI_TILE_PICKUP_NONE
            && !(ai->acting_unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRITICAL)]
                & STATUS_BYTE_MASK(STATUS_ID_CRITICAL))
            && ai->autobattle_setting != AI_AUTOBATTLE_RETREAT) {
            ai->current_action.rank.priority += 2;
        }
        ai->acting_unit->movement_taken = 1;
    }
}

/* Mark the candidate action as not decided on: only the selected action ever
 * holds AI_ACTION_DECIDED (ai_build_command). */
void ai_clear_current_action_decision(void) {
    g_ai_data_base.current_action.rank.decided = 0;
}

/*
 * Simulate an action and finalize its hit-percentage average when requested.
 *
 * Accounting starts only for the acting unit with an unset base hit rate.
 * The 0xff counter disables accounting; that path returns 1 without
 * requiring a valuable target. Counted entries are target/strike pairs.
 */
s32 ai_simulate_action_hits(action_command_t* action) {
    ai_data_t* ai;
    s32 base_hit;
    s32 cnt;

    ai = &g_ai_data_base;
    g_ai_data_base.hit_counter = 0xFF;
    if (action->actor_id == g_ai_data_base.acting_unit_id) {
        base_hit = g_ai_data_base.current_action.rank.base_hit_percent;
        g_ai_data_base.ability_processing_done = 1;
        if (base_hit == 0xFF) {
            g_ai_data_base.hit_counter = 0;
            g_ai_data_base.total_hit_percent = 0;
            g_ai_data_base.valuable_target_hit = 0;
        }
    }
    ai_simulate_strikes(action, 0);
    cnt = ai->hit_counter;
    if (cnt != 0xFF) {
        if (ai->valuable_target_hit == 0 || cnt == 0) {
            return 0;
        }
        ai->current_action.rank.base_hit_percent = ai->total_hit_percent / cnt;
    }
    return 1;
}

/*
 * Simulate strikes and eligible reactions, updating AI hit accounting.
 *
 * A nonzero reaction mode skips initial preparation and recursive reaction dispatch.
 * Hit accounting is disabled by counter 0xff; included target/strike pairs
 * can adjust priority and mark whether a valuable target was hit.
 */
void ai_simulate_strikes(action_command_t* action, s32 reaction_mode) {
    ai_data_t* ai = &g_ai_data_base;
    battle_strike_work_t work;
    s32 target_index;
    s32 target_id;
    action_data_t* result;
    ability_ai_flags_t* behavior;
    s32 self_hit;
    s32 positive;
    s32 negative;

    if (reaction_mode == 0 && action_init_current_data(action->actor_id) != 0) {
        return;
    }
    behavior = &g_main_ability_data[action->ability_id].ai_flags;
    if (g_ai_data_base.hit_counter != 0xff) {
        self_hit = 0;
        positive = 0;
        negative = 1;
        if (behavior->flags_1 & AI_ABILITY_AFFECTS_STATS) {
            positive = 3;
            negative = 4;
        }
    }
    do {
        if (action_resolve_ability_strike(action->actor_id, &work) == 0 && work.target_count != 0) {
            /* The target scans only for 0xff, without a count or 16-entry bound. */
            for (target_index = 0; work.target_list[target_index] != 0xff; target_index++) {
                target_id = work.target_list[target_index];
                result = &g_battle_unit_stats[target_id].action;
                if (result->attack_accuracy <= 0) {
                    continue;
                }
                if (!(behavior->flags_2 & AI_ABILITY_FLAG_2_TARGET_MAP_TILES)) {
                    if (ai_is_unit_untargetable(target_id) != 0) {
                        continue;
                    }
                    if (action->targeting_type == BATTLE_TARGETING_TYPE_UNIT
                        && ai->unit_acts_before_me[action->target_id] != 0) {
                        if (action->target_id != target_id) {
                            continue;
                        }
                    } else if (!(behavior->flags_1 & AI_ABILITY_TARGET_ALLIES)
                        && ai->unit_acts_before_me[target_id] != 0) {
                        continue;
                    }
                }
                action_finalize_attack_and_flag_reactions(target_id);
                if (ai->hit_counter == 0xff) {
                    continue;
                }
                if (((behavior->flags_1 & AI_ABILITY_TARGET_ENEMIES) && ai->unit_decisions[target_id].enemy_flag != 0)
                    || ((behavior->flags_1 & AI_ABILITY_TARGET_ALLIES)
                        && ai->unit_decisions[target_id].enemy_flag == 0)) {
                    ai->current_action.rank.priority += positive;
                } else if (((behavior->flags_1 & AI_ABILITY_TARGET_ENEMIES)
                               && ai->unit_decisions[target_id].enemy_flag == 0)
                    || ((behavior->flags_1 & AI_ABILITY_TARGET_ALLIES)
                        && ai->unit_decisions[target_id].enemy_flag != 0)) {
                    ai->current_action.rank.priority -= negative;
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
        action_store_target_stats_pointer_data(action->actor_id);
    } while (work.continue_attack != 0);
    if (ai->hit_counter != 0xff && ai->ranked_origin_scan != 0) {
        if (ai->ability_effect_on_self == AI_ABILITY_EFFECT_USEFUL && self_hit != 0) {
            ai->useful_on_caster_flag = 1;
        } else if (ai->ability_effect_on_self == AI_ABILITY_EFFECT_ADVERSE && self_hit == 0) {
            ai->useful_on_caster_flag = 1;
        }
    }
    action_store_acting_unit_data(&g_battle_unit_stats[action->actor_id]);
    if (reaction_mode == 0) {
        /* Reuse the finished loop counter's low 16 bits for the reaction ID. */
        while ((target_id = battle_reaction_prepare_next((s16*)&target_index)) != -1) {
            if (ai->unit_acts_before_me[target_id] == 0) {
                ai_simulate_strikes(&g_battle_unit_stats[target_id].action_command, 1);
            }
        }
    }
}

/*
 * Save baseline unit data, or suspend simulated status and restore baseline status.
 *
 * BASELINE saves selected fields, action records, and team Golem values. Any
 * non-baseline mode saves only the compact status record before restoring the
 * baseline identity, death counter, current status, and Death Sentence CT.
 */
void ai_save_unit_state(ai_snapshot_mode_e mode) {
    ai_data_t* ai = &g_ai_data_base;
    ai_status_snapshot_t* status;
    ai_extended_snapshot_t* snapshot;
    unit_stats_t* unit;
    s32 i;
    s32 id;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        id = ai->unit_compact_indices[i];
        if (id == 0xff) {
            continue;
        }
        unit = &g_battle_unit_stats[i];
        if (mode != AI_SNAPSHOT_BASELINE) {
            status = &ai->unit_status_records[id];
        } else {
            status = &g_ai_unit_snapshots[id].status;
        }
        status->entd_slot = unit->entd_slot;
        status->death_counter = unit->death_counter;
        mem_copy_bytes(status->current_status, unit->status_sets.current, sizeof(status->current_status));
        if (mode != AI_SNAPSHOT_BASELINE) {
            status->death_sentence_ct = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)];
            snapshot = &g_ai_unit_snapshots[id];
            unit->entd_slot = snapshot->status.entd_slot;
            unit->death_counter = snapshot->status.death_counter;
            unit->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)]
                = snapshot->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)];
            mem_copy_bytes(
                unit->status_sets.current, snapshot->status.current_status, sizeof(snapshot->status.current_status));
        } else {
            snapshot = (ai_extended_snapshot_t*)status;
            snapshot->team_flags = unit->team_flags;
            mem_copy_bytes(snapshot->status_ct, unit->status_ct, BATTLE_TIMED_STATUS_COUNT);
            mem_copy_bytes(snapshot->equipment, unit->equipment, sizeof(snapshot->equipment));
            snapshot->brave = unit->brave;
            mem_copy_bytes(&snapshot->faith, &unit->faith, 4);
            snapshot->mp = unit->mp;
            mem_copy_bytes(snapshot->base_attributes, unit->base_attributes, 3);
            mem_copy_bytes(snapshot->attributes, unit->attributes, 4);
            mem_copy_bytes(&snapshot->x, &unit->x, 3);
            snapshot->charged_ability_ct = unit->charged_ability_ct;
            mem_copy_bytes(&snapshot->has_turn, &unit->has_turn, 3);
            snapshot->mount_info = unit->mount_info;
            mem_copy_bytes(&snapshot->auto_battle_setting, &unit->auto_battle_setting, 8);
            mem_copy_u16_values(&ai->unit_action_records[id], &unit->action_command, 20);
        }
    }
    if (mode == AI_SNAPSHOT_BASELINE) {
        mem_copy_bytes(ai->team_golem, g_battle_team_golem, sizeof(ai->team_golem));
    }
}

/*
 * Restore baseline unit data or suspended simulation status.
 *
 * BASELINE restores selected baseline fields, action records, and team Golem
 * values. Any non-baseline mode restores only the compact status record.
 */
void ai_restore_unit_state(ai_snapshot_mode_e mode) {
    ai_data_t* ai = &g_ai_data_base;
    ai_status_snapshot_t* status;
    ai_extended_snapshot_t* snapshot;
    unit_stats_t* unit;
    s32 i;
    s32 id;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        id = ai->unit_compact_indices[i];
        if (id == 0xff) {
            continue;
        }
        unit = &g_battle_unit_stats[i];
        if (mode != AI_SNAPSHOT_BASELINE) {
            status = &ai->unit_status_records[id];
        } else {
            status = &g_ai_unit_snapshots[id].status;
        }
        unit->entd_slot = status->entd_slot;
        unit->death_counter = status->death_counter;
        mem_copy_bytes(unit->status_sets.current, status->current_status, sizeof(status->current_status));
        if (mode != AI_SNAPSHOT_BASELINE) {
            unit->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)] = status->death_sentence_ct;
        } else {
            snapshot = (ai_extended_snapshot_t*)status;
            unit->team_flags = snapshot->team_flags;
            mem_copy_bytes(unit->status_ct, snapshot->status_ct, BATTLE_TIMED_STATUS_COUNT);
            mem_copy_bytes(unit->equipment, snapshot->equipment, sizeof(snapshot->equipment));
            unit->brave = snapshot->brave;
            mem_copy_bytes(&unit->faith, &snapshot->faith, 4);
            unit->mp = snapshot->mp;
            mem_copy_bytes(unit->base_attributes, snapshot->base_attributes, 3);
            mem_copy_bytes(unit->attributes, snapshot->attributes, 4);
            mem_copy_bytes(&unit->x, &snapshot->x, 3);
            unit->charged_ability_ct = snapshot->charged_ability_ct;
            mem_copy_bytes(&unit->has_turn, &snapshot->has_turn, 3);
            unit->mount_info = snapshot->mount_info;
            mem_copy_bytes(&unit->auto_battle_setting, &snapshot->auto_battle_setting, 8);
            mem_copy_u16_values(&unit->action_command, &ai->unit_action_records[id], 20);
        }
    }
    if (mode == AI_SNAPSHOT_BASELINE) {
        mem_copy_bytes(g_battle_team_golem, ai->team_golem, sizeof(ai->team_golem));
    }
}

/* Keep in the target list only the units the considered ability is useful on.
 * Returns 1 while one is left. */
s32 ai_filter_useful_targets(void) {
    ai_data_t* ai;
    s32 result;
    s32 i;

    result = 0;
    ai = &g_ai_data_base;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        if (ai->targetability.live.unit_targetable[i] != 0) {
            if (ai_classify_ability_effect(i, 1) == AI_ABILITY_EFFECT_USEFUL) {
                result = 1;
            } else {
                ai->targetability.live.unit_targetable[i] = 0;
            }
        }
    }
    return result;
}

/*
 * Return the AI's usefulness category for one target.
 *
 * This is the acting side's heuristic, not the sign of HP damage or a success
 * guarantee. Nonzero check_reflect enables the timing-aware Reflect restriction.
 */
ai_ability_effect_e ai_classify_ability_effect(s32 unit_id, s32 check_reflect) {
    ai_data_t* ai = &g_ai_data_base;
    unit_stats_t* unit = &g_battle_unit_stats[unit_id];
    ai_unit_decision_t* decision = &ai->unit_decisions[unit_id];
    s32 ct = g_ai_data_base.considered_ability.ct;
    s32 i;
    s32 bit;
    s32 mask;
    s32 status;
    s16 ability_id;

    if ((g_ai_data_base.considered_ability.ai_flags.word & AI_ABILITY_ENEMIES_ONLY) && !decision->enemy_flag)
        return AI_ABILITY_EFFECT_NONE;
    if ((ai->considered_ability.ai_flags.word & AI_ABILITY_ALLIES_ONLY) && decision->enemy_flag)
        return AI_ABILITY_EFFECT_NONE;
    if (unit_id == ai->acting_unit_id) {
        ability_id = ai->considered_ability.ability_id;
        if (ability_id < ABILITY_ID_ITEM_FIRST
            && (g_main_ability_range_data[ability_id].flags_2 & ABILITY_SECONDARY_FLAG_2_CANNOT_HIT_CASTER))
            return AI_ABILITY_EFFECT_NONE;
    }
    if (check_reflect && (ai->considered_ability.ai_flags.word & AI_ABILITY_REFLECTABLE)
        && ai_is_status_active_through_delay(ct, unit, STATUS_ID_REFLECT))
        return AI_ABILITY_EFFECT_NONE;
    if ((ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTED_BY_FAITH)
        && ai_is_status_active_through_delay(ct, unit, STATUS_ID_INNOCENT))
        return AI_ABILITY_EFFECT_NONE;
    if (ai_is_status_active_through_delay(ct, unit, STATUS_ID_JUMP))
        return AI_ABILITY_EFFECT_NONE;

    /* Death/revival timing determines which effects can still matter. */
    i = 0;
    if ((ai->considered_ability.ai_flags.word & AI_ABILITY_CANCEL_STATUS)
        && (ai->considered_ability.parameters.fields.status_infliction[STATUS_BYTE_INDEX(STATUS_ID_DEAD)]
            & STATUS_BYTE_MASK(STATUS_ID_DEAD)))
        i = 1;
    if (decision->targeting_flags_2 & AI_TARGET_DEAD_WITH_RERAISE) {
        s32 ability_ct = ai->considered_ability.ct;
        if (ability_ct < ai_calculate_clockticks_until_unit_acts(unit))
            return AI_ABILITY_EFFECT_NONE;
        if (i)
            return AI_ABILITY_EFFECT_NONE;
    }
    if (decision->targeting_flags_2 & AI_TARGET_DEAD_WITHOUT_RERAISE) {
        s32 ability_ct = ai->considered_ability.ct;
        if (ai_calculate_clockticks_until_death_counter_expires(unit) < ability_ct)
            return AI_ABILITY_EFFECT_NONE;
        if (!i)
            return AI_ABILITY_EFFECT_NONE;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)] & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))
        && (!(ai->considered_ability.ai_flags.word & AI_ABILITY_CANCEL_STATUS)
            || !(ai->considered_ability.parameters.fields.status_infliction[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
                & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))))
        return AI_ABILITY_EFFECT_NONE;
    if (ai_is_status_active_through_delay(ct, unit, STATUS_ID_WALL)
        && (!(ai->considered_ability.ai_flags.word & AI_ABILITY_CANCEL_STATUS)
            || !(ai->considered_ability.parameters.fields.status_infliction[STATUS_BYTE_INDEX(STATUS_ID_WALL)]
                & STATUS_BYTE_MASK(STATUS_ID_WALL))))
        return AI_ABILITY_EFFECT_NONE;

    i = ai->considered_ability.element;
    if (i) {
        if (i == (i & unit->elemental_affinity[ELEMENTAL_AFFINITY_NULLIFY])
            || (i == 8
                && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
                    & STATUS_BYTE_MASK(STATUS_ID_FLOAT)))) {
            return AI_ABILITY_EFFECT_NONE;
        }
    }

    if (ai->considered_ability.ai_flags.word & AI_ABILITY_TARGET_ENEMIES) {
        if (ai->considered_ability.ai_flags.word & AI_ABILITY_UNDEAD_REVERSE) {
            if (decision->enemy_flag) {
                if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)] & STATUS_BYTE_MASK(STATUS_ID_UNDEAD))
                    return AI_ABILITY_EFFECT_ADVERSE;
            } else if (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)]
                           & STATUS_BYTE_MASK(STATUS_ID_UNDEAD))) {
                return AI_ABILITY_EFFECT_ADVERSE;
            }
        } else if (decision->enemy_flag) {
            if (unit->elemental_affinity[ELEMENTAL_AFFINITY_ABSORB] & ai->considered_ability.element)
                return AI_ABILITY_EFFECT_ADVERSE;
        } else {
            if (ai_is_status_active_through_delay(ct, unit, STATUS_ID_CHARM)
                && ai_can_status_cancellation_affect_turn(unit, STATUS_ID_CHARM))
                i = 1;
            else
                i = 0;
            if ((ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTS_HP)
                && g_main_action_menu_types_by_skillset[ai->considered_ability.skillset] != ACTION_MENU_TYPE_CHARGE) {
                if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CONFUSION)]
                    & STATUS_BYTE_MASK(STATUS_ID_CONFUSION))
                    return AI_ABILITY_EFFECT_USEFUL;
                if (ai_is_status_active_through_delay(ct, unit, STATUS_ID_SLEEP))
                    return AI_ABILITY_EFFECT_USEFUL;
                if (i)
                    return AI_ABILITY_EFFECT_USEFUL;
            }
            if ((unit->elemental_affinity[ELEMENTAL_AFFINITY_ABSORB] & ai->considered_ability.element)
                && (decision->targeting_flags_2 & AI_TARGET_HP_BELOW_HALF))
                return AI_ABILITY_EFFECT_USEFUL;
            if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CONFUSION)]
                    & (STATUS_BYTE_MASK(STATUS_ID_CONFUSION) | STATUS_BYTE_MASK(STATUS_ID_BLOOD_SUCK)))
                || i) {
                if (ai->considered_ability.ai_flags.word & AI_ABILITY_ADD_STATUS) {
                    /* Early acceptance of a disabling effect bypasses the generic
                     * already-present/immunity filters below. Status bytes 1..4
                     * are loaded together as an aligned 32-bit value by the target. */
                    s32 status_mask = ai->considered_ability.parameters.packed.status_tail
                        & (AI_STATUS_TAIL_PACKED_MASK(STATUS_ID_SLOW) | AI_STATUS_TAIL_PACKED_MASK(STATUS_ID_STOP)
                            | AI_STATUS_TAIL_PACKED_MASK(STATUS_ID_SLEEP)
                            | AI_STATUS_TAIL_PACKED_MASK(STATUS_ID_DONT_MOVE)
                            | AI_STATUS_TAIL_PACKED_MASK(STATUS_ID_DONT_ACT));
                    if (status_mask)
                        return AI_ABILITY_EFFECT_USEFUL;
                }
            }
            return AI_ABILITY_EFFECT_ADVERSE;
        }
        if ((ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTS_HP)
            && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEATH_SENTENCE)]
                & STATUS_BYTE_MASK(STATUS_ID_DEATH_SENTENCE)))
            return AI_ABILITY_EFFECT_USEFUL;
        if ((ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTS_MP) && unit->mp > decision->lowest_mp_cost)
            return AI_ABILITY_EFFECT_USEFUL;
    }

    if (ai->considered_ability.ai_flags.word & AI_ABILITY_TARGET_ALLIES) {
        if (ai->considered_ability.ai_flags.word & AI_ABILITY_UNDEAD_REVERSE) {
            if (decision->enemy_flag) {
                if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)] & STATUS_BYTE_MASK(STATUS_ID_UNDEAD))
                    return AI_ABILITY_EFFECT_USEFUL;
                return AI_ABILITY_EFFECT_ADVERSE;
            }
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)] & STATUS_BYTE_MASK(STATUS_ID_UNDEAD))
                return AI_ABILITY_EFFECT_ADVERSE;
        } else if (decision->enemy_flag)
            return AI_ABILITY_EFFECT_ADVERSE;
        /* Death Sentence: the Add Reraise shortcut likewise bypasses the
         * generic already-present/immunity filters. */
        if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEATH_SENTENCE)]
            & STATUS_BYTE_MASK(STATUS_ID_DEATH_SENTENCE)) {
            if ((ai->considered_ability.ai_flags.word & AI_ABILITY_ADD_STATUS)
                && (ai->considered_ability.parameters.fields.status_infliction[STATUS_BYTE_INDEX(STATUS_ID_RERAISE)]
                    & STATUS_BYTE_MASK(STATUS_ID_RERAISE)))
                return AI_ABILITY_EFFECT_USEFUL;
            return AI_ABILITY_EFFECT_NONE;
        }
        if ((ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTS_HP)
            && (decision->targeting_flags_2 & AI_TARGET_HP_BELOW_HALF))
            return AI_ABILITY_EFFECT_USEFUL;
        if ((ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTS_MP)
            && (decision->targeting_flags_2 & AI_TARGET_MP_CONSTRAINED))
            return AI_ABILITY_EFFECT_USEFUL;
    }

    if (!(ai->considered_ability.ai_flags.word & (AI_ABILITY_TARGET_ALLIES | AI_ABILITY_TARGET_ENEMIES))) {
        if (ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTS_HP)
            return AI_ABILITY_EFFECT_USEFUL;
        if (ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTS_MP)
            return AI_ABILITY_EFFECT_USEFUL;
    }
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_ADD_STATUS) {
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            mask = ai->considered_ability.parameters.fields.status_infliction[i];
            mask ^= mask & unit->status_sets.current[i];
            if (mask) {
                mask ^= mask & unit->status_sets.immunity[i];
                if (mask) {
                    switch (i) {
                    case 1:
                        if ((mask & STATUS_BYTE_MASK(STATUS_ID_SILENCE)) && !decision->silence_mod)
                            mask ^= STATUS_BYTE_MASK(STATUS_ID_SILENCE);
                        if ((mask & STATUS_BYTE_MASK(STATUS_ID_DARKNESS)) && decision->evade_mod < 2)
                            mask ^= STATUS_BYTE_MASK(STATUS_ID_DARKNESS);
                        break;
                    case 2:
                        if ((mask & STATUS_BYTE_MASK(STATUS_ID_RERAISE))
                            && !(decision->targeting_flags_2 & AI_TARGET_HP_BELOW_HALF))
                            mask ^= STATUS_BYTE_MASK(STATUS_ID_RERAISE);
                        if ((mask & STATUS_BYTE_MASK(STATUS_ID_FROG)) && !decision->enemy_flag)
                            mask ^= STATUS_BYTE_MASK(STATUS_ID_FROG);
                        if (!(decision->targeting_flags_2 & AI_TARGET_HP_BELOW_THREE_QUARTERS))
                            mask &= (u8)~STATUS_BYTE_MASK(STATUS_ID_FLOAT);
                        break;
                    case 3:
                        if ((mask & STATUS_BYTE_MASK(STATUS_ID_REGEN))
                            && !(decision->targeting_flags_2 & AI_TARGET_HP_BELOW_HALF))
                            mask ^= STATUS_BYTE_MASK(STATUS_ID_REGEN);
                        if (!(decision->targeting_flags_2 & AI_TARGET_HP_BELOW_THREE_QUARTERS))
                            mask &= (u8) ~(STATUS_BYTE_MASK(STATUS_ID_PROTECT) | STATUS_BYTE_MASK(STATUS_ID_SHELL)
                                | STATUS_BYTE_MASK(STATUS_ID_WALL));
                        if ((mask & (STATUS_BYTE_MASK(STATUS_ID_HASTE) | STATUS_BYTE_MASK(STATUS_ID_SLOW)))
                            && ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_STOP)]
                                    & STATUS_BYTE_MASK(STATUS_ID_STOP))
                                || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SLEEP)]
                                    & STATUS_BYTE_MASK(STATUS_ID_SLEEP))))
                            mask &= (u8) ~(STATUS_BYTE_MASK(STATUS_ID_HASTE) | STATUS_BYTE_MASK(STATUS_ID_SLOW));
                        break;
                    case 4:
                        if (!(decision->targeting_flags_2 & AI_TARGET_HP_BELOW_THREE_QUARTERS))
                            mask &= (u8) ~(STATUS_BYTE_MASK(STATUS_ID_FAITH) | STATUS_BYTE_MASK(STATUS_ID_REFLECT));
                        break;
                    }
                    if (mask)
                        return AI_ABILITY_EFFECT_USEFUL;
                }
            }
        }
    }
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_CANCEL_STATUS) {
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            mask = ai->considered_ability.parameters.fields.status_infliction[i];
            mask &= unit->status_sets.current[i];
            if (mask) {
                mask ^= mask & unit->status_sets.innate[i];
                if (mask) {
                    if (i >= 3) {
                        for (bit = 0; bit < 8; bit++) {
                            s32 flag = 0x80 >> bit;
                            if (mask & flag) {
                                status = i * 8 + bit;
                                if (!ai_is_status_active_through_delay(ct, unit, status)
                                    || !ai_can_status_cancellation_affect_turn(unit, status))
                                    mask ^= flag;
                            }
                        }
                    }
                    switch (i) {
                    case 1:
                        if ((mask & STATUS_BYTE_MASK(STATUS_ID_SILENCE)) && !decision->silence_mod)
                            mask ^= STATUS_BYTE_MASK(STATUS_ID_SILENCE);
                        if ((mask & STATUS_BYTE_MASK(STATUS_ID_DARKNESS)) && !decision->evade_mod)
                            mask ^= STATUS_BYTE_MASK(STATUS_ID_DARKNESS);
                        break;
                    case 2:
                        if ((mask & STATUS_BYTE_MASK(STATUS_ID_FROG)) && decision->enemy_flag)
                            mask ^= STATUS_BYTE_MASK(STATUS_ID_FROG);
                        break;
                    }
                    if (mask)
                        return AI_ABILITY_EFFECT_USEFUL;
                }
            }
        }
    }
    /* Late stat-effect gate: Praise, Accumulate, and Yell only.
     * Odd parity still falls through to the Unequip flag below. */
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTS_STATS) {
        ability_id = ai->considered_ability.ability_id;
        if (ability_id != ABILITY_ID_TALK_PRAISE && ability_id != ABILITY_ID_BASIC_SKILL_ACCUMULATE
            && ability_id != ABILITY_ID_GUTS_YELL)
            return AI_ABILITY_EFFECT_USEFUL;
        if (!(rand() & 1))
            return AI_ABILITY_EFFECT_USEFUL;
    }
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_UNEQUIP)
        return AI_ABILITY_EFFECT_USEFUL;
    return AI_ABILITY_EFFECT_NONE;
}

/*
 * Test whether a current status lasts through the requested charge time.
 *
 * Equality counts as present. Jump uses its charged-ability timer; other
 * timed statuses use their duration unless the status is innate.
 */
s32 ai_is_status_active_through_delay(s32 delay, const unit_stats_t* unit, s32 status_id) {
    s32 byte_off;

    byte_off = status_id / 8;

    if (!(unit->status_sets.current[byte_off] & (0x80 >> (status_id - byte_off * 8)))) {
        return 0;
    }
    if (status_id == STATUS_ID_JUMP) {
        if (unit->charged_ability_ct < delay) {
            return 0;
        }
    } else {
        if (status_id < STATUS_ID_POISON) {
            return 1;
        }
        if (unit->status_sets.innate[byte_off] & (0x80 >> (status_id - byte_off * 8))) {
            return 1;
        }
        if (unit->status_ct[STATUS_CT_INDEX(status_id)] < delay) {
            return 0;
        }
    }
    return 1;
}

/*
 * Reject removal of a control status that expires before the unit's next turn.
 *
 * Only Charm, Don't Move, and Don't Act are filtered; other IDs return true.
 * Equality counts as lasting through the turn. This does not test presence.
 */
s32 ai_can_status_cancellation_affect_turn(const unit_stats_t* unit, s32 status) {
    if (status == STATUS_ID_CHARM || (status - STATUS_ID_DONT_MOVE) < 2U) {
        if (ai_calculate_clockticks_until_unit_acts(unit) > unit->status_ct[STATUS_CT_INDEX(status)]) {
            return 0;
        }
    }
    return 1;
}

/* For an enemy-targeting ability, drop from the target list each enemy that
 * will act before the ability lands. Returns 1 while a target is left. */
s32 ai_exclude_enemies_acting_before_ct(void) {
    s32 result;
    s32 i;
    ai_data_t* ai;
    ai_data_t* walk;

    result = 0;
    ai = &g_ai_data_base;
    /* The target walks a pointer that starts at the block base and steps by
     * sizeof(ai_unit_decision_t), reaching unit_decisions[i].enemy_flag as a
     * 0x1834 displacement; a pointer started at unit_decisions adds an addiu. */
    for (i = 0, walk = ai; i < BATTLE_UNIT_SLOT_COUNT;
        i++, walk = (ai_data_t*)((u8*)walk + sizeof(ai_unit_decision_t))) {
        if (ai->targetability.live.unit_targetable[i] != 0) {
            if ((ai->considered_ability.ai_flags.word & AI_ABILITY_FLAG_1_TARGET_ENEMIES)
                && (walk->unit_decisions[0].enemy_flag != 0) && (ai_can_unit_act_within_charge_time(i) != 0)) {
                ai->targetability.live.unit_targetable[i] = 0;
            } else {
                result = 1;
            }
        }
    }
    return result;
}

/* 1 when the unit gets a turn by the considered ability's charge deadline
 * and can act by then; 0 for an instant ability. */
s32 ai_can_unit_act_within_charge_time(s32 unit_index) {
    s32 min_ct;
    u8 ct;

    ct = g_ai_data_base.considered_ability.ct;
    if (ct != 0) {
        min_ct = ai_calculate_clockticks_until_unit_acts(&g_battle_unit_stats[unit_index]);
        if (min_ct != 0x7FFFFFFF) {
            if (ct >= min_ct) {
                return ai_is_unit_active_after_delay(ct, unit_index);
            }
        }
    }
    return 0;
}

/* 1 when the unit can act limit clockticks from now: present, Speed above 0,
 * not crystal, petrified or treasure, with no Stop, Don't Move or Sleep that
 * lasts that long, and alive, or dead with Reraise and due a turn by then. */
s32 ai_is_unit_active_after_delay(s32 limit, s32 unit_id) {
    unit_stats_t* unit = &g_battle_unit_stats[unit_id];

    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return 0;
    }
    if (unit->attributes[UNIT_ATTRIBUTE_SPEED] == 0) {
        return 0;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))
        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)] & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))
        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
        return 0;
    }
    if (ai_is_status_active_through_delay(limit, unit, STATUS_ID_STOP) != 0) {
        return 0;
    }
    if (ai_is_status_active_through_delay(limit, unit, STATUS_ID_DONT_MOVE) != 0) {
        return 0;
    }
    if (ai_is_status_active_through_delay(limit, unit, STATUS_ID_SLEEP) != 0) {
        return 0;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD)) == 0) {
        return 1;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_RERAISE)] & STATUS_BYTE_MASK(STATUS_ID_RERAISE)) == 0
        || limit < ai_calculate_clockticks_until_unit_acts(unit)) {
        return 0;
    }
    return 1;
}

/* Position the temporary simulation unit at the supplied AI coordinates. */
void ai_move_temp_unit_to_coords(const ai_coords_t* coord) {
    g_ai_data_base.acting_unit->x = coord->fields.x;
    g_ai_data_base.acting_unit->position.y = coord->fields.y;
    g_ai_data_base.acting_unit->position.higher_elevation = coord->fields.elevation;
}

/* Pack a unit's tile as AI coordinates: x, y and the elevation bit (position
 * bit 15). */
void ai_copy_unit_coordinates(s32 unit_idx, ai_coords_t* dst) {
    dst->fields.x = g_battle_unit_stats[unit_idx].x;
    dst->fields.y = g_battle_unit_stats[unit_idx].position.y;
    dst->fields.elevation = g_battle_unit_stats[unit_idx].position.higher_elevation;
    dst->fields.zero = 0;
}
