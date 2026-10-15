/* Ai action selection. Its start is a cut chosen by topic; its end is a cut chosen by topic. */

#include "fft/battle_ai.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/memory.h"
#include "psx/types.h"

/*
 * Select an action for the recovery-oriented AI policy.
 *
 * Check urgent statuses before the designated-target, enemy and ally passes.
 * Return -1 to suspend and 0 when selection completes.
 */
s32 battle_ai_select_action_recovery(void) {
    ai_data_t* ai = &g_ai_data_base;
    unit_stats_t* acting = g_ai_data_base.acting_unit;
    u8 statuses[AI_RECOVERY_STATUS_COUNT];
    s32 target;
    u32 status;
    s32 result;

    memcpy(statuses, g_ai_recovery_status_ids, sizeof(statuses));
    target = acting->auto_battle_target;
    if (g_ai_data_base.resume_pending) {
        /* Resume: each phase re-enters the step that suspended. */
        switch (g_ai_data_base.action_selection_phase) {
        case AI_RECOVERY_SELECTION_SCORE_TILES:
            goto score_tiles;
        case AI_RECOVERY_SELECTION_MOVEMENT:
            goto movement;
        case AI_RECOVERY_SELECTION_CHARGING:
            goto charging;
        case AI_RECOVERY_SELECTION_TARGET_ABILITIES:
            goto target_abilities;
        case AI_RECOVERY_SELECTION_ENEMY_ABILITIES:
            goto enemy_abilities;
        case AI_RECOVERY_SELECTION_CANCEL_STATUS:
            goto cancel_status;
        default:
            goto ally_abilities;
        }
    }
    g_ai_data_base.movement_scenario = 0;
score_tiles:
    if (battle_ai_score_reachable_tiles() == -1) {
        ai->action_selection_phase = AI_RECOVERY_SELECTION_SCORE_TILES;
        return -1;
    }
    g_ai_recovery_status_retry = 0;
    if (((ai->unit_decisions[target].targeting_flags_2 >> 4) & (AI_TARGET_UNTARGETABLE >> 4))
        || ai->unit_decisions[target].enemy_flag) {
    retarget:
        battle_ai_select_peril_target();
        target = ai->acting_unit->auto_battle_target;
    }
    battle_ai_focus_on_target(target);
movement:
    if (battle_ai_build_targeted_movement() == -1) {
        ai->action_selection_phase = AI_RECOVERY_SELECTION_MOVEMENT;
        return -1;
    }
    ai->movement_scenario = 2;
    battle_ai_invert_target_priority();
    ai->movement_scenario = 0;
    if (!g_ai_recovery_status_retry
        && !((ai->unit_decisions[target].targeting_flags_2 >> 6) & (AI_TARGET_DEAD_WITH_RERAISE >> 6))) {
        g_ai_recovery_status_index = 0;
        do {
            u32 status_flags
                = g_battle_unit_stats[target].status_sets.current[(status = statuses[g_ai_recovery_status_index]) >> 3];
            if (status_flags & (0x80 >> (status & 7))) {
            cancel_status:
                result = battle_ai_evaluate_status_cancellation(target, statuses[g_ai_recovery_status_index]);
                if (result == -1) {
                    ai->action_selection_phase = AI_RECOVERY_SELECTION_CANCEL_STATUS;
                    return -1;
                }
                if (result == 2)
                    return 0;
                if (result == 0) {
                    /* Retry target selection once, then skip further
                     * status-cancel attempts; the retry re-enters the
                     * retarget block above. */
                    g_ai_recovery_status_retry = 1;
                    goto retarget;
                }
            }
            g_ai_recovery_status_index++;
        } while (g_ai_recovery_status_index < AI_RECOVERY_STATUS_COUNT);
    }
    ai->movement_scenario = 1;
charging:
    result = battle_ai_evaluate_charging_movement();
    switch (result) {
    case -1:
        ai->action_selection_phase = AI_RECOVERY_SELECTION_CHARGING;
        return -1;
    case 1:
        return 0;
    }
    ai->movement_scenario = 0;
    if (battle_ai_select_active_units(AI_UNIT_FILTER_SPECIFIC, target)) {
    target_abilities:
        if (battle_ai_evaluate_ability_list() == -1) {
            ai->action_selection_phase = AI_RECOVERY_SELECTION_TARGET_ABILITIES;
            return -1;
        }
        if (battle_ai_select_action_ranked())
            return 0;
    }
    ai->movement_scenario = 1;
    if (battle_ai_select_active_units(AI_UNIT_FILTER_ENEMIES, 0)) {
    enemy_abilities:
        if (battle_ai_evaluate_ability_list() == -1) {
            ai->action_selection_phase = AI_RECOVERY_SELECTION_ENEMY_ABILITIES;
            return -1;
        }
        if (battle_ai_select_action_ranked())
            return 0;
    }
    if (battle_ai_select_active_units(AI_UNIT_FILTER_ALLIES, 0)) {
    ally_abilities:
        if (battle_ai_evaluate_ability_list() != -1) {
            if (battle_ai_select_action_ranked())
                return 0;
        } else {
            ai->action_selection_phase = AI_RECOVERY_SELECTION_ALLY_ABILITIES;
            return -1;
        }
    }
    ai->movement_scenario = 2;
    battle_ai_select_action_fallback();
    return 0;
}

/* Evaluate the actor's abilities for removing one status from a target.
 *
 * The return value distinguishes suspension (-1), no usable ability (0), a
 * retry-worthy result (1), and a selected action (2). The Blood Suck path
 * deliberately tests the transient Jump flag, preserving the shipped AI
 * quirk.
 */
s32 battle_ai_evaluate_status_cancellation(s32 unit_id, s32 status_id) {
    ai_data_t* ai = &g_ai_data_base;
    unit_stats_t* unit = &g_battle_unit_stats[unit_id];
    ai_unit_decision_t* decision;
    ai_ability_entry_t* entry;
    s32 flags;
    s32 status_byte;

    if (g_ai_data_base.resume_pending != 0) {
        /* Resume: the target jumps into the middle of the ability loop. */
        goto evaluate_ability;
    }

    decision = &ai->unit_decisions[unit_id];
    g_ai_status_candidate_count = 0;
    g_ai_status_insufficient_mp = 0;
    g_ai_status_sufficient_mp = 0;

    switch (status_id) {
    case STATUS_ID_DEAD:
        if (!(decision->flags & AI_DECISION_TRANSIENT_DEAD))
            return 1;
        break;
    case STATUS_ID_PETRIFY:
        if (!(decision->flags & AI_DECISION_TRANSIENT_PETRIFIED))
            return 1;
        break;
    case STATUS_ID_BLOOD_SUCK:
        if (!(decision->flags & AI_DECISION_TRANSIENT_JUMP))
            return 1;
        break;
    }

    if (battle_ai_select_active_units(AI_UNIT_FILTER_SPECIFIC, unit_id) != 0) {
        for (ai->ability_counter = 0; ai->ability_counter < AI_ABILITY_LIST_COUNT; ai->ability_counter++) {
            entry = &ai->ability_lists[ai->acting_unit_compact_index][ai->ability_counter];
            if (entry->skillset == SKILLSET_ID_NONE)
                break;
            battle_ai_load_ability_entry(entry);
            flags = ai->considered_ability.ai_flags.word;
            if (!(flags & AI_ABILITY_CANCEL_STATUS))
                continue;

            status_byte = status_id / 8;
            if (!(ai->considered_ability.parameters.fields.status_infliction[status_byte] & (0x80 >> (status_id % 8))))
                continue;
            if ((flags & AI_ABILITY_AFFECTED_BY_FAITH)
                && (unit->status_sets.innate[STATUS_BYTE_INDEX(STATUS_ID_INNOCENT)]
                    & STATUS_BYTE_MASK(STATUS_ID_INNOCENT)))
                continue;
            if ((flags & AI_ABILITY_REFLECTABLE)
                && (unit->status_sets.innate[STATUS_BYTE_INDEX(STATUS_ID_REFLECT)]
                    & STATUS_BYTE_MASK(STATUS_ID_REFLECT)))
                continue;
            if ((flags & AI_ABILITY_AFFECTED_BY_SILENCE)
                && (ai->acting_unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SILENCE)]
                    & STATUS_BYTE_MASK(STATUS_ID_SILENCE)))
                continue;

            if (ai->acting_unit->mp < ai->considered_ability.mp_cost)
                g_ai_status_insufficient_mp = 1;
            else
                g_ai_status_sufficient_mp = 1;
            g_ai_status_candidate_count++;

        evaluate_ability:
            if (battle_ai_evaluate_ability_outcome() == -1)
                return -1;
        }
    }

    if (ai->ranked_actions[0].rank.base_hit_percent != 0) {
        battle_mem_copy_u16_values(&ai->selected_action, &ai->ranked_actions[0], sizeof(ai_action_data_t));
        return 2;
    }
    if (g_ai_status_candidate_count == 0)
        return 0;
    if (g_ai_status_insufficient_mp != 0 && ai->has_mp_recovery_ability == 0 && g_ai_status_sufficient_mp == 0)
        return 0;
    return 1;
}

/*
 * Select the allied target with the highest recovery-priority score.
 *
 * Keep the actor as fallback and retain the earlier slot on equal scores.
 * Status-helper checks require the actor's aggregate cancellation capability;
 * the HP-recovery flag enables dead-with-Reraise, Critical and low-HP scoring.
 * Return 1 only when the best signed score exceeds 0x02000000.
 */
s32 battle_ai_select_peril_target(void) {
    ai_data_t* ai = &g_ai_data_base;
    s32 best_score = 0;
    s32 i;

    g_ai_data_base.acting_unit->auto_battle_target = g_ai_data_base.acting_unit_id;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit_stats_t* unit = &g_battle_unit_stats[i];
        ai_unit_decision_t* decision = &ai->unit_decisions[i];
        s32 distance_score, missing_hp, team_bits, status_score, score;

        if (decision->targeting_flags_2 & AI_TARGET_UNTARGETABLE) {
            continue;
        }
        if (decision->enemy_flag) {
            continue;
        }
        distance_score = 255 - battle_ai_calculate_distance_between_units(ai->acting_unit, unit);
        missing_hp = 128 - ((unit->hp << 7) / unit->max_hp);
        team_bits = unit->team_flags & BATTLE_TEAM_IMPORTANCE_MASK;
        if (battle_ai_can_cancel_current_status(unit, STATUS_ID_DEAD)
            && (decision->targeting_flags_2 & AI_TARGET_DEAD_WITHOUT_RERAISE)) {
            status_score = 0x78;
        } else if ((ai->acting_unit_decision->targeting_flags_1 & AI_TARGETING_FLAG_1_CAN_HEAL_ALLIES)
            && ((decision->targeting_flags_2 & AI_TARGET_DEAD_WITH_RERAISE)
                || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRITICAL)]
                    & STATUS_BYTE_MASK(STATUS_ID_CRITICAL)))) {
            status_score = 0x77;
        } else if (battle_ai_can_cancel_current_status(unit, STATUS_ID_BLOOD_SUCK)) {
            status_score = 0x76;
        } else if (battle_ai_can_cancel_current_status(unit, STATUS_ID_PETRIFY)) {
            status_score = 0x75;
        } else if (battle_ai_can_cancel_current_status(unit, STATUS_ID_CONFUSION)) {
            status_score = 0x74;
        } else if (battle_ai_can_cancel_current_status(unit, STATUS_ID_FROG)) {
            status_score = 0x73;
        } else {
            status_score = 0;
            if (ai->acting_unit_decision->targeting_flags_1 & AI_TARGETING_FLAG_1_CAN_HEAL_ALLIES) {
                status_score = -(decision->targeting_flags_2 & AI_TARGET_HP_BELOW_HALF) & 0x72;
            }
        }
        if (status_score == 0) {
            if (decision->targeting_flags_1 & AI_TARGETING_FLAG_1_MOSTLY_SUPPORT) {
                continue;
            }
            if (ai->acting_unit_id == i) {
                distance_score = 0;
            }
            score = (missing_hp << 16) + (team_bits << 8) + distance_score;
        } else {
            score = (status_score << 24) + (distance_score << 16) + (missing_hp << 8) + team_bits;
        }
        if (best_score < score) {
            best_score = score;
            ai->acting_unit->auto_battle_target = i;
        }
    }
    /* Clear the unit's special targeting behavior; this does not impose a distance limit. */
    ai->acting_unit_decision->flags &= ~AI_DECISION_SPECIAL_BEHAVIOR;
    return best_score > 0x02000000;
}

/*
 * Test whether the actor's available abilities can remove this current status.
 *
 * Innate statuses are excluded. The cancellation mask aggregates the actor's
 * usable ability list; it does not describe only the considered ability.
 */
s32 battle_ai_can_cancel_current_status(const unit_stats_t* unit, s32 status_bit) {
    ai_data_t* ai = &g_ai_data_base;
    s32 byte_off;
    s32 bit;
    s32 mask;

    byte_off = status_bit / 8;
    bit = status_bit - byte_off * 8;
    mask = 0x80;

    if (!(unit->status_sets.current[byte_off] & (mask >> bit))) {
        return 0;
    }
    if (unit->status_sets.innate[byte_off] & (mask >> bit)) {
        return 0;
    }
    if (!(ai->status_to_cancel[byte_off] & (mask >> bit))) {
        return 0;
    }
    return 1;
}

/* With an ally in peril, switch to the recovery policy and return -2.
 * Otherwise focus on the unit's auto-battle target and run the general search;
 * -1 suspends. */
s32 battle_ai_select_action_fading_life(void) {
    if (g_ai_data_base.resume_pending == 0) {
        if (battle_ai_select_peril_target() != 0) {
            g_ai_data_base.autobattle_setting = AI_AUTOBATTLE_RECOVERY;
            return -2;
        }
        battle_ai_focus_on_target(g_ai_data_base.acting_unit->auto_battle_target);
    }
    if (battle_ai_select_action_general() == -1)
        return -1;
    return 0;
}

/*
 * Select an action for the retreat-oriented AI path.
 *
 * Try abilities targeting the actor and an existing charge before the broader
 * ability pass. Return -1 to suspend and 0 when selection completes.
 */
s32 battle_ai_select_action_retreat(void) {
    ai_data_t* ai = &g_ai_data_base;
    ai_ability_entry_t* entry;
    s32 i, result;

    if (g_ai_data_base.resume_pending) {
        /* Resume: re-enter the step that suspended. */
        switch (g_ai_retreat_stage) {
        case AI_RETREAT_MOVEMENT:
            goto movement;
        case AI_RETREAT_SELF_ABILITIES:
            goto self_abilities;
        case AI_RETREAT_CHARGING:
            goto charging;
        default:
            goto other_abilities;
        }
    }
    battle_ai_focus_on_target(battle_ai_find_nearest_target(AI_NEAREST_HEALER_OR_CRYSTAL));
movement:
    if (battle_ai_run_cowardly_movement() == -1) {
        g_ai_retreat_stage = AI_RETREAT_MOVEMENT;
        return -1;
    }
    ai->movement_scenario = 1;
    battle_ai_invert_target_priority();
    if (battle_ai_select_active_units(AI_UNIT_FILTER_SPECIFIC, ai->acting_unit_id)) {
    self_abilities:
        if (battle_ai_evaluate_ability_list() == -1) {
            g_ai_retreat_stage = AI_RETREAT_SELF_ABILITIES;
            return -1;
        }
    }
    if (battle_ai_select_action_ranked())
        return 0;
charging:
    result = battle_ai_evaluate_charging_movement();
    switch (result) {
    case -1:
        g_ai_retreat_stage = AI_RETREAT_CHARGING;
        return -1;
    case 1:
        return 0;
    }
    for (i = 0; i < AI_ABILITY_LIST_COUNT; i++) {
        entry = &ai->ability_lists[ai->acting_unit_compact_index][i];
        if (entry->skillset == SKILLSET_ID_NONE)
            break;
        battle_ai_load_ability_entry(entry);
        /* The aligned skillset/usage pair preserves the target's LHU/SH. */
        if (g_main_action_menu_types_by_skillset[entry->skillset] == ACTION_MENU_TYPE_CHARGE)
            entry->usage.policy.usable = 0;
    }
    if (battle_ai_select_active_units(AI_UNIT_FILTER_ANY, 255)) {
    other_abilities:
        if (battle_ai_evaluate_ability_list() == -1) {
            g_ai_retreat_stage = AI_RETREAT_OTHER_ABILITIES;
            return -1;
        }
        if (battle_ai_select_action_ranked())
            return 0;
    }
    battle_ai_select_action_fallback();
    return 0;
}

/* Score the reachable tiles, fill the distance grid for the decision's target,
 * and store the best destination as movement scenario 1. -1 suspends; the next
 * call resumes at the step that did. */
s32 battle_ai_run_cowardly_movement(void) {
    ai_data_t* ai = &g_ai_data_base;
    ai_cowardly_movement_stage_e state;

    if (g_ai_data_base.resume_pending != 0) {
        /* Resume: re-enter the step that suspended. */
        state = g_ai_cowardly_movement_stage;
        if (state == AI_COWARDLY_MOVEMENT_SCORE_TILES) {
            goto score_tiles;
        }
        if (state == AI_COWARDLY_MOVEMENT_TARGET_DISTANCE) {
            goto fill_target_distance;
        }
    }
    g_ai_data_base.movement_scenario = 0;
score_tiles:
    if (battle_ai_score_reachable_tiles() == -1) {
        g_ai_cowardly_movement_stage = AI_COWARDLY_MOVEMENT_SCORE_TILES;
        return -1;
    }
fill_target_distance:
    if (battle_ai_fill_target_distance_grid(&ai->acting_unit_decision->target) == -1) {
        g_ai_cowardly_movement_stage = AI_COWARDLY_MOVEMENT_TARGET_DISTANCE;
        return -1;
    }
    battle_ai_select_destination(0xff, AI_DESTINATION_PRIORITY_FIRST);
    ai->movement_scenario = 1;
    ai->candidate_coords[1] = ai->candidate_coords[0];
    battle_ai_set_candidate_and_actor_reachable_tiles();
    return 0;
}

/*
 * Select the nearest unit admitted by the requested target filter.
 *
 * Manhattan-distance ties keep the first slot. If none qualify, return the
 * acting unit's ID. Mode 5 admits crystals before ordinary targetability
 * checks, while mode 4 shares mode 0's Blood Suck exclusion.
 */
s32 battle_ai_find_nearest_target(ai_nearest_target_mode_e mode) {
    ai_data_t* ai = &g_ai_data_base;
    s32 nearest_distance = 0x7fffffff;
    s32 nearest_id = g_ai_data_base.acting_unit_id;
    s32 i;
    unit_stats_t* unit;
    ai_unit_decision_t* decision;
    s32 distance;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        decision = &ai->unit_decisions[i];
        /* Crystals always qualify as healer-mode destinations. */
        if (mode != AI_NEAREST_HEALER_OR_CRYSTAL
            || !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
                & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))) {
            if (battle_ai_is_unit_untargetable(i))
                continue;
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)] & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))
                continue;
            if (mode != AI_NEAREST_ELIGIBLE_ENEMY
                && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD)))
                continue;
            switch (mode) {
            case AI_NEAREST_HEALER_OR_CRYSTAL:
                if (decision->enemy_flag || !(decision->targeting_flags_1 & AI_TARGETING_FLAG_1_CAN_HEAL_ALLIES))
                    continue;
                break;
            case AI_NEAREST_IMMINENT_ENEMY:
                if (!ai->targetability.live.unit_targetable[i])
                    continue;
                if (battle_ai_should_exclude_from_imminent_enemies(i))
                    continue;
                if (battle_ai_calculate_distance_between_units(ai->acting_unit, unit) < 2
                    && battle_ai_calculate_height_difference_between_units(&ai->acting_unit_coords, unit) < -2)
                    continue;
                break;
            case AI_NEAREST_ENEMY:
                if (!decision->enemy_flag)
                    continue;
                break;
            case AI_NEAREST_ELIGIBLE_ENEMY:
                if (!decision->enemy_flag || (decision->targeting_flags_2 & AI_TARGET_DEAD_WITHOUT_RERAISE))
                    continue;
                if (!ai->targetability.live.unit_targetable[i] || !(decision->flags & AI_DECISION_CONSIDERED))
                    continue;
            case AI_NEAREST_WITHOUT_BLOOD_SUCK:
                if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_BLOOD_SUCK)]
                    & STATUS_BYTE_MASK(STATUS_ID_BLOOD_SUCK))
                    continue;
                break;
            }
        }
        distance = battle_ai_calculate_distance_between_units(ai->acting_unit, unit);
        if (distance < nearest_distance) {
            nearest_distance = distance;
            nearest_id = i;
        }
    }
    return nearest_id;
}

/* Distance in tiles along the map axes (|dx| + |dy|), ignoring height. */
s32 battle_ai_calculate_distance_between_units(const unit_stats_t* first, const unit_stats_t* second) {
    s32 dx = first->x - second->x;
    s32 dy = first->position.y - second->position.y;
    if (dx < 0)
        dx = -dx;
    if (dy < 0)
        dy = -dy;
    return dx + dy;
}

/*
 * Make only the candidate and acting tiles reachable in the selected scenario.
 *
 * Clear both 18-row levels before setting the candidate bit and adding the
 * acting bit. Integer address stages preserve the target's addition order;
 * direct typed indexing introduces extra instructions. Field offsets come
 * from the shared workspace declaration.
 */
void battle_ai_set_candidate_and_actor_reachable_tiles(void) {
    ai_data_t* ai = &g_ai_data_base;
    s32 row;

    for (row = 0; row < AI_GRID_ROW_COUNT; row++) {
        u32 row_address = row * sizeof(u16);
        u32 scenario_address = (u32)ai + ai->movement_scenario * sizeof(ai->reachable_tiles[0]);
        *(u16*)(row_address + scenario_address + ((u32)&ai->reachable_tiles[0][0][0] - (u32)ai)) = 0;
        scenario_address = (u32)ai + ai->movement_scenario * sizeof(ai->reachable_tiles[0]);
        row_address += scenario_address;
        *(u16*)(row_address + ((u32)&ai->reachable_tiles[0][1][0] - (u32)ai)) = 0;
    }
    {
        u32 scenario_address = ai->movement_scenario * sizeof(ai->reachable_tiles[0]) + (u32)ai;
        u32 coords_address = (u32)ai + ai->movement_scenario * sizeof(ai->candidate_coords[0]);
        u32 level_address;
        u32 row_address;
        scenario_address += (u32)&ai->reachable_tiles - (u32)ai;
        level_address = *(u8*)(coords_address + ((u32)&ai->candidate_coords[0].fields.elevation - (u32)ai))
                * sizeof(ai->reachable_tiles[0][0])
            + scenario_address;
        row_address = *(u8*)(coords_address + ((u32)&ai->candidate_coords[0].fields.y - (u32)ai))
                * sizeof(ai->reachable_tiles[0][0][0])
            + level_address;
        *(u16*)row_address = 0x8000 >> *(u8*)(coords_address + ((u32)&ai->candidate_coords[0].fields.x - (u32)ai));
    }
    {
        u32 scenario_address = ai->movement_scenario * sizeof(ai->reachable_tiles[0]) + (u32)ai;
        u32 level_address;
        u32 row_address;
        scenario_address += (u32)&ai->reachable_tiles - (u32)ai;
        level_address = ai->acting_unit_coords.fields.elevation * sizeof(ai->reachable_tiles[0][0]) + scenario_address;
        row_address = ai->acting_unit_coords.fields.y * sizeof(ai->reachable_tiles[0][0][0]) + level_address;
        *(u16*)row_address |= 0x8000 >> ai->acting_unit_coords.fields.x;
    }
}

/* Reset the selected action to the candidate coordinates for the current
 * movement scenario; the Defend skillset marks a Defend when the unit has it. */
void battle_ai_select_action_fallback(void) {
    u8 scenario = g_ai_data_base.movement_scenario;
    unit_stats_t* unit = g_ai_data_base.acting_unit;
    u32 coords;
    s32 support;

    g_ai_data_base.selected_action.act_before_move = 0;
    coords = g_ai_data_base.candidate_coords[scenario].word;
    support = unit->abilities.support;
    g_ai_data_base.selected_action.coords.word = coords;
    if (support == ABILITY_ID_SUPPORT_DEFEND) {
        g_ai_data_base.selected_action.command.skillset_id = SKILLSET_ID_DEFEND;
    } else {
        g_ai_data_base.selected_action.command.skillset_id = 0;
    }
}

/*
 * Fill target-distance estimates for rows containing reachable tiles.
 *
 * Unmounted Teleport or Teleport 2 uses Manhattan distance; other movement subtracts
 * propagated remaining-range values. Fill every map-width tile in each
 * selected row, leaving zero-mask rows unchanged. Return -1 when the child
 * suspends through its out-parameter, or 0 after filling the grid.
 */
s32 battle_ai_fill_target_distance_grid(const ai_coords_t* target) {
    ai_data_t* ai = &g_ai_data_base;
    s32 suspended = 0;
    s32 use_manhattan;
    s32 target_remaining_range;
    s32 level, x, y;
    s32 distance, y_distance;

    if (g_ai_data_base.resume_pending != 0)
        g_ai_data_base.search_resume_flag = 0;
    else
        g_ai_data_base.search_resume_flag = 1;
    use_manhattan = 0;
    if (!(ai->acting_unit->mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER)
        && (ai->acting_unit->ability_flags.movement[1]
            & (BATTLE_MOVEMENT_SET_2_TELEPORT | BATTLE_MOVEMENT_SET_2_TELEPORT_2))) {
        use_manhattan = 1;
    } else {
        battle_ai_propagate_target_movement(ai->acting_unit_id, target->fields.x, target->fields.y,
            target->fields.elevation, ai->search_resume_flag, &suspended, 1);
        if (suspended != 0)
            return -1;
        ai->resume_pending = 0;
        target_remaining_range = g_target_panels[(target->fields.elevation * MAP_TILES_PER_LAYER)
            + target->fields.y * ai->map_max_x + target->fields.x]
                                     .remaining_range;
    }
    for (level = 0; level < MAP_TERRAIN_LAYER_COUNT; level++) {
        for (y = 0; y < ai->map_max_y; y++) {
            if (ai->reachable_tiles[0][level][y] != 0) {
                for (x = 0; x < ai->map_max_x; x++) {
                    if (use_manhattan) {
                        distance = target->fields.x - x;
                        if (distance < 0)
                            distance = -distance;
                        y_distance = target->fields.y - y;
                        if (y_distance < 0)
                            y_distance = -y_distance;
                        distance += y_distance;
                    } else {
                        distance = target_remaining_range
                            - g_target_panels[(level * MAP_TILES_PER_LAYER) + y * ai->map_max_x + x].remaining_range;
                    }
                    ai->tile_target_distance[level][y][x] = distance;
                }
            }
        }
    }
    return 0;
}

/*
 * Score reachable tiles by nearby eligible enemies.
 *
 * Each enemy contributes max(10 - Manhattan distance, 0), ignoring height.
 * Only scenario-0 reachable tiles are updated; other cells remain unchanged.
 * The shared targetability list is rebuilt before scoring both map layers.
 */
void battle_ai_fill_foe_proximity_grid(void) {
    ai_data_t* ai = &g_ai_data_base;
    s32 i;
    s32 level;
    s32 y;
    s32 x;
    s32 distance;
    s32 dy;
    u16 row;
    unit_stats_t* unit;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        ai->targetability.live.unit_targetable[i] = 0;
        if (battle_ai_check_target_type(i) == 0 && battle_ai_should_exclude_from_imminent_enemies(i) == 0) {
            ai->targetability.live.unit_targetable[i] = 1;
        }
    }
    for (level = 0; level < MAP_TERRAIN_LAYER_COUNT; level++) {
        unit_stats_t* units = g_battle_unit_stats;
        s32 proximity_limit = 10;
        for (y = 0; y < ai->map_max_y; y++) {
            row = ai->reachable_tiles[0][level][y];
            if (row == 0) {
                continue;
            }
            for (x = 0; x < ai->map_max_x; x++) {
                if (!((row << x) & 0x8000)) {
                    continue;
                }
                ai->tile_foe_proximity[level][y][x] = 0;
                for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                    unit = &units[i];
                    if (ai->targetability.live.unit_targetable[i] == 0) {
                        continue;
                    }
                    distance = unit->x - x;
                    if (distance < 0) {
                        distance = -distance;
                    }
                    dy = unit->position.y - y;
                    if (dy < 0) {
                        dy = -dy;
                    }
                    distance += dy;
                    if (distance < 11) {
                        distance = proximity_limit - distance;
                        ai->tile_foe_proximity[level][y][x] += distance;
                    }
                }
            }
        }
    }
}

/* Rejects a unit from the AI's imminent-enemy set.
 *
 * The unit must be an enemy that acts no later than the acting unit and will
 * no longer be disabled by Stop, Don't Act, Sleep, Chicken, or Charm then. */
s32 battle_ai_should_exclude_from_imminent_enemies(s32 unit_id) {
    ai_data_t* ai = &g_ai_data_base;
    unit_stats_t* unit;
    s32 limit;

    if (g_ai_data_base.unit_decisions[unit_id].enemy_flag == 0)
        return 1;
    unit = &g_battle_unit_stats[unit_id];
    limit = ai->acting_unit_remaining_clockticks;
    if (limit < battle_ai_calculate_clockticks_until_unit_acts(unit)
        || battle_ai_is_status_active_through_delay(ai->acting_unit_remaining_clockticks, unit, STATUS_ID_STOP)
        || battle_ai_is_status_active_through_delay(ai->acting_unit_remaining_clockticks, unit, STATUS_ID_DONT_ACT)
        || battle_ai_is_status_active_through_delay(ai->acting_unit_remaining_clockticks, unit, STATUS_ID_SLEEP)
        || battle_ai_is_status_active_through_delay(ai->acting_unit_remaining_clockticks, unit, STATUS_ID_CHICKEN))
        return 1;
    return battle_ai_is_status_active_through_delay(ai->acting_unit_remaining_clockticks, unit, STATUS_ID_CHARM) != 0;
}

/*
 * Score each reachable tile, resuming suspended evaluations.
 *
 * Return -1 to suspend and 0 after restoring the simulation coordinates.
 * Integer address stages preserve the target's addition order; direct typed
 * indexing changes the machine code. Each level has 18 rows of 16 tiles.
 */
s32 battle_ai_score_reachable_tiles(void) {
    s32 x, y, level;
    ai_data_t* ai = &g_ai_data_base;
    u32 row;

    if (g_ai_data_base.resume_pending != 0) {
        x = g_ai_data_base.current_action.coords.fields.x;
        y = g_ai_data_base.current_action.coords.fields.y;
        level = g_ai_data_base.current_action.coords.fields.elevation;
        {
            u32 scenario_address = g_ai_data_base.movement_scenario * sizeof(ai->reachable_tiles[0]) + (u32)ai;
            u32 level_address;
            u32 row_address;
            scenario_address += (u32)&ai->reachable_tiles - (u32)ai;
            level_address = level * sizeof(ai->reachable_tiles[0][0]) + scenario_address;
            row_address = y * sizeof(ai->reachable_tiles[0][0][0]) + level_address;
            row = *(u16*)row_address;
        }
        /* This tile was already set up before the interrupted evaluation. */
        goto evaluate_tile;
    }
    level = 0;
    g_ai_data_base.current_action.act_before_move = 0;
    g_ai_data_base.current_action.command.skillset_id = 0;
    for (; level < MAP_TERRAIN_LAYER_COUNT; level++) {
        for (y = 0; y < ai->map_max_y; y++) {
            {
                u32 scenario_address = ai->movement_scenario * sizeof(ai->reachable_tiles[0]) + (u32)ai;
                u32 level_address;
                u32 row_address;
                scenario_address += (u32)&ai->reachable_tiles - (u32)ai;
                level_address = level * sizeof(ai->reachable_tiles[0][0]) + scenario_address;
                row_address = y * sizeof(ai->reachable_tiles[0][0][0]) + level_address;
                row = *(u16*)row_address;
            }
            if (row != 0) {
                for (x = 0; x < ai->map_max_x; x++) {
                    if ((row << x) & 0x8000) {
                        ai->current_action.coords.fields.x = x;
                        ai->current_action.coords.fields.y = y;
                        ai->current_action.coords.fields.elevation = level;
                        battle_ai_update_tile_pickup();
                    evaluate_tile:
                        if (battle_ai_simulate_action_and_score() == -1) {
                            /* Keep the current coordinates and uncommitted score. */
                            return -1;
                        }
                        {
                            u32 level_address = level * sizeof(ai->tile_priority[0]) + (u32)ai;
                            u32 row_address;
                            u32 tile_address;
                            level_address += (u32)&ai->tile_priority - (u32)ai;
                            row_address = y * sizeof(ai->tile_priority[0][0]) + level_address;
                            tile_address = x * sizeof(ai->tile_priority[0][0][0]) + row_address;
                            *(u16*)tile_address = -ai->current_action.rank.priority;
                        }
                    }
                }
            }
        }
    }
    /* Restore the simulation unit's coordinates only after the whole scan completes. */
    battle_ai_move_temp_unit_to_coords(&ai->acting_unit_coords);
    return 0;
}

/* Clockticks until the unit gets a turn: its Stop CT plus (100 - CT) / Speed
 * rounded up, and no less than its Sleep CT while it sleeps. 0x7fffffff when
 * it never will: Speed 0, crystal, petrified, treasure, or dead without
 * Reraise (from its decision record). Don't Act and Don't Move are not looked
 * at. */
s32 battle_ai_calculate_clockticks_until_unit_acts(const unit_stats_t* unit) {
    ai_data_t* ai;
    s32 speed;
    s32 ticks;
    s32 rem;

    ai = &g_ai_data_base;
    speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
    ticks = 0;
    if (speed == 0) {
        return 0x7FFFFFFF;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))
        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)] & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))
        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
        return 0x7FFFFFFF;
    }
    if ((ai->unit_decisions[unit->entd_slot].targeting_flags_2 >> 5) & (AI_TARGET_DEAD_WITHOUT_RERAISE >> 5)) {
        return 0x7FFFFFFF;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_STOP)] & STATUS_BYTE_MASK(STATUS_ID_STOP)) {
        ticks = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_STOP)];
    }
    if (unit->ct < UNIT_TURN_CT_THRESHOLD) {
        rem = UNIT_TURN_CT_THRESHOLD - unit->ct;
        ticks += rem / speed;
        if (rem % speed != 0) {
            ticks += 1;
        }
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD)) {
        return ticks;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SLEEP)] & STATUS_BYTE_MASK(STATUS_ID_SLEEP)) == 0) {
        return ticks;
    }
    if (ticks < unit->status_ct[STATUS_CT_INDEX(STATUS_ID_SLEEP)]) {
        ticks = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_SLEEP)];
    }
    return ticks;
}

/*
 * Predict the clockticks remaining before the death counter expires.
 *
 * Divide (counter + 1) * 100 - CT by speed, adding one for any remainder.
 * Counter zero lasts another turn; expiration may revive an Undead unit.
 * Callers reject an ability only when its CT is greater than this result.
 * The final two parameters are overwritten before use. Keeping them as
 * argument-register scratch reproduces the target's a1/a2 allocation; callers
 * supply only the unit pointer.
 */
s32 battle_ai_calculate_clockticks_until_death_counter_expires(unit_stats_t* unit, s32 total, s32 speed) {
    s32 counter;
    s32 ticks;
    s32 sum;

    speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
    if (speed == 0)
        return 0x7fffffff;
    if (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD)))
        return 0x7fffffff;
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_RERAISE)] & STATUS_BYTE_MASK(STATUS_ID_RERAISE))
        return 0x7fffffff;
    counter = unit->death_counter;
    if (counter == 0xff)
        return 0x7fffffff;
    total = counter;
    total *= UNIT_TURN_CT_THRESHOLD;
    sum = total + UNIT_TURN_CT_THRESHOLD;
    total = sum - unit->ct;
    ticks = total / speed;
    if (total % speed)
        ticks++;
    return ticks;
}

/*
 * Initialize AI workspace defaults and present-unit indexing.
 *
 * Average maximum HP includes all present slots and is capped at 255.
 */
void battle_ai_init_workspace(void) {
    s32 i = 0;
    s32 present_count = 0;
    s32 total_max_hp = 0;
    ai_data_t* ai = &g_ai_data_base;

    g_ai_workspace = ai;
    g_ai_unit_snapshots = g_ai_unit_snapshot_storage;
    ai->resume_pending = 0;
    ai->selected_action.rank.decided = 0;
    ai->selected_action.coords.fields.zero = 0;
    ai->hardcoded_status_abilities[0].skillset = SKILLSET_ID_BLOOD_SUCK_STATUS;
    ai->hardcoded_status_abilities[0].packed_id = ABILITY_ID_BLOOD_SUCK;
    ai->hardcoded_status_abilities[0].usage.flags = AI_ABILITY_ENTRY_USABLE | AI_ABILITY_ENTRY_EXHAUSTIVE_ORIGIN_SCAN;
    ai->hardcoded_status_abilities[1].skillset = SKILLSET_ID_FROG_ATTACK;
    ai->hardcoded_status_abilities[1].packed_id = ABILITY_ID_FROG_ATTACK;
    ai->hardcoded_status_abilities[1].usage.flags = AI_ABILITY_ENTRY_USABLE | AI_ABILITY_ENTRY_EXHAUSTIVE_ORIGIN_SCAN;
    ai->present_unit_average_max_hp = 0;
    ai->map_max_x = g_map_max_x;
    ai->map_max_y = g_map_max_y;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        ai->unit_compact_indices[i] = 0xff;
        if (g_battle_unit_stats[i].entd_slot != BATTLE_ENTD_SLOT_NONE) {
            ai->unit_compact_indices[i] = present_count;
            battle_ai_init_unit_abilities(i);
            present_count++;
            total_max_hp += g_battle_unit_stats[i].max_hp;
        }
    }
    if (present_count != 0) {
        s32 average = total_max_hp / present_count;
        if (average >= 256) {
            ai->present_unit_average_max_hp = 255;
        } else {
            ai->present_unit_average_max_hp = average;
        }
    }
    {
        s32 empty = 0xff;
        for (i = AI_KNOWN_TRAP_COUNT - 1; i >= 0; i--) {
            ai->known_trap_coords[i].fields.x = empty;
        }
    }
    ai->throw_ability_id = ABILITY_ID_THROW_KNIFE;
    ai->throw_weapon_id = ITEM_ID_DAGGER;
}

/*
 * Initialize a unit's ability-use statistics and scenario targeting.
 *
 * Monster Skill-dependent entries count toward totals but not the MP,
 * Silence, or evasion numerators. Behavior updates depend on scenario flags.
 */
void battle_ai_init_unit_abilities(s32 unit_id) {
    ai_data_t* ai = &g_ai_data_base;
    unit_stats_t* unit = &g_battle_unit_stats[unit_id];
    ai_unit_decision_t* decision;
    ai_ability_entry_t* entry;
    s32 ability_count;
    s32 mp_count;
    s32 silence_count;
    s32 evade_count;
    s32 i;

    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE || ai->unit_compact_indices[unit_id] >= AI_COMPACT_UNIT_COUNT) {
        return;
    }
    battle_ai_build_unit_ability_list(unit_id);
    ability_count = 0;
    mp_count = 0;
    silence_count = 0;
    evade_count = 0;
    decision = &ai->unit_decisions[unit_id];
    decision->highest_mp_cost = 0;
    decision->lowest_mp_cost = 0xff;
    for (i = 0; i < AI_ABILITY_LIST_COUNT; i++) {
        entry = &ai->ability_lists[ai->unit_compact_indices[unit_id]][i];
        if (entry->skillset == SKILLSET_ID_NONE) {
            break;
        }
        battle_ai_load_ability_entry(entry);
        ability_count++;
        /* The target reads and updates skillset/usage as one 16-bit value. */
        if (!entry->usage.policy.requires_monster_skill) {
            if (ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTED_BY_SILENCE) {
                silence_count++;
            }
            if (ai->considered_ability.ai_flags.word & AI_ABILITY_EVADEABLE) {
                evade_count++;
            }
            if (ai->considered_ability.mp_cost != 0) {
                mp_count++;
                if (decision->highest_mp_cost < ai->considered_ability.mp_cost) {
                    decision->highest_mp_cost = ai->considered_ability.mp_cost;
                }
                if (ai->considered_ability.mp_cost < decision->lowest_mp_cost) {
                    decision->lowest_mp_cost = ai->considered_ability.mp_cost;
                }
            }
        }
        entry->usage.flags &= AI_ABILITY_ENTRY_INIT_PRESERVE_MASK;
        if (ai->considered_ability.ai_flags.word & (AI_ABILITY_DIRECT_TRAJECTORY | AI_ABILITY_ARC_TRAJECTORY)) {
            continue;
        }
        if (ai->considered_ability.ai_flags.fields.flags_3 & AI_ABILITY_FLAG_3_EXHAUSTIVE_ORIGIN_MASK) {
            entry->usage.policy.exhaustive_origin_scan = 1;
        } else {
            if (ai->considered_ability.ability_id < ABILITY_ID_ITEM_FIRST) {
                if (ai->considered_ability.parameters.fields.range + ai->considered_ability.parameters.fields.aoe < 3
                    && (g_main_ability_range_data[ai->considered_ability.ability_id].flags_4
                        & ABILITY_SECONDARY_FLAG_4_EVADEABLE)) {
                    entry->usage.policy.exhaustive_origin_scan = 1;
                    continue;
                }
                if (g_main_ability_range_data[ai->considered_ability.ability_id].flags_2
                    & (ABILITY_SECONDARY_FLAG_2_CANNOT_HIT_CASTER | ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ENEMIES)) {
                    entry->usage.policy.force_consume_evaluated_origins = 1;
                    continue;
                }
            }
            if (ai->considered_ability.parameters.fields.aoe == 0) {
                entry->usage.policy.force_consume_evaluated_origins = 1;
            }
        }
    }
    decision->silence_mod = battle_ai_calculate_ratio_times_4(ability_count, silence_count);
    decision->mp_ability_mod = battle_ai_calculate_ratio_times_4(ability_count, mp_count);
    decision->evade_mod = battle_ai_calculate_ratio_times_4(ability_count, evade_count);
    battle_mem_copy_bytes(&decision->flags, &unit->ai_flags, 5);
    decision->target.fields.x = unit->ai_target_x;
    decision->target.fields.y = unit->ai_target_y;
    decision->target.fields.elevation = unit->ai_flags >> 7;
    decision->target.fields.zero = 0;
    if (!(decision->flags & AI_DECISION_SPECIAL_BEHAVIOR)) {
        return;
    }
    if (decision->targeting_flags_2 & AI_TARGET_CONSERVE_CT) {
        ai->unit_autobattle_modes[unit_id] = AI_AUTOBATTLE_RETREAT;
        return;
    }
    if (!(decision->flags & AI_DECISION_FOCUS_TARGET)) {
        return;
    }
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        if (g_battle_unit_stats[i].entd_slot == BATTLE_ENTD_SLOT_NONE
            || g_battle_unit_stats[i].unit_id != decision->main_target_id) {
            continue;
        }
        decision->main_target_id = i;
        if (!(decision->flags & AI_DECISION_AGGRESSIVE)) {
            return;
        }
        unit->auto_battle_target = i;
        if ((g_battle_unit_stats[i].initial_team_flags & BATTLE_TEAM_MASK)
            != (unit->initial_team_flags & BATTLE_TEAM_MASK)) {
            ai->unit_autobattle_modes[unit_id] = AI_AUTOBATTLE_TARGETED;
        } else {
            ai->unit_autobattle_modes[unit_id] = AI_AUTOBATTLE_RECOVERY;
        }
        return;
    }
    decision->flags &= ~AI_DECISION_SPECIAL_BEHAVIOR;
}

/* dividend * 4 / divisor, or 0 for a zero divisor: a share in quarters, such
 * as how many of a unit's abilities Silence stops. */
s32 battle_ai_calculate_ratio_times_4(s32 divisor, s32 dividend) {
    if (divisor == 0) {
        return 0;
    }
    return dividend * 4 / divisor;
}

/* Byte-oriented view of the same four-byte ability entry. */
typedef struct ai_ability_entry_byte_view {
    u8 ability_id_low;
    u8 unit_and_id_high;
    u8 skillset;
    u8 usage_flags;
} ai_ability_entry_byte_view_t;
typedef char ai_ability_entry_byte_view_must_be_4[sizeof(ai_ability_entry_byte_view_t) == 4 ? 1 : -1];

/*
 * Build a unit's ability list and weapon-attack properties.
 *
 * Non-monster skillsets include Attack and a secondary skillset. Frog Attack
 * is appended for every unit; a terminator is written only if space remains.
 */
void battle_ai_build_unit_ability_list(s32 unit_id) {
    unit_stats_t* unit = &g_battle_unit_stats[unit_id];
    ai_data_t* ai = &g_ai_data_base;
    ai_weapon_data_t* weapon;
    s32 list_id = ai->unit_compact_indices[unit_id];
    s32 count = 0;
    s32 weapon_flags;

    if (unit->primary_skillset < SKILLSET_ID_MONSTER_FIRST) {
        ai->ability_lists[list_id][0].skillset = SKILLSET_ID_ATTACK;
        ai->ability_lists[list_id][0].packed_id = unit_id << AI_ABILITY_ENTRY_UNIT_SHIFT;
        weapon = &ai->unit_weapon_data[list_id];
        weapon->words[0] = 0;
        weapon->words[1] = 0;
        weapon->fields.flags_1 |= AI_ABILITY_FLAG_1_AFFECTS_HP;
        weapon->fields.flags_2 |= AI_ABILITY_FLAG_2_EVADEABLE;
        weapon->fields.flags_3 |= AI_ABILITY_FLAG_3_PHYSICAL_ATTACK;
        weapon_flags = AI_WEAPON_FORMULA_NONE;
        if (!(unit->unit_flags & UNIT_FLAG_MONSTER)) {
            battle_ai_store_weapon_attack_data(
                weapon, unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON], &weapon_flags);
            battle_ai_store_weapon_attack_data(
                weapon, unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON], &weapon_flags);
        }
        switch (weapon_flags) {
        case AI_WEAPON_FORMULA_NONE:
            weapon->fields.range = 1;
            /* Fall through to normal enemy targeting. */
        case AI_WEAPON_FORMULA_DAMAGE:
            weapon->fields.flags_1 |= AI_ABILITY_FLAG_1_TARGET_ENEMIES;
            break;
        case AI_WEAPON_FORMULA_ABSORB_HP:
            weapon->fields.flags_1 |= AI_ABILITY_FLAG_1_TARGET_ENEMIES;
            weapon->fields.flags_2 |= AI_ABILITY_FLAG_2_UNDEAD_REVERSE;
            break;
        case AI_WEAPON_FORMULA_HEAL:
            weapon->fields.flags_1 |= AI_ABILITY_FLAG_1_TARGET_ALLIES;
            weapon->fields.flags_2 |= AI_ABILITY_FLAG_2_UNDEAD_REVERSE;
            break;
        }
        count++;
    }
    count = battle_ai_add_usable_skillset_abilities(unit_id, unit->primary_skillset, count);
    if (unit->primary_skillset < SKILLSET_ID_MONSTER_FIRST) {
        count = battle_ai_add_usable_skillset_abilities(unit_id, unit->secondary_skillset, count);
    }
    ai->ability_lists[list_id][count].skillset = SKILLSET_ID_FROG_ATTACK;
    ai->ability_lists[list_id][count].packed_id = (unit_id << AI_ABILITY_ENTRY_UNIT_SHIFT) + ABILITY_ID_FROG_ATTACK;
    /* The byte view preserves the target's separate final-store address. */
    ((ai_ability_entry_byte_view_t*)&ai->ability_lists[list_id][count])->usage_flags
        = AI_ABILITY_ENTRY_USABLE | AI_ABILITY_ENTRY_EXHAUSTIVE_ORIGIN_SCAN;
    count++;
    if (count < AI_ABILITY_LIST_COUNT) {
        ai->ability_lists[list_id][count].skillset = SKILLSET_ID_NONE;
    }
}

/*
 * Merge one weapon's attack properties into the unit's AI weapon summary.
 *
 * Range and weapon ID are replaced; elements and trajectory/formula flags
 * accumulate. Calling for the left hand last gives its range precedence.
 */
void battle_ai_store_weapon_attack_data(ai_weapon_data_t* out, s32 item_id, s32* flags_out) {
    weapon_data_t* weapon;
    s32 flags;
    s32 formula;

    if (item_id == ITEM_ID_NONE) {
        return;
    }
    weapon = &g_main_item_weapon_data[item_id];
    flags = weapon->flags;
    if (flags & WEAPON_FLAG_ARC) {
        out->fields.flags_3 |= AI_ABILITY_FLAG_3_ARC_TRAJECTORY;
    } else if (flags & WEAPON_FLAG_DIRECT) {
        out->fields.flags_3 |= AI_ABILITY_FLAG_3_DIRECT_TRAJECTORY;
    }
    formula = weapon->formula;
    switch (formula) {
    case ABILITY_FORMULA_WEAPON_ABSORB_HP:
        *flags_out |= AI_WEAPON_FORMULA_ABSORB_HP;
        break;
    case ABILITY_FORMULA_WEAPON_HEAL:
        *flags_out |= AI_WEAPON_FORMULA_HEAL;
        break;
    default:
        *flags_out |= AI_WEAPON_FORMULA_DAMAGE;
        break;
    }
    out->fields.weapon_id = item_id;
    out->fields.range = weapon->range;
    out->fields.element |= weapon->element;
}

/* Append a skillset's usable abilities to the unit's AI ability list and
 * return the updated count.
 *
 * Throw adds three shuriken and three ball entries, then at most one entry for
 * the first other known throw ability; Arithmeticks adds a single entry;
 * Charge is skipped with a gun equipped. Other skillsets add each AI-usable,
 * known ability whose MP cost fits the unit's maximum MP and whose
 * sword/materia-blade requirement passes; a unit with innate Innocent adds
 * none while the workspace's considered ability, not the one being added, is
 * Faith-based. Jump keeps only the best level-jump ability; without one it
 * writes a bare Jump entry that the returned count does not include. For
 * monster skillsets only the fourth ability keeps the monster-skill flag.
 *
 * The known-throw tail sits before the main body because the target emits it
 * there and GCC 2.6.3 keeps source block order. The per-loop temporaries,
 * ai as a local and the int max_mp reproduce the target's allocation, loop
 * set-up order and signed comparison. */
s32 battle_ai_add_usable_skillset_abilities(s32 unit_id, s32 skillset, s32 count) {
    s32 jump_found;
    ai_data_t* ai;
    u16* abilities;
    s32 menu;
    s32 list_id;
    s32 i;
    ai_ability_entry_t* entry;
    ai_ability_entry_t* slot;
    ai_ability_entry_t* row;
    unit_stats_t* unit;
    s32 ability_id;
    s32 unit_bits;
    u8 mp_cost;
    s32 max_mp;
    u8 flags;

    ai = &g_ai_data_base;
    if (skillset == 0) {
        return count;
    }
    jump_found = 0;
    abilities = main_ability_store_skillset_abilities(skillset, SKILLSET_ABILITY_FILTER_ACTION);
    menu = g_main_action_menu_types_by_skillset[skillset];
    list_id = ai->unit_compact_indices[unit_id];
    if (menu == ACTION_MENU_TYPE_WEAPON_INVENTORY) {
        if (battle_ai_is_ability_known(unit_id, skillset, 0)) {
            unit_bits = unit_id << AI_ABILITY_ENTRY_UNIT_SHIFT;
            for (i = 0, slot = ai->ability_lists[list_id]; i < 3; i++) {
                slot[count].skillset = skillset;
                slot[count].packed_id = unit_bits + i + ITEM_ID_THROWABLE_FIRST;
                count++;
            }
        }
        if (battle_ai_is_ability_known(unit_id, skillset, 1)) {
            i = 0;
            unit_bits = unit_id << AI_ABILITY_ENTRY_UNIT_SHIFT;
            for (row = ai->ability_lists[list_id]; i < 3; i++) {
                row[count].skillset = skillset;
                row[count].packed_id = unit_bits + i + ITEM_ID_FIRE_BALL;
                count++;
            }
        }
        for (i = 2; i < 12; i++) {
            if (battle_ai_is_ability_known(unit_id, skillset, i)) {
                entry = &ai->ability_lists[list_id][count];
                count++;
                entry->skillset = skillset;
                entry->packed_id = unit_id << AI_ABILITY_ENTRY_UNIT_SHIFT;
                break;
            }
        }
        return count;
    }
    if (menu == ACTION_MENU_TYPE_ARITHMETICKS) {
        entry = &ai->ability_lists[list_id][count];
        entry->skillset = skillset;
        entry->packed_id = unit_id << AI_ABILITY_ENTRY_UNIT_SHIFT;
        return count + 1;
    }
    if (menu == ACTION_MENU_TYPE_CHARGE) {
        i = ai->unit_weapon_data[list_id].fields.weapon_id;
        if (i >= ITEM_ID_ROMANDA_GUN && i <= ITEM_ID_BLAST_GUN) {
            return count;
        }
    }
    unit = &g_battle_unit_stats[unit_id];
    for (i = 0; i < HUMAN_SKILLSET_ACTION_SLOT_COUNT; i++) {
        ability_id = abilities[i];
        if (ability_id == 0) {
            continue;
        }
        entry = &ai->ability_lists[list_id][count];
        if (!(g_main_ability_data[ability_id].ai_flags.flags_4 & AI_ABILITY_FLAG_4_USABLE_BY_AI)) {
            continue;
        }
        if (ability_id < ABILITY_ID_ITEM_FIRST) {
            mp_cost = g_main_ability_range_data[ability_id].mp_cost;
            if (unit->ability_flags.support[1] & BATTLE_SUPPORT_SET_2_HALF_MP) {
                mp_cost >>= 1;
            }
            max_mp = unit->max_mp;
            if (max_mp < mp_cost) {
                continue;
            }
            if ((ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTED_BY_FAITH)
                && (unit->status_sets.innate[STATUS_BYTE_INDEX(STATUS_ID_INNOCENT)]
                    & STATUS_BYTE_MASK(STATUS_ID_INNOCENT))) {
                continue;
            }
            flags = g_main_ability_range_data[ability_id].flags_4;
            if (flags & ABILITY_SECONDARY_FLAG_4_REQUIRES_SWORD) {
                if (!(unit->equipped_flags & BATTLE_UNIT_EQUIPPED_FLAG_SWORD)) {
                    continue;
                }
            } else if (flags & ABILITY_SECONDARY_FLAG_4_REQUIRES_MATERIA_BLADE) {
                if (!(unit->equipped_flags & BATTLE_UNIT_EQUIPPED_FLAG_MATERIA_BLADE)) {
                    continue;
                }
            }
        }
        if (!battle_ai_is_ability_known(unit_id, skillset, i)) {
            continue;
        }
        if (skillset >= SKILLSET_ID_MONSTER_FIRST) {
            if (i == 3) {
                entry->usage.policy.requires_monster_skill = 1;
            } else {
                entry->usage.policy.requires_monster_skill = 0;
            }
        }
        entry->skillset = skillset;
        entry->packed_id = ability_id;
        if (menu == ACTION_MENU_TYPE_JUMP) {
            if (ability_id >= ABILITY_ID_JUMP_VERTICAL_2) {
                continue;
            }
            if (jump_found == 1) {
                count--;
                entry = &ai->ability_lists[list_id][count];
                entry->packed_id = ability_id;
            }
            jump_found = 1;
        }
        count++;
        entry->packed_id += unit_id << AI_ABILITY_ENTRY_UNIT_SHIFT;
    }
    if (menu == ACTION_MENU_TYPE_JUMP && jump_found == 0) {
        entry->skillset = SKILLSET_ID_JUMP;
        entry->packed_id = unit_id << AI_ABILITY_ENTRY_UNIT_SHIFT;
    }
    return count;
}

/* Whether the unit has learned the given ability bit of a skillset: returns 1
 * for monsters (every ability known) and for skillsets the unit does not own.
 * Job skillsets 5..23 map to learned_abilities rows 0..18; the unit's own
 * primary or base-job skillset maps to row 0. */
s32 battle_ai_is_ability_known(s32 unit_id, s32 skillset_id, s32 bit) {
    unit_stats_t* unit = &g_battle_unit_stats[unit_id];
    s32 byte;
    u8* row;

    if ((unit->unit_flags & UNIT_FLAG_MONSTER) != 0) {
        return 1;
    }
    if (skillset_id >= SKILLSET_ID_BASIC_SKILL && skillset_id < SKILLSET_ID_MIMIC) {
        skillset_id = skillset_id - SKILLSET_ID_BASIC_SKILL;
    } else {
        if (skillset_id != unit->primary_skillset && skillset_id != unit->base_job_skillset)
            return 1;
        skillset_id = 0;
    }
    /* Typed indexing moves the field offset ahead of the unit-base addition.
     * Keep the target's address order, deriving the offset from the field. */
    row = (u8*)(skillset_id * UNIT_LEARNED_ABILITY_BYTES_PER_JOB + (u32)unit
        + ((u32)unit->career.learned_abilities - (u32)unit));
    byte = bit / 8;
    return (row[byte] & (0x80 >> (bit - (byte * 8)))) != 0;
}
