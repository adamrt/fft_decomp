/* Between-turn state machine, AT list build/sort and turn-end bookkeeping. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_status.h"
#include "psx/types.h"

/* Runs the in-between-turn state machine in g_battle_between_turn_state until a phase yields a
 * unit event (returned as unit id | event << 8) or 1200 phases pass.
 * Mode 1 resets the machine and mode 2 finds the first unit able to act.
 *
 * Phases 2, 4, 5 and 13 store the next state on their loop's closing line.
 * Under -gcoff a separate line puts a line note between the loop end and the
 * state constant, and loop.c then keeps 3, 5, 6 and 14 inside the state loop
 * (cross-jumped `li v0,N` tails). With no note between them the constant set
 * directly follows the loop and is hoisted with the table base, as in the
 * target (a spilled invariant rematerialised as `li a2,N`). */
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

/*
 * Per-turn status upkeep: stage the change the unit's current status set
 * produces this tick in the unit's action record.
 *
 * Returns 2 for a Reraise or undead revival and for any other staged change
 * (Defending removed, the Brave-below-10 change), 1 when the death counter
 * runs out and the unit becomes a crystal or treasure, 3 when Death Sentence
 * expires on a unit that is not undead, and 0 when nothing happens.
 */
s32 status_build_upkeep_action(s32 unit_id, unit_stats_t* unit) {
    action_data_t* action = &unit->action;
    s32 roll;
    s32 mask;
    u8 counter;

    action_clear_current_data(action);
    roll = rand();
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)]
            & (STATUS_BYTE_MASK(STATUS_ID_DEAD) | STATUS_BYTE_MASK(STATUS_ID_UNDEAD)))
            == STATUS_BYTE_MASK(STATUS_ID_DEAD)
        && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_RERAISE)] & STATUS_BYTE_MASK(STATUS_ID_RERAISE))) {
        action->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] = STATUS_BYTE_MASK(STATUS_ID_DEAD);
        action->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_RERAISE)] = STATUS_BYTE_MASK(STATUS_ID_RERAISE);
        action->attack_type = ACTION_TYPE_HP_HEALING | ACTION_TYPE_STATUS_CHANGE;
        action->hp_healing = unit->max_hp / 10;
        return 2;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD))
        && g_action_state == ACTION_STATE_EXECUTE) {
        if (unit->team_flags & BATTLE_TEAM_FLAG_IMMORTAL) {
            return 0;
        }
        if (unit->unit_flags & (UNIT_FLAG_SAVE_FORMATION | UNIT_FLAG_LOAD_FORMATION)) {
            return 0;
        }
        counter = unit->death_counter;
        counter -= 1;
        if (counter != 0xff) {
            unit->death_counter = counter;
            return 0;
        }
        if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)] & STATUS_BYTE_MASK(STATUS_ID_UNDEAD))
            && (roll & 1)) {
            action->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] = STATUS_BYTE_MASK(STATUS_ID_DEAD);
            action->hp_healing = ((rand() * unit->max_hp) / 0x8000) + 1;
            action->attack_type = ACTION_TYPE_HP_HEALING | ACTION_TYPE_STATUS_CHANGE;
            return 2;
        }
        mask = 0x10;
        if (unit->initial_team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) {
            mask = 0x110;
        }
        if (roll & mask) {
            action->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
                = STATUS_BYTE_MASK(STATUS_ID_CRYSTAL);
            action->attack_type = ACTION_TYPE_STATUS_CHANGE;
            return 1;
        }
        action->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] = STATUS_BYTE_MASK(STATUS_ID_TREASURE);
        action->attack_type = ACTION_TYPE_STATUS_CHANGE;
        return 1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEATH_SENTENCE)]
        & STATUS_BYTE_MASK(STATUS_ID_DEATH_SENTENCE)) {
        counter = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)];
        counter -= 1;
        if (counter == 0) {
            action->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_DEATH_SENTENCE)]
                = STATUS_BYTE_MASK(STATUS_ID_DEATH_SENTENCE);
            action->attack_type = ACTION_TYPE_STATUS_CHANGE;
            if (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)]
                    & STATUS_BYTE_MASK(STATUS_ID_UNDEAD))) {
                action->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] = STATUS_BYTE_MASK(STATUS_ID_DEAD);
                return 3;
            }
        } else {
            unit->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)] = counter;
        }
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEFENDING)] & STATUS_BYTE_MASK(STATUS_ID_DEFENDING)) {
        action->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_DEFENDING)] = STATUS_BYTE_MASK(STATUS_ID_DEFENDING);
        action->attack_type = ACTION_TYPE_STATUS_CHANGE;
    }
    if (unit->brave < 10) {
        action->brave_change = 0x81;
        action->attack_type |= ACTION_TYPE_PSEUDO_STATUS;
    }
    return (action->attack_type != 0) * 2;
}

/* Returns the BATTLE_TURN_STATUS_* flags that decide how a unit takes part in the
 * turn order.
 *
 * CT_FROZEN alone for an empty slot, a unit with a status of the freeze-CT set or
 * a ridden mount; INCAPACITATED | DEAD for a dead unit; otherwise INCAPACITATED |
 * SLEEP for Sleep plus the HASTE and SLOW bits. The retail status data puts Sleep
 * in the freeze-CT set, which leaves the SLEEP result unreachable. */
s32 status_calculate_turn_flags(const unit_stats_t* unit) {
    s32 status1;
    s32 status4;
    s32 status5;
    s32 flags;

    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return BATTLE_TURN_STATUS_CT_FROZEN;
    }
    if (main_unit_has_status_in_set(unit, MAIN_STATUS_CHECK_SET_FREEZE_CT) != 0) {
        return BATTLE_TURN_STATUS_CT_FROZEN;
    }
    if ((unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) != 0) {
        return BATTLE_TURN_STATUS_CT_FROZEN;
    }
    status1 = unit->status_sets.current[0];
    status4 = unit->status_sets.current[3];
    status5 = unit->status_sets.current[4];
    if ((status1 & STATUS_BYTE_MASK(STATUS_ID_DEAD)) != 0) {
        return BATTLE_TURN_STATUS_INCAPACITATED | BATTLE_TURN_STATUS_DEAD;
    }
    flags = -((status5 & STATUS_BYTE_MASK(STATUS_ID_SLEEP)) != 0)
        & (BATTLE_TURN_STATUS_INCAPACITATED | BATTLE_TURN_STATUS_SLEEP);
    if ((status4 & STATUS_BYTE_MASK(STATUS_ID_HASTE)) != 0) {
        flags |= BATTLE_TURN_STATUS_HASTE;
    }
    if ((status4 & STATUS_BYTE_MASK(STATUS_ID_SLOW)) != 0) {
        flags |= BATTLE_TURN_STATUS_SLOW;
    }
    return flags;
}

/* Determine whether either side has met its battle-ending condition.
 *
 * Returns -1 when the blue side is defeated, 0 when all opposing units are
 * defeated, and 1 while both sides can continue. */
s32 action_check_battle_outcome(void) {
    s32 player_unit_count;
    s32 enemy_unit_count;
    s32 unit_index;
    s32 selected;
    unit_stats_t* unit;

    player_unit_count = 0;
    enemy_unit_count = 0;
    for (unit_index = 0; unit_index < BATTLE_UNIT_SLOT_COUNT; unit_index++) {
        unit = &g_battle_unit_stats[unit_index];
        if ((unit->initial_team_flags & BATTLE_TEAM_IMPORTANCE_MASK) == BATTLE_TEAM_IMPORTANCE_MASK) {
            if (unit->existence != BATTLE_UNIT_EXISTENCE_NONE) {
                selected = unit->existence == BATTLE_UNIT_EXISTENCE_DISABLED;
                selected += main_unit_has_status_in_set(unit, MAIN_STATUS_CHECK_SET_UNIT_LOST);
                if (selected != 0) {
                    if (unit->initial_team_flags & BATTLE_TEAM_MASK) {
                        enemy_unit_count++;
                    } else {
                        player_unit_count++;
                    }
                }
            }
        }
    }

    if (player_unit_count != 0) {
        return -1;
    }
    if (enemy_unit_count != 0) {
        return 0;
    }

    for (unit_index = 0; unit_index < BATTLE_UNIT_SLOT_COUNT; unit_index++) {
        unit = &g_battle_unit_stats[unit_index];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
            selected = unit->initial_team_flags & BATTLE_TEAM_MASK;
            if (main_unit_has_status_in_set(unit, MAIN_STATUS_CHECK_SET_KO) == 0) {
                if (selected != 0) {
                    enemy_unit_count++;
                } else {
                    player_unit_count++;
                }
            }
        }
    }

    if (player_unit_count == 0) {
        return -1;
    }
    return enemy_unit_count != 0;
}

/* Return the unit's position on a freshly built AT list.
 *
 * Returns -1 for an empty slot, -3 for a unit that cannot act and -2 when the
 * unit is not on the list. */
s32 action_find_unit_turn_index(const unit_stats_t* unit) {
    battle_at_entry_t at_list[BATTLE_AT_LIST_CAPACITY];
    s32 i;
    battle_at_entry_t* entry;
    u8 battle_id;

    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE)
        return -1;
    if (status_is_unit_absent_dead_crystal_treasure_petrified_or_ridden(unit) != 0)
        return -3;
    battle_id = unit->battle_id;
    action_calculate_at_list(at_list, BATTLE_AT_LIST_MODE_TURNS_ONLY);
    for (i = 0; i < BATTLE_AT_LIST_CAPACITY; i++) {
        entry = &at_list[i];
        if ((entry->unit_code & BATTLE_AT_UNIT_ID_MASK) == (battle_id & 0xFF)) {
            return i;
        }
    }
    return -2;
}

/* Rebuild the AT list and return the unit id of the chosen entry.
 *
 * Returns -4 for an empty entry; a charged-action entry (bit 0x40) returns its
 * unit id plus 0x100. */
s32 action_calculate_at_list_and_get_specific_unit_id(s32 at_index) {
    battle_at_entry_t at_list[BATTLE_AT_LIST_CAPACITY];
    u32 entry;
    u32 unit_id;

    action_calculate_at_list(at_list, BATTLE_AT_LIST_MODE_ALL);
    entry = at_list[at_index].unit_code;
    unit_id = entry & BATTLE_AT_UNIT_ID_MASK;
    if (unit_id == BATTLE_AT_END_OF_LIST) {
        return -4;
    }
    if (entry & BATTLE_AT_ACTION_ENTRY) {
        unit_id += 0x100;
    }
    return unit_id;
}

/* Return 1 when the unit cannot participate in the AT list. */
s32 status_is_unit_absent_dead_crystal_treasure_petrified_or_ridden(const unit_stats_t* unit) {
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return 1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
        & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD))) {
        return 1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
        & (STATUS_BYTE_MASK(STATUS_ID_PETRIFY) | STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
        return 1;
    }
    if (unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
        return 1;
    }
    return 0;
}

/* Preview where a planned action lands on the AT list.
 *
 * Runs the attack preparation for action in the preview state, rebuilds
 * at_list and counts the turns until the action resolves, then restores the
 * unit's action data, CT, move/act flags and statuses. */
s32 action_preview_at_list(unit_stats_t* unit, action_command_t* action, battle_at_entry_t* at_list) {
    action_command_t saved_action;
    action_command_t* action_data;
    s32 old_state;
    s32 turns;
    u8 old_charged_ability_ct;
    u8 old_inflicted_status;
    u8 old_movement_taken;
    u8 old_action_taken;

    action_data = &unit->action_command;
    main_util_copy_action_data(action_data, &saved_action);
    old_charged_ability_ct = unit->charged_ability_ct;
    old_inflicted_status = unit->inflicted_status[0];
    old_movement_taken = unit->movement_taken;
    old_action_taken = unit->action_taken;
    old_state = g_action_state;
    g_action_state = ACTION_STATE_PREVIEW;
    if (action_call_attack_preparation_at_preview(action) == BATTLE_ATTACK_PREPARATION_IMMEDIATE) {
        unit->charged_ability_ct = 0;
    }
    turns = action_calculate_at_list(at_list, BATTLE_AT_LIST_MODE_PREVIEW);
    if (turns != -1) {
        turns = action_get_number_of_turns_to_resolve(unit->battle_id, unit->charged_ability_ct, at_list) & 0xFF;
    }
    g_action_state = old_state;
    unit->movement_taken = old_movement_taken;
    unit->action_taken = old_action_taken;
    unit->inflicted_status[0] = old_inflicted_status;
    main_status_store_current(unit);
    unit->charged_ability_ct = old_charged_ability_ct;
    main_util_copy_action_data(&saved_action, action_data);
    return turns;
}

/* Rebuilds the 40-entry AT list: charged and performing actions first, then
 * each unit's next turns simulated tick by tick under Stop/Sleep, Slow and
 * Haste. Returns -1 when no entry was added.
 *
 * One u8 local holds the Sleep CT, the charging CT and the per-tick CT gain,
 * as the target's shared $v1 shows. Its early live range conflicts with $v0,
 * which keeps the gain out of $v0 so reorg can copy the join's zero-extension
 * into both Haste delay slots. The u16 wait and the value + wait order keep
 * the target's addu operand order through combine. The perform loop's key must
 * be a multi-set u32: a single-set shift temporary gets sched1's birth boost
 * and lands after the a1 load. The 0x50-byte array is the unreferenced frame
 * area the target reserves. */
s32 action_calculate_at_list(battle_at_entry_t* list, battle_at_list_mode_e mode) {
    unit_stats_t* unit;
    u8 unused[0x50];
    s32 placed;
    s32 slot;
    s32 phase_tick;
    u16 wait;
    u8 value;
    u16 tick;
    s32 turns;
    s32 slow;
    s32 haste;
    u8 speed;
    u16 ct;
    u32 remainder;
    s32 added;
    u8 current;
    u8 innate;
    u32 key;

    placed = 0;
    phase_tick = g_battle_between_turn_resume_state == BATTLE_BETWEEN_TURN_STATE_PICK_UNIT_AFTER_CHARGE;
    for (slot = 0; slot < BATTLE_AT_LIST_CAPACITY; slot++) {
        list[slot].unit_code = 0xff;
        list[slot].action_id_low = 0xff;
        list[slot].sort_key = 0xffff;
    }

    if (mode != BATTLE_AT_LIST_MODE_ACTIONS_ONLY) {
        unit = battle_unit_find_active_data_pointer();
        if (unit != 0) {
            action_sort_at_list(unit->battle_id, 0, 0, list);
        }
    }

    for (slot = 0; slot < 0x15; slot++) {
        unit = &g_battle_unit_stats[slot];
        if (status_is_unit_absent_dead_crystal_treasure_petrified_or_ridden(unit) != 0) {
            continue;
        }

        wait = 0;
        if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_STOP)] & STATUS_BYTE_MASK(STATUS_ID_STOP)) {
            wait = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_STOP)];
        }
        if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SLEEP)] & STATUS_BYTE_MASK(STATUS_ID_SLEEP)) {
            value = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_SLEEP)];
            if (wait < value) {
                wait = value;
            }
        }

        value = unit->charged_ability_ct;
        if (value != 0xff && mode != BATTLE_AT_LIST_MODE_TURNS_ONLY) {
            tick = value + wait;
            if (value != 0 || unit->has_turn == 0 || mode != BATTLE_AT_LIST_MODE_PREVIEW) {
                tick += phase_tick;
            }
            placed += action_sort_at_list(slot, 1, (tick << 8) & 0xff00, list);
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PERFORMING)]
                & STATUS_BYTE_MASK(STATUS_ID_PERFORMING)) {
                main_unit_copy_last_ability_ct(unit);
                for (turns = 0; turns < BATTLE_AT_LIST_CAPACITY - 1; turns++) {
                    tick += unit->ability_ct;
                    key = tick;
                    if (key >= 0x100) {
                        break;
                    }
                    key <<= 8;
                    placed += action_sort_at_list(slot, 1, key & 0xff00, list);
                }
            }
        }

        if (mode == BATTLE_AT_LIST_MODE_ACTIONS_ONLY) {
            continue;
        }

        slow = 0;
        haste = 0;
        current = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SLOW)];
        innate = unit->status_sets.innate[STATUS_BYTE_INDEX(STATUS_ID_SLOW)];
        if (current & STATUS_BYTE_MASK(STATUS_ID_SLOW)) {
            slow = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_SLOW)];
            if (innate & STATUS_BYTE_MASK(STATUS_ID_SLOW)) {
                slow = 0x200;
            }
            slow -= wait;
            if (slow < 0) {
                slow = 0;
            }
        }
        if (current & STATUS_BYTE_MASK(STATUS_ID_HASTE)) {
            haste = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_HASTE)];
            if (innate & STATUS_BYTE_MASK(STATUS_ID_HASTE)) {
                haste = 0x200;
            }
            haste -= wait;
            if (haste < 0) {
                haste = 0;
            }
        }

        speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
        if (speed == 0) {
            speed = 1;
        }
        tick = wait;
        ct = unit->ct;
        for (turns = 0; turns < BATTLE_AT_LIST_CAPACITY; turns++) {
            while (ct < UNIT_TURN_CT_THRESHOLD) {
                tick++;
                value = speed;
                if (slow != 0) {
                    value >>= 1;
                    slow--;
                    if (value == 0) {
                        value = 1;
                    }
                    if (haste != 0) {
                        haste--;
                    }
                } else if (haste != 0) {
                    haste--;
                    value = value * 3 >> 1;
                }
                ct += value;
                if (tick >= 0x1f5) {
                    break;
                }
            }
            if (tick >= 0x100) {
                break;
            }
            remainder = ct;
            remainder %= UNIT_TURN_CT_THRESHOLD;
            ct = remainder;
            added = action_sort_at_list(slot, 0, ((tick << 8) + (UNIT_TURN_CT_THRESHOLD - remainder)) & 0xffff, list);
            if (added == 0) {
                break;
            }
            placed += added;
        }
    }

    return -(placed == 0);
}

/* Insert an entry into the 40-entry AT list ahead of the first one with a
 * larger key; the last entry drops off. Mode 0 is a unit's turn (the battle ID).
 * Mode 1 is a charged ability landing: battle ID + 0x40 with the ability id's
 * low byte in action_id_low, battle ID - 0x40 for an ability id of 0x100 or more, or
 * battle ID + 0x60 for Jump. Returns 1 when the entry was placed. */
s32 action_sort_at_list(s32 battle_id, s32 mode, s32 sort_key, battle_at_entry_t* list) {
    unit_stats_t* unit;
    s32 i;
    s32 j;
    s32 unit_code;
    s32 ability_id;
    s32 ability_code;
    s32 cmp;
    u16 key = sort_key;

    unit = &g_battle_unit_stats[battle_id];
    for (i = 0; i < BATTLE_AT_LIST_CAPACITY; i++) {
        cmp = key < list[i].sort_key;
        ability_code = battle_id + BATTLE_AT_ACTION_ENTRY;
        if (cmp) {
            for (j = BATTLE_AT_LIST_CAPACITY - 1; j > i; j--) {
                list[j] = list[j - 1];
            }
            unit_code = battle_id;
            list[i].sort_key = key;
            if (mode != 0) {
                unit_code = ability_code;
                if (unit->action_command.skillset_id == SKILLSET_ID_JUMP) {
                    unit_code = battle_id + (BATTLE_AT_ACTION_ENTRY | BATTLE_AT_JUMP);
                } else {
                    ability_id = unit->action_command.ability_id;
                    list[i].action_id_low = ability_id;
                    if (ability_id >= 0x100) {
                        unit_code = battle_id - BATTLE_AT_ACTION_ENTRY;
                    }
                }
            }
            list[i].unit_code = unit_code;
            break;
        }
    }
    return i != BATTLE_AT_LIST_CAPACITY;
}

void action_set_only_action_taken(s32 unit_id) {
    action_set_move_act_flags(unit_id, 0, 1);
}

void action_set_only_movement_taken(s32 unit_id) {
    action_set_move_act_flags(unit_id, 1, 0);
}

/* Mark the unit's move and/or action as taken and end its turn once nothing is
 * left to do. -1 for an absent unit, 1 when the turn ended, otherwise 0. */
s32 action_set_move_act_flags(s32 idx, s32 move_flag, s32 act_flag) {
    unit_stats_t* unit;
    s32 ret;
    unit = &g_battle_unit_stats[idx];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE)
        return -1;
    if (move_flag != 0) {
        unit->movement_taken = 1;
    }
    if (act_flag != 0) {
        unit->action_taken = 1;
    }
    ret = action_should_end_unit_turn(unit);
    if (ret != 0) {
        ret = action_end_turn(idx);
    }
    return ret;
}

/* End the unit's turn: CT gains 20 when it neither moved nor has Don't Move,
 * and 20 when it neither acted nor has Don't Act or Performing; the result is
 * capped at 60. Always returns 1. */
s32 action_end_turn(s32 unit_id) {
    unit_stats_t* unit;
    u16 ct;
    u8 status4;
    u8 movement_check;
    u8 action_check;
    u32 result;

    unit = &g_battle_unit_stats[unit_id];
    if (g_battle_unit_stats[unit_id].entd_slot == BATTLE_ENTD_SLOT_NONE)
        return 1;
    if (unit->has_turn == 0)
        return 1;
    ct = unit->ct;
    status4 = unit->status_sets.current[4];
    movement_check = unit->movement_taken | (status4 & STATUS_BYTE_MASK(STATUS_ID_DONT_MOVE));
    action_check = (status4 & STATUS_BYTE_MASK(STATUS_ID_DONT_ACT)) | unit->action_taken
        | (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PERFORMING)] & STATUS_BYTE_MASK(STATUS_ID_PERFORMING));
    if (movement_check == 0) {
        ct += 0x14;
    }
    result = ct & 0xFFFF;
    if (action_check == 0) {
        ct += 0x14;
        result = ct & 0xFFFF;
    }
    if (result >= 0x3D)
        ct = 0x3C;
    unit->ct = ct;
    unit->has_turn = 0;
    return 1;
}

/* Returns whether a unit with no remaining move or action should end its turn. */
s32 action_should_end_unit_turn(const unit_stats_t* unit) {
    s32 cannot_act;
    u8 no_move;
    u8 no_action;
    u8 taken;

    cannot_act = battle_unit_get_action_restriction(unit);
    no_move = unit->movement_taken
        | (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_MOVE)] & STATUS_BYTE_MASK(STATUS_ID_DONT_MOVE));
    no_action = unit->action_taken | cannot_act;
    taken = unit->movement_taken | unit->action_taken;
    if (no_move != 0 && no_action != 0 && taken != 0) {
        return 1;
    }
    return 0;
}

/* 1 when the unit's turn is over. A status that blocks turns
 * (status_calculate_turn_flags) ends it here. */
s32 action_check_change_of_turn(s32 unit_id) {
    unit_stats_t* unit = &g_battle_unit_stats[unit_id];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return 1;
    }
    if (!(status_calculate_turn_flags(unit) & BATTLE_TURN_STATUS_BLOCKS_TURN_MASK)) {
        return unit->has_turn == 0;
    }
    unit->has_turn = 0;
    return 1;
}
