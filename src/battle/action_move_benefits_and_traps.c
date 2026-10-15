/* Tile traps, movement-ability benefits and the EXP/JP/level gains they yield. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_formula.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Set up the action result for the trap on a unit's tile.
 *
 * A mount passes the trap to the rider in its linked partner slot. Trap 0
 * levels the target down, 1 and 2 inflict Death Sentence and Sleep (status
 * byte 4 bits 0x01 and 0x10) when status_modify_inflictions allows it, and 3
 * deals max HP / 5 damage. Returns the trap id, or 0xff for none. */
s32 action_apply_tile_trap(unit_stats_t* unit) {
    u8 trap;
    action_data_t* action;

    trap = map_set_item_trap_data(unit->x, unit->position.y, unit->position.higher_elevation)->trap_id;
    if (unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
        unit = &g_battle_unit_stats[unit->mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK];
    }
    g_action_current_ability.target_id = unit->battle_id;
    g_action_target = unit;
    g_action_target_data = &unit->action;
    action_clear_current_data(&unit->action);
    switch (trap) {
    case BATTLE_TRAP_ID_DEGENERATOR:
        g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
        g_action_target_data->special_effect = ACTION_SPECIAL_EFFECT_LEVEL_DOWN;
        break;
    case BATTLE_TRAP_ID_DEATHTRAP:
        g_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_DEATH_SENTENCE)]
            = STATUS_BYTE_MASK(STATUS_ID_DEATH_SENTENCE);
        if (status_modify_inflictions(0) != 0) {
            g_action_target_data->attack_type = ACTION_TYPE_STATUS_CHANGE;
        } else {
            formula_force_attack_miss();
        }
        break;
    case BATTLE_TRAP_ID_SLEEPING_GAS:
        g_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_SLEEP)]
            = STATUS_BYTE_MASK(STATUS_ID_SLEEP);
        if (status_modify_inflictions(0) != 0) {
            g_action_target_data->attack_type = ACTION_TYPE_STATUS_CHANGE;
        } else {
            formula_force_attack_miss();
        }
        break;
    case BATTLE_TRAP_ID_STEEL_NEEDLE:
        action = g_action_target_data;
        action->hp_damage = unit->max_hp / 5;
        action->attack_type = ACTION_TYPE_HP_DAMAGE;
        break;
    default:
        trap = BATTLE_TRAP_ID_NONE;
        break;
    }
    return trap;
}

/* Prepare the action record for a movement ability's benefit.
 *
 * Initialize the record, then select HP, MP, EXP or JP in that order. Move-HP
 * Up and Move-MP Up restore a tenth of the maximum, rounded up; Move-Get EXP
 * and Move-Get JP give battle_unit_calculate_move_exp_jp_up_to_earn's amount,
 * doubled by Gained EXP Up or Gained JP Up. Return -1 for a blocking status
 * and 0 otherwise, including when no movement benefit is present. The caller
 * applies and displays the result. */
s32 action_init_movement_ability_benefit(unit_stats_t* unit) {
    s32 flags;
    u8 unit_id;
    s32 amount;
    action_data_t* action;

    unit_id = unit->battle_id;
    g_action_target = unit;
    g_action_target_data = &unit->action;
    g_action_current_ability.target_id = unit_id;
    action_clear_current_data(&unit->action);
    if (main_unit_has_status_in_set(unit, MAIN_STATUS_CHECK_SET_PREVENT_REACTION))
        return -1;
    flags = move_get_support_flags(unit);
    if (flags & MOVE_POST_EVENT_MOVE_HP_UP) {
        amount = (unit->max_hp + 9) / 10;
        action = g_action_target_data;
        action->attack_type = ACTION_TYPE_HP_HEALING;
        action->hp_healing = amount;
    } else if (flags & MOVE_POST_EVENT_MOVE_MP_UP) {
        amount = (unit->max_mp + 9) / 10;
        action = g_action_target_data;
        action->attack_type = ACTION_TYPE_MP_HEALING;
        action->mp_healing = amount;
    } else if (flags & MOVE_POST_EVENT_MOVE_GET_EXP) {
        g_action_target_data->exp_change = battle_unit_calculate_move_exp_jp_up_to_earn(g_action_target);
        if (g_action_target->ability_flags.support[1] & BATTLE_SUPPORT_SET_2_GAINED_EXP_UP)
            g_action_target_data->exp_change *= 2;
        g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    } else if (flags & MOVE_POST_EVENT_MOVE_GET_JP) {
        g_action_target_data->jp_change = battle_unit_calculate_move_exp_jp_up_to_earn(g_action_target);
        if (g_action_target->ability_flags.support[1] & BATTLE_SUPPORT_SET_2_GAINED_JP_UP)
            g_action_target_data->jp_change *= 2;
        g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    }
    return 0;
}

/* Simulate the unit's pending action and write the EXP and JP gained and any
 * new level and job level to rewards[0..3]. Returns -1 for an empty unit
 * slot, otherwise the action finalization result. */
s32 battle_unit_build_gained_exp_jp_level_job_level(unit_stats_t* unit, action_reward_display_t* rewards) {
    s32 job_slot[2];
    s32 result;
    s32 unit_id;

    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    unit_id = unit->battle_id;
    g_action_current_ability.level_gained_flag = 0;
    g_action_current_ability.job_level_gained_flag = 0;
    g_action_context = ACTION_CONTEXT_REACTION_OR_SIMULATION;
    g_battle_acting_unit_id = unit_id;
    result = action_finalize_attack_and_flag_reactions(unit->battle_id);
    g_action_context = ACTION_CONTEXT_PRIMARY;
    rewards->earned_experience = unit->action.exp_change;
    rewards->earned_jp = unit->action.jp_change;
    rewards->level_for_display = 0;
    if (g_action_current_ability.level_gained_flag != 0) {
        rewards->level_for_display = unit->level;
    }
    rewards->job_level_for_display = 0;
    if (g_action_current_ability.job_level_gained_flag != 0) {
        rewards->job_level_for_display = battle_unit_load_job_level(unit, unit->job_id, job_slot);
    }
    return result;
}

/* Adds the pending JP change to the unit's job and, if that changed the job
   level, sets the job-level-gained flag so the caller queues the level-up report. */
void action_apply_jp_change(unit_stats_t* unit) {
    s32 jp;
    s32 initial_level;
    u8 job;
    u8 battle_id;

    job = unit->job_id;
    battle_id = unit->battle_id;
    initial_level = battle_unit_load_job_level(unit, job, &jp);
    battle_unit_store_jp_and_calculate_unlocked_jobs(
        battle_id, unit->action.jp_change, unit->initial_team_flags & BATTLE_TEAM_MASK, jp);
    if (battle_unit_load_job_level(unit, job, &jp) != initial_level) {
        g_action_current_ability.job_level_gained_flag = 1;
    }
}
