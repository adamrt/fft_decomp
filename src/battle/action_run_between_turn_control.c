#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_status.h"
#include "psx/types.h"

s32 action_run_between_turn_control(battle_between_turn_mode_e mode) {
    unit_stats_t* unit;
    s32 result;
    s32 count;
    s32 i;
    s32 best;
    s32 ret;
    s32 flags;
    u16 max_ct;
    u16 speed;
    u16 ct;
    u8 value;

    result = 0;
    count = 0;
    if (mode == BATTLE_BETWEEN_TURN_MODE_RESET) {
        g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_ADD_CT;
        g_battle_turn_unit_id = -1;
        for (i = BATTLE_TEAM_COUNT - 1; i >= 0; i--) {
            g_battle_team_golem[i] = 0;
        }
        g_action_current_attacker.mimic_pending = 0;
        for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
            unit = &g_battle_unit_stats[i];
            unit->ct = 0;
            unit->has_turn = 0;
            unit->ability_outcome = 0;
            unit->charged_ability_ct = 0xff;
        }
        ai_end_action_simulation();
        return 0xe000;
    }
    if (mode == BATTLE_BETWEEN_TURN_MODE_FIND_ACTIVE_UNIT) {
        for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
            unit = &g_battle_unit_stats[i];
            if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
                if (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
                        & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD)
                            | STATUS_BYTE_MASK(STATUS_ID_JUMP)))) {
                    if (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
                            & (STATUS_BYTE_MASK(STATUS_ID_PETRIFY) | STATUS_BYTE_MASK(STATUS_ID_TREASURE)))) {
                        return i | 0xd000;
                    }
                }
            }
        }
        return 0xdf00;
    }
    while (result == 0) {
        switch (g_battle_between_turn_state) {
        case BATTLE_BETWEEN_TURN_STATE_ADD_CT:
            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                unit = &g_battle_unit_stats[i];
                flags = status_calculate_turn_flags(unit);
                if (!(flags & BATTLE_TURN_STATUS_CT_FROZEN)) {
                    speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
                    if (flags & BATTLE_TURN_STATUS_SLOW) {
                        speed >>= 1;
                    } else if (flags & BATTLE_TURN_STATUS_HASTE) {
                        speed += speed >> 1;
                    }
                    ct = unit->ct + speed;
                    if (ct >= 0xff) {
                        ct = 0xfe;
                    }
                    unit->ct = ct;
                }
            }
            g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_PICK_UNIT;
            break;
        case BATTLE_BETWEEN_TURN_STATE_PICK_UNIT:
            max_ct = UNIT_TURN_CT_THRESHOLD - 1;
            best = 0xff;
            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                unit = &g_battle_unit_stats[i];
                flags = status_calculate_turn_flags(unit);
                if (!(flags & BATTLE_TURN_STATUS_CT_FROZEN)) {
                    if (max_ct < unit->ct) {
                        best = i;
                        max_ct = unit->ct;
                    }
                }
            }
            if (best != 0xff) {
                unit = &g_battle_unit_stats[best];
                if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_JUMP)] & STATUS_BYTE_MASK(STATUS_ID_JUMP)) {
                    unit->ct = UNIT_TURN_CT_THRESHOLD - 1;
                    continue;
                }
                if (unit->ct == 0xff) {
                    unit->ct = 0;
                } else {
                    unit->ct = max_ct % UNIT_TURN_CT_THRESHOLD;
                }
                ret = status_build_upkeep_action(best, unit);
                if (ret != 0) {
                    result = best | BATTLE_TURN_EVENT_ACTION_RESULT;
                    if (ret & 1) {
                        if (ret == 1) {
                            main_unit_increase_casualty_counters(unit);
                        }
                        return result;
                    }
                }
                flags = status_calculate_turn_flags(unit);
                if (!(flags
                        & (BATTLE_TURN_STATUS_CT_FROZEN | BATTLE_TURN_STATUS_INCAPACITATED | BATTLE_TURN_STATUS_DEAD))
                    || ret == 2) {
                    unit->has_turn = 1;
                    unit->movement_taken = 0;
                    unit->action_taken = 0;
                    unit->transparent_removal_flag = 1;
                    if (ret == 0) {
                        result = best | BATTLE_TURN_EVENT_UNIT_READY;
                    }
                    g_battle_between_turn_resume_state = g_battle_between_turn_state;
                    g_battle_turn_unit_id = best;
                    g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_UNIT_TURN;
                    return result;
                }
                continue;
            }
            g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_TICK_CHARGES;
            break;
        case BATTLE_BETWEEN_TURN_STATE_TICK_CHARGES:
            i = 0;
            do {
                unit = &g_battle_unit_stats[i];
                flags = status_calculate_turn_flags(unit);
                if (!(flags & BATTLE_TURN_STATUS_BLOCKS_WAIT_MENU_MASK)) {
                    value = unit->charged_ability_ct;
                    if (value != 0xff && value != 0) {
                        value--;
                        unit->charged_ability_ct = value;
                    }
                }
                /* clang-format off */
            } while (++i < BATTLE_UNIT_SLOT_COUNT); g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_RESOLVE_CHARGE;
            /* clang-format on */
            break;
        case BATTLE_BETWEEN_TURN_STATE_RESOLVE_CHARGE:
            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                unit = &g_battle_unit_stats[i];
                flags = status_calculate_turn_flags(unit);
                if (!(flags & BATTLE_TURN_STATUS_BLOCKS_WAIT_MENU_MASK) && unit->charged_ability_ct == 0) {
                    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PERFORMING)]
                        & STATUS_BYTE_MASK(STATUS_ID_PERFORMING)) {
                        main_unit_copy_last_ability_ct(unit);
                        unit->charged_ability_ct = unit->ability_ct;
                    } else {
                        unit->charged_ability_ct = 0xff;
                    }
                    g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_PICK_UNIT_AFTER_CHARGE;
                    result = i | BATTLE_TURN_EVENT_ABILITY_READY;
                    return result;
                }
            }
            g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_EXPIRE_STATUSES;
            break;
        case BATTLE_BETWEEN_TURN_STATE_EXPIRE_STATUSES:
            i = 0;
            do {
                unit = &g_battle_unit_stats[i];
                if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE
                    && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_JUMP)]
                        & STATUS_BYTE_MASK(STATUS_ID_JUMP))) {
                    status_update_expiring(i);
                }
                /* clang-format off */
            } while (++i < BATTLE_UNIT_SLOT_COUNT); g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_COLLECT_PENDING_ACTIONS;
            /* clang-format on */
            break;
        case BATTLE_BETWEEN_TURN_STATE_COLLECT_PENDING_ACTIONS:
            i = 0;
            do {
                unit = &g_battle_unit_stats[i];
                g_battle_mimic_pending_flags[i] = 0;
                if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE
                    && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_JUMP)]
                        & STATUS_BYTE_MASK(STATUS_ID_JUMP))) {
                    g_battle_mimic_pending_flags[i] = unit->action.attack_type;
                }
                /* clang-format off */
            } while (++i < BATTLE_UNIT_SLOT_COUNT); g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_RUN_PENDING_ACTIONS;
            /* clang-format on */
            break;
        case BATTLE_BETWEEN_TURN_STATE_RUN_PENDING_ACTIONS:
            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                if (g_battle_mimic_pending_flags[i] != 0) {
                    g_battle_mimic_pending_flags[i] = 0;
                    result = i | BATTLE_TURN_EVENT_ACTION_RESULT;
                    return result;
                }
            }
            g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_ADD_CT;
            break;
        case BATTLE_BETWEEN_TURN_STATE_PICK_UNIT_AFTER_CHARGE:
            max_ct = 0xfe;
            best = 0xff;
            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                flags = status_calculate_turn_flags(&g_battle_unit_stats[i]);
                if (!(flags & BATTLE_TURN_STATUS_CT_FROZEN)) {
                    if (max_ct < g_battle_unit_stats[i].ct) {
                        best = i;
                        max_ct = g_battle_unit_stats[i].ct;
                    }
                }
            }
            if (best != 0xff) {
                unit = &g_battle_unit_stats[best];
                unit->has_turn = 1;
                unit->transparent_removal_flag = 1;
                unit->ct = 0;
                unit->movement_taken = 0;
                unit->action_taken = 0;
                g_battle_between_turn_resume_state = BATTLE_BETWEEN_TURN_STATE_PICK_UNIT_AFTER_CHARGE;
                g_battle_turn_unit_id = best;
                g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_UNIT_TURN;
                result = best | BATTLE_TURN_EVENT_UNIT_READY;
                return result;
            }
            g_battle_between_turn_resume_state = BATTLE_BETWEEN_TURN_STATE_RESOLVE_CHARGE;
            g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_MARK_MIMES;
            break;
        case BATTLE_BETWEEN_TURN_STATE_UNIT_TURN:
            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                unit = &g_battle_unit_stats[i];
                if (unit->has_turn != 0) {
                    s32 unit_status;
                    s32 end_turn;

                    unit_status = status_calculate_turn_flags(unit);
                    end_turn = action_should_end_unit_turn(unit);
                    if ((unit_status & BATTLE_TURN_STATUS_CT_FROZEN) || end_turn != 0) {
                        action_end_turn(i);
                        unit->has_turn = 0;
                    } else {
                        result = i | BATTLE_TURN_EVENT_UNIT_READY;
                        return result;
                    }
                }
            }
            g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_POISON_AND_REGEN;
            break;
        case BATTLE_BETWEEN_TURN_STATE_POISON_AND_REGEN:
            g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_TERRAIN_POISON;
            if (g_battle_turn_unit_id != -1) {
                unit = &g_battle_unit_stats[g_battle_turn_unit_id];
                if (status_apply_poison_and_regen(unit)) {
                    result = g_battle_turn_unit_id | BATTLE_TURN_EVENT_ACTION_RESULT;
                    return result;
                }
            }
            break;
        case BATTLE_BETWEEN_TURN_STATE_TERRAIN_POISON:
            g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_REMOVE_TRANSPARENT;
            if (g_battle_turn_unit_id != -1) {
                unit = &g_battle_unit_stats[g_battle_turn_unit_id];
                if (status_prepare_terrain_poison(unit)) {
                    result = g_battle_turn_unit_id | BATTLE_TURN_EVENT_ACTION_RESULT;
                    return result;
                }
            }
            break;
        case BATTLE_BETWEEN_TURN_STATE_REMOVE_TRANSPARENT:
            g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_MARK_MIMES;
            if (g_battle_turn_unit_id != -1) {
                unit = &g_battle_unit_stats[g_battle_turn_unit_id];
                if (status_remove_transparent(unit)) {
                    result = g_battle_turn_unit_id | BATTLE_TURN_EVENT_ACTION_RESULT;
                    return result;
                }
            }
            break;
        case BATTLE_BETWEEN_TURN_STATE_MARK_MIMES:
            if (g_action_current_attacker.mimic_pending != 0) {
                i = 0;
                do {
                    unit = &g_battle_unit_stats[i];
                    g_battle_mimic_pending_flags[i] = 0;
                    if (unit->job_id == JOB_ID_MIME && unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
                        g_battle_mimic_pending_flags[i] = 1;
                    }
                    /* clang-format off */
                } while (++i < BATTLE_UNIT_SLOT_COUNT); g_battle_between_turn_state = BATTLE_BETWEEN_TURN_STATE_RUN_MIMES;
                /* clang-format on */
            } else {
                g_battle_between_turn_state = g_battle_between_turn_resume_state;
            }
            break;
        case BATTLE_BETWEEN_TURN_STATE_RUN_MIMES:
            g_action_current_attacker.mimic_pending = 0;
            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                unit = &g_battle_unit_stats[i];
                if (g_battle_mimic_pending_flags[i] != 0 && unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
                    g_battle_mimic_pending_flags[i] = 0;
                    ret = action_set_mimic_ability(unit);
                    if (ret == 1) {
                        result = i | BATTLE_TURN_EVENT_ABILITY_READY;
                        return result;
                    }
                    if (ret == -1) {
                        result = i | BATTLE_TURN_EVENT_MIME;
                        return result;
                    }
                }
            }
            g_battle_between_turn_state = g_battle_between_turn_resume_state;
            break;
        }
        count++;
        if (count >= 1201) {
            break;
        }
    }
    return BATTLE_TURN_EVENT_NONE;
}
