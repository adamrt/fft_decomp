/* Advance the battle clock through turns, charged abilities, status upkeep
 * and Mimes. The AT List predicts the same units and charged actions without
 * advancing the live clock; turn completion tracks movement and actions. */

#include "fft/action.h"
#include "fft/ai_decision.h"
#include "fft/at_list.h"
#include "fft/job.h"
#include "fft/skillset.h"
#include "fft/status.h"
#include "fft/turn.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/libc.h"
#include "psx/types.h"

static u8 g_turn_pending_flags[21] = { 0 };

static s32 turn_set_move_act_taken(s32 idx, s32 move_flag, s32 act_flag);
static s32 turn_should_end(const unit_stats_t* unit);
static s32 at_list_insert_entry(s32 battle_id, s32 mode, s32 sort_key, at_entry_t* list);
static s32 status_build_upkeep_action(s32 unit_id, unit_stats_t* unit);
static s32 at_list_is_unit_excluded(const unit_stats_t* unit);

/* The clocktick loop between turns. RESET clears every unit's CT and turn state; FIND_ACTIVE_UNIT
 * returns 0xd000 | the first unit not Dead, Crystal, Jumping, Petrified or Treasure (0xdf00 if
 * none). Otherwise it steps the between-turn states (CT gain, the 100-CT pick, charge and status
 * countdowns, poison and regen, mimes) until one yields a turn_event_e | unit id, giving up
 * with TURN_EVENT_NONE after 1201 steps. */
s32 turn_run_clock(turn_clock_mode_e mode) {
    unit_stats_t* unit;
    s32 result = 0;
    s32 count = 0;
    s32 i;
    s32 best;
    s32 ret;
    s32 flags;
    u16 max_ct;
    u16 speed;
    u16 ct;
    u8 value;

    if (mode == TURN_CLOCK_MODE_RESET) {
        g_turn_clock_state = TURN_CLOCK_STATE_ADD_CT;
        g_turn_unit_id = -1;
        for (i = UNIT_TEAM_COUNT - 1; i >= 0; i--) {
            g_action_team_golem[i] = 0;
        }
        g_action_mimic_attacker.mimic_pending = 0;

        for (i = 0; i < UNIT_SLOT_COUNT; i++) {
            unit = &g_unit_stats[i];
            unit->ct = 0;
            unit->has_turn = 0;
            unit->ability_outcome = 0;
            unit->charged_ability_ct = 0xff;
        }

        ai_simulate_end();
        return 0xe000;
    }

    if (mode == TURN_CLOCK_MODE_FIND_ACTIVE_UNIT) {
        for (i = 0; i < UNIT_SLOT_COUNT; i++) {
            unit = &g_unit_stats[i];
            if (unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE) {
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
        switch (g_turn_clock_state) {
        case TURN_CLOCK_STATE_ADD_CT:
            for (i = 0; i < UNIT_SLOT_COUNT; i++) {
                unit = &g_unit_stats[i];
                flags = turn_calculate_status_flags(unit);
                if (!(flags & TURN_STATUS_CT_FROZEN)) {
                    speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
                    if (flags & TURN_STATUS_SLOW) {
                        speed >>= 1;
                    } else if (flags & TURN_STATUS_HASTE) {
                        speed += speed >> 1;
                    }
                    ct = unit->ct + speed;
                    /* The tick caps CT at 0xFE; only Quick sets 0xFF. */
                    if (ct >= 0xff) {
                        ct = 0xfe;
                    }
                    unit->ct = ct;
                }
            }

            g_turn_clock_state = TURN_CLOCK_STATE_PICK_UNIT;
            break;
        case TURN_CLOCK_STATE_PICK_UNIT:
            max_ct = UNIT_TURN_CT_THRESHOLD - 1;
            best = 0xff;
            for (i = 0; i < UNIT_SLOT_COUNT; i++) {
                unit = &g_unit_stats[i];
                flags = turn_calculate_status_flags(unit);
                if (!(flags & TURN_STATUS_CT_FROZEN)) {
                    if (max_ct < unit->ct) {
                        best = i;
                        max_ct = unit->ct;
                    }
                }
            }

            if (best != 0xff) {
                unit = &g_unit_stats[best];
                /* A jumping unit waits at 99 CT, ready to act once it lands; pick again. */
                if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_JUMP)] & STATUS_BYTE_MASK(STATUS_ID_JUMP)) {
                    unit->ct = UNIT_TURN_CT_THRESHOLD - 1;
                    continue;
                }

                /* A Quick target (CT 0xFF) restarts from 0; otherwise CT beyond 100 carries over. */
                if (unit->ct == 0xff) {
                    unit->ct = 0;
                } else {
                    unit->ct = max_ct % UNIT_TURN_CT_THRESHOLD;
                }

                ret = status_build_upkeep_action(best, unit);
                if (ret != 0) {
                    result = best | TURN_EVENT_ACTION_RESULT;
                    /* A crystal or treasure (1) or an expired Death Sentence (3) ends here without
                     * a turn; only the first counts as a casualty. */
                    if (ret & 1) {
                        if (ret == 1) {
                            unit_increase_casualty_counters(unit);
                        }
                        return result;
                    }
                }

                /* A staged revival or other change (2) gets its turn even when the unit can't act. */
                flags = turn_calculate_status_flags(unit);
                if (!(flags & (TURN_STATUS_CT_FROZEN | TURN_STATUS_INCAPACITATED | TURN_STATUS_DEAD)) || ret == 2) {
                    unit->has_turn = 1;
                    unit->movement_taken = 0;
                    unit->action_taken = 0;
                    unit->transparent_removal_flag = 1;
                    if (ret == 0) {
                        result = best | TURN_EVENT_UNIT_READY;
                    }
                    g_turn_clock_resume_state = g_turn_clock_state;
                    g_turn_unit_id = best;
                    g_turn_clock_state = TURN_CLOCK_STATE_UNIT_TURN;
                    return result;
                }
                continue;
            }

            g_turn_clock_state = TURN_CLOCK_STATE_TICK_CHARGES;
            break;
        case TURN_CLOCK_STATE_TICK_CHARGES:
            i = 0;
            do {
                unit = &g_unit_stats[i];
                flags = turn_calculate_status_flags(unit);
                if (!(flags & TURN_STATUS_BLOCKS_WAIT_MENU_MASK)) {
                    value = unit->charged_ability_ct;
                    if (value != 0xff && value != 0) {
                        value--;
                        unit->charged_ability_ct = value;
                    }
                }
                /* clang-format off */
            } while (++i < UNIT_SLOT_COUNT); g_turn_clock_state = TURN_CLOCK_STATE_RESOLVE_CHARGE;
            /* clang-format on */
            break;
        case TURN_CLOCK_STATE_RESOLVE_CHARGE:
            for (i = 0; i < UNIT_SLOT_COUNT; i++) {
                unit = &g_unit_stats[i];
                flags = turn_calculate_status_flags(unit);
                if (!(flags & TURN_STATUS_BLOCKS_WAIT_MENU_MASK) && unit->charged_ability_ct == 0) {
                    /* A performer's song or dance starts charging again instead of ending. */
                    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PERFORMING)]
                        & STATUS_BYTE_MASK(STATUS_ID_PERFORMING)) {
                        unit_copy_last_ability_ct(unit);
                        unit->charged_ability_ct = unit->ability_ct;
                    } else {
                        unit->charged_ability_ct = 0xff;
                    }
                    g_turn_clock_state = TURN_CLOCK_STATE_PICK_UNIT_AFTER_CHARGE;
                    result = i | TURN_EVENT_ABILITY_READY;
                    return result;
                }
            }

            g_turn_clock_state = TURN_CLOCK_STATE_EXPIRE_STATUSES;
            break;
        case TURN_CLOCK_STATE_EXPIRE_STATUSES:
            i = 0;
            do {
                unit = &g_unit_stats[i];
                if (unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE
                    && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_JUMP)]
                        & STATUS_BYTE_MASK(STATUS_ID_JUMP))) {
                    status_update_expiring(i);
                }
                /* clang-format off */
            } while (++i < UNIT_SLOT_COUNT); g_turn_clock_state = TURN_CLOCK_STATE_COLLECT_PENDING_ACTIONS;
            /* clang-format on */
            break;
        case TURN_CLOCK_STATE_COLLECT_PENDING_ACTIONS:
            /* The pending flags list the units whose expiring statuses left an
             * action result to apply; MARK_MIMES reuses them for Mimes. */
            i = 0;
            do {
                unit = &g_unit_stats[i];
                g_turn_pending_flags[i] = 0;
                if (unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE
                    && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_JUMP)]
                        & STATUS_BYTE_MASK(STATUS_ID_JUMP))) {
                    g_turn_pending_flags[i] = unit->action.attack_type;
                }
                /* clang-format off */
            } while (++i < UNIT_SLOT_COUNT); g_turn_clock_state = TURN_CLOCK_STATE_RUN_PENDING_ACTIONS;
            /* clang-format on */
            break;
        case TURN_CLOCK_STATE_RUN_PENDING_ACTIONS:
            for (i = 0; i < UNIT_SLOT_COUNT; i++) {
                if (g_turn_pending_flags[i] != 0) {
                    g_turn_pending_flags[i] = 0;
                    result = i | TURN_EVENT_ACTION_RESULT;
                    return result;
                }
            }

            g_turn_clock_state = TURN_CLOCK_STATE_ADD_CT;
            break;
        case TURN_CLOCK_STATE_PICK_UNIT_AFTER_CHARGE:
            /* Only a Quick target (CT 0xFF, above the tick's 0xFE cap) takes its turn here. */
            max_ct = 0xfe;
            best = 0xff;
            for (i = 0; i < UNIT_SLOT_COUNT; i++) {
                flags = turn_calculate_status_flags(&g_unit_stats[i]);
                if (!(flags & TURN_STATUS_CT_FROZEN)) {
                    if (max_ct < g_unit_stats[i].ct) {
                        best = i;
                        max_ct = g_unit_stats[i].ct;
                    }
                }
            }

            if (best != 0xff) {
                unit = &g_unit_stats[best];
                unit->has_turn = 1;
                unit->transparent_removal_flag = 1;
                unit->ct = 0;
                unit->movement_taken = 0;
                unit->action_taken = 0;
                g_turn_clock_resume_state = TURN_CLOCK_STATE_PICK_UNIT_AFTER_CHARGE;
                g_turn_unit_id = best;
                g_turn_clock_state = TURN_CLOCK_STATE_UNIT_TURN;
                result = best | TURN_EVENT_UNIT_READY;
                return result;
            }

            g_turn_clock_resume_state = TURN_CLOCK_STATE_RESOLVE_CHARGE;
            g_turn_clock_state = TURN_CLOCK_STATE_MARK_MIMES;
            break;
        case TURN_CLOCK_STATE_UNIT_TURN:
            for (i = 0; i < UNIT_SLOT_COUNT; i++) {
                unit = &g_unit_stats[i];
                if (unit->has_turn != 0) {
                    s32 unit_status;
                    s32 end_turn;

                    unit_status = turn_calculate_status_flags(unit);
                    end_turn = turn_should_end(unit);
                    if ((unit_status & TURN_STATUS_CT_FROZEN) || end_turn != 0) {
                        turn_end(i);
                        unit->has_turn = 0;
                    } else {
                        result = i | TURN_EVENT_UNIT_READY;
                        return result;
                    }
                }
            }

            g_turn_clock_state = TURN_CLOCK_STATE_POISON_AND_REGEN;
            break;
        case TURN_CLOCK_STATE_POISON_AND_REGEN:
            g_turn_clock_state = TURN_CLOCK_STATE_TERRAIN_POISON;
            if (g_turn_unit_id != -1) {
                unit = &g_unit_stats[g_turn_unit_id];
                if (status_prepare_poison_and_regen(unit)) {
                    result = g_turn_unit_id | TURN_EVENT_ACTION_RESULT;
                    return result;
                }
            }
            break;
        case TURN_CLOCK_STATE_TERRAIN_POISON:
            g_turn_clock_state = TURN_CLOCK_STATE_REMOVE_TRANSPARENT;
            if (g_turn_unit_id != -1) {
                unit = &g_unit_stats[g_turn_unit_id];
                if (status_prepare_terrain_poison(unit)) {
                    result = g_turn_unit_id | TURN_EVENT_ACTION_RESULT;
                    return result;
                }
            }
            break;
        case TURN_CLOCK_STATE_REMOVE_TRANSPARENT:
            g_turn_clock_state = TURN_CLOCK_STATE_MARK_MIMES;
            if (g_turn_unit_id != -1) {
                unit = &g_unit_stats[g_turn_unit_id];
                if (status_prepare_transparent_removal(unit)) {
                    result = g_turn_unit_id | TURN_EVENT_ACTION_RESULT;
                    return result;
                }
            }
            break;
        case TURN_CLOCK_STATE_MARK_MIMES:
            if (g_action_mimic_attacker.mimic_pending != 0) {
                i = 0;
                do {
                    unit = &g_unit_stats[i];
                    g_turn_pending_flags[i] = 0;
                    if (unit->job_id == JOB_ID_MIME && unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE) {
                        g_turn_pending_flags[i] = 1;
                    }
                    /* clang-format off */
                } while (++i < UNIT_SLOT_COUNT); g_turn_clock_state = TURN_CLOCK_STATE_RUN_MIMES;
                /* clang-format on */
            } else {
                g_turn_clock_state = g_turn_clock_resume_state;
            }
            break;
        case TURN_CLOCK_STATE_RUN_MIMES:
            g_action_mimic_attacker.mimic_pending = 0;
            for (i = 0; i < UNIT_SLOT_COUNT; i++) {
                unit = &g_unit_stats[i];
                if (g_turn_pending_flags[i] != 0 && unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE) {
                    g_turn_pending_flags[i] = 0;
                    ret = action_mimic_set_ability(unit);
                    if (ret == 1) {
                        result = i | TURN_EVENT_ABILITY_READY;
                        return result;
                    }
                    if (ret == -1) {
                        result = i | TURN_EVENT_MIME;
                        return result;
                    }
                }
            }

            g_turn_clock_state = g_turn_clock_resume_state;
            break;
        }

        count++;
        if (count >= 1201) {
            break;
        }
    }

    return TURN_EVENT_NONE;
}

/*
 * Per-turn status upkeep: stage the change the unit's current status set
 * produces this tick in the unit's action record.
 *
 * Returns 2 for a Reraise or undead revival and for any other staged change
 * (Defending removed, +1 Brave for a Chicken with Brave below 10), 1 when the
 * death counter runs out and the unit becomes a crystal or treasure, 3 when
 * Death Sentence expires on a unit that is not undead, and 0 when nothing happens.
 *
 * Reraise revives with a tenth of max HP; an Undead unit whose death counter
 * runs out revives on a coin flip with 1 to max HP. Outside the execute action
 * state (previews and AI simulations) a dead unit's counter does not move.
 */
static s32 status_build_upkeep_action(s32 unit_id, unit_stats_t* unit) {
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
        if (unit->team_flags & UNIT_TEAM_FLAG_IMMORTAL) {
            return 0;
        }
        if (unit->unit_flags & (UNIT_FLAG_SAVE_FORMATION | UNIT_FLAG_LOAD_FORMATION)) {
            return 0;
        }
        /* The unit leaves on the tick its counter is already 0. */
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
        /* Any set bit of the mask gives a crystal: 1 in 2 units, or 3 in 4 for a
         * unit that started player-controlled; treasure otherwise. */
        mask = 0x10;
        if (unit->initial_team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) {
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
        action->brave_change = ACTION_STAT_CHANGE_INCREASE | 1;
        action->attack_type |= ACTION_TYPE_PSEUDO_STATUS;
    }
    return (action->attack_type != 0) * 2;
}

/* Returns the TURN_STATUS_* flags that decide how a unit takes part in the
 * turn order.
 *
 * CT_FROZEN alone for an empty slot, a unit with a status of the freeze-CT set or
 * a ridden mount; INCAPACITATED | DEAD for a dead unit; otherwise INCAPACITATED |
 * SLEEP for Sleep plus the HASTE and SLOW bits. The retail status data puts Sleep
 * in the freeze-CT set, which leaves the SLEEP result unreachable. */
s32 turn_calculate_status_flags(const unit_stats_t* unit) {
    s32 status1;
    s32 status4;
    s32 status5;
    s32 flags;

    if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
        return TURN_STATUS_CT_FROZEN;
    }
    if (status_unit_has_any_in_set(unit, STATUS_CHECK_SET_FREEZE_CT) != 0) {
        return TURN_STATUS_CT_FROZEN;
    }
    if ((unit->mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT) != 0) {
        return TURN_STATUS_CT_FROZEN;
    }
    status1 = unit->status_sets.current[0];
    status4 = unit->status_sets.current[3];
    status5 = unit->status_sets.current[4];
    if ((status1 & STATUS_BYTE_MASK(STATUS_ID_DEAD)) != 0) {
        return TURN_STATUS_INCAPACITATED | TURN_STATUS_DEAD;
    }
    flags = -((status5 & STATUS_BYTE_MASK(STATUS_ID_SLEEP)) != 0) & (TURN_STATUS_INCAPACITATED | TURN_STATUS_SLEEP);
    if ((status4 & STATUS_BYTE_MASK(STATUS_ID_HASTE)) != 0) {
        flags |= TURN_STATUS_HASTE;
    }
    if ((status4 & STATUS_BYTE_MASK(STATUS_ID_SLOW)) != 0) {
        flags |= TURN_STATUS_SLOW;
    }
    return flags;
}

/* Determine whether either side has met its battle-ending condition.
 *
 * Returns -1 when the blue side is defeated, 0 when all opposing units are
 * defeated, and 1 while both sides can continue. A unit of importance 3 (whose
 * loss decides the battle) that is disabled or lost decides it at once:
 * defeat for a player-team unit, victory for any other. Otherwise the side
 * left with no unit free of the KO status set loses, the player side first. */
s32 event_check_battle_outcome(void) {
    s32 player_unit_count = 0;
    s32 enemy_unit_count = 0;
    s32 unit_index;
    s32 selected;
    unit_stats_t* unit;

    for (unit_index = 0; unit_index < UNIT_SLOT_COUNT; unit_index++) {
        unit = &g_unit_stats[unit_index];
        if ((unit->initial_team_flags & UNIT_TEAM_IMPORTANCE_MASK) == UNIT_TEAM_IMPORTANCE_MASK) {
            if (unit->existence != UNIT_EXISTENCE_NONE) {
                selected = unit->existence == UNIT_EXISTENCE_DISABLED;
                selected += status_unit_has_any_in_set(unit, STATUS_CHECK_SET_UNIT_LOST);
                if (selected != 0) {
                    if (unit->initial_team_flags & UNIT_TEAM_MASK) {
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

    for (unit_index = 0; unit_index < UNIT_SLOT_COUNT; unit_index++) {
        unit = &g_unit_stats[unit_index];
        if (unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE) {
            selected = unit->initial_team_flags & UNIT_TEAM_MASK;
            if (status_unit_has_any_in_set(unit, STATUS_CHECK_SET_KO) == 0) {
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
 * Returns -1 for an empty slot, -3 for a unit the AT list leaves out (Dead,
 * Crystal, Petrify, Treasure or a ridden mount) and -2 when the unit is not on
 * the list. */
s32 at_list_find_unit(const unit_stats_t* unit) {
    at_entry_t at_list[AT_LIST_CAPACITY];
    s32 i;
    at_entry_t* entry;
    u8 battle_id;

    if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE)
        return -1;
    if (at_list_is_unit_excluded(unit) != 0)
        return -3;
    battle_id = unit->battle_id;
    at_list_build(at_list, AT_LIST_MODE_TURNS_ONLY);
    for (i = 0; i < AT_LIST_CAPACITY; i++) {
        entry = &at_list[i];
        if ((entry->unit_code & AT_UNIT_ID_MASK) == battle_id) {
            return i;
        }
    }
    return -2;
}

/* Rebuild the AT list and return the unit id of the chosen entry.
 *
 * Returns AT_LIST_ENTRY_EMPTY for an empty entry; a charged-action
 * entry (AT_ACTION_ENTRY) returns its unit id plus
 * AT_LIST_ENTRY_CHARGED_ACTION. */
s32 at_list_get_entry_unit_id(s32 at_index) {
    at_entry_t at_list[AT_LIST_CAPACITY];
    u32 entry;
    u32 unit_id;

    at_list_build(at_list, AT_LIST_MODE_ALL);
    entry = at_list[at_index].unit_code;
    unit_id = entry & AT_UNIT_ID_MASK;
    if (unit_id == AT_END_OF_LIST) {
        return AT_LIST_ENTRY_EMPTY;
    }
    if (entry & AT_ACTION_ENTRY) {
        unit_id += AT_LIST_ENTRY_CHARGED_ACTION;
    }
    return unit_id;
}

/* Return 1 when the unit cannot participate in the AT list. */
static s32 at_list_is_unit_excluded(const unit_stats_t* unit) {
    if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
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
    if (unit->mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT) {
        return 1;
    }
    return 0;
}

/* Preview where a planned action lands on the AT list.
 *
 * Runs the attack preparation for action in the preview state, rebuilds
 * at_list and counts the turns until the action resolves, then restores the
 * unit's action data, CT, move/act flags and statuses. */
s32 at_list_preview(unit_stats_t* unit, action_command_t* action, at_entry_t* at_list) {
    action_command_t saved_action;
    action_command_t* action_data = &unit->action_command;
    s32 old_state;
    s32 turns;
    u8 old_charged_ability_ct;
    u8 old_inflicted_status;
    u8 old_movement_taken;
    u8 old_action_taken;

    action_copy_data(action_data, &saved_action);
    old_charged_ability_ct = unit->charged_ability_ct;
    old_inflicted_status = unit->inflicted_status[0];
    old_movement_taken = unit->movement_taken;
    old_action_taken = unit->action_taken;
    old_state = g_action_state;
    g_action_state = ACTION_STATE_PREVIEW;
    if (action_commit_preparation(action) == ACTION_PREPARATION_IMMEDIATE) {
        unit->charged_ability_ct = 0;
    }
    turns = at_list_build(at_list, AT_LIST_MODE_PREVIEW);
    if (turns != -1) {
        turns = at_list_count_turns_to_resolve(unit->battle_id, unit->charged_ability_ct, at_list);
    }
    g_action_state = old_state;
    unit->movement_taken = old_movement_taken;
    unit->action_taken = old_action_taken;
    unit->inflicted_status[0] = old_inflicted_status;
    status_store_current(unit);
    unit->charged_ability_ct = old_charged_ability_ct;
    action_copy_data(&saved_action, action_data);
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
s32 at_list_build(at_entry_t* list, at_list_mode_e mode) {
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
    phase_tick = g_turn_clock_resume_state == TURN_CLOCK_STATE_PICK_UNIT_AFTER_CHARGE;
    for (slot = 0; slot < AT_LIST_CAPACITY; slot++) {
        list[slot].unit_code = 0xff;
        list[slot].action_id_low = 0xff;
        list[slot].sort_key = 0xffff;
    }

    if (mode != AT_LIST_MODE_ACTIONS_ONLY) {
        /* The unit taking its turn heads the list with key 0. */
        unit = unit_find_active();
        if (unit != 0) {
            at_list_insert_entry(unit->battle_id, 0, 0, list);
        }
    }

    for (slot = 0; slot < UNIT_SLOT_COUNT; slot++) {
        unit = &g_unit_stats[slot];
        if (at_list_is_unit_excluded(unit) != 0) {
            continue;
        }

        /* Stop and Sleep delay every entry by their remaining CT. */
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
        if (value != 0xff && mode != AT_LIST_MODE_TURNS_ONLY) {
            tick = value + wait;
            if (value != 0 || unit->has_turn == 0 || mode != AT_LIST_MODE_PREVIEW) {
                tick += phase_tick;
            }
            placed += at_list_insert_entry(slot, 1, (tick << 8) & 0xff00, list);

            /* A performer repeats its song or dance every ability_ct ticks while the tick fits
             * the key's high byte. */
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PERFORMING)]
                & STATUS_BYTE_MASK(STATUS_ID_PERFORMING)) {
                unit_copy_last_ability_ct(unit);
                for (turns = 0; turns < AT_LIST_CAPACITY - 1; turns++) {
                    tick += unit->ability_ct;
                    key = tick;
                    if (key >= 0x100) {
                        break;
                    }
                    key <<= 8;
                    placed += at_list_insert_entry(slot, 1, key & 0xff00, list);
                }
            }
        }

        if (mode == AT_LIST_MODE_ACTIONS_ONLY) {
            continue;
        }

        slow = 0;
        haste = 0;
        current = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SLOW)];
        innate = unit->status_sets.innate[STATUS_BYTE_INDEX(STATUS_ID_SLOW)];
        if (current & STATUS_BYTE_MASK(STATUS_ID_SLOW)) {
            slow = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_SLOW)];
            /* Innate Slow or Haste outlasts the simulation (0x200 ticks). */
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
        /* A zero Speed would never reach 100 CT. */
        if (speed == 0) {
            speed = 1;
        }

        tick = wait;
        ct = unit->ct;
        for (turns = 0; turns < AT_LIST_CAPACITY; turns++) {
            while (ct < UNIT_TURN_CT_THRESHOLD) {
                tick++;
                value = speed;
                /* Slow takes precedence over Haste, but both count down. */
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
                /* Give up on a unit that cannot reach 100 CT within 500 ticks. */
                if (tick >= 501) {
                    break;
                }
            }

            /* Keys hold the tick in one byte. */
            if (tick >= 0x100) {
                break;
            }

            remainder = ct;
            remainder %= UNIT_TURN_CT_THRESHOLD;
            ct = remainder;
            /* Within a tick, the unit that overshoots 100 CT further sorts first. */
            added = at_list_insert_entry(slot, 0, ((tick << 8) + (UNIT_TURN_CT_THRESHOLD - remainder)) & 0xffff, list);
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
 * Mode 1 is a charged ability landing: battle ID + AT_ACTION_ENTRY with
 * the ability id's low byte in action_id_low, or battle ID + AT_ACTION_ENTRY
 * + AT_JUMP for Jump. For an ability id of 0x100 or more the code is battle
 * ID - 0x40, which as a byte is battle ID + 0xc0: the action entry plus
 * AT_ABILITY_ID_HIGH_BIT. Returns 1 when the entry was placed. */
static s32 at_list_insert_entry(s32 battle_id, s32 mode, s32 sort_key, at_entry_t* list) {
    unit_stats_t* unit;
    s32 i;
    s32 j;
    s32 unit_code;
    s32 ability_id;
    s32 ability_code;
    s32 cmp;
    u16 key = sort_key;

    unit = &g_unit_stats[battle_id];
    for (i = 0; i < AT_LIST_CAPACITY; i++) {
        cmp = key < list[i].sort_key;
        ability_code = battle_id + AT_ACTION_ENTRY;
        if (cmp) {
            for (j = AT_LIST_CAPACITY - 1; j > i; j--) {
                list[j] = list[j - 1];
            }
            unit_code = battle_id;
            list[i].sort_key = key;
            if (mode != 0) {
                unit_code = ability_code;
                if (unit->action_command.skillset_id == SKILLSET_ID_JUMP) {
                    unit_code = battle_id + (AT_ACTION_ENTRY | AT_JUMP);
                } else {
                    ability_id = unit->action_command.ability_id;
                    list[i].action_id_low = ability_id;
                    if (ability_id >= 0x100) {
                        unit_code = battle_id - AT_ACTION_ENTRY;
                    }
                }
            }
            list[i].unit_code = unit_code;
            break;
        }
    }
    return i != AT_LIST_CAPACITY;
}

/* Mark the unit's action as taken (its move flag is left as is) and end its turn if nothing is
 * left to do. */
void turn_set_act_taken(s32 unit_id) {
    turn_set_move_act_taken(unit_id, 0, 1);
}

/* Mark the unit's move as taken (its action flag is left as is) and end its turn if nothing is
 * left to do. */
void turn_set_move_taken(s32 unit_id) {
    turn_set_move_act_taken(unit_id, 1, 0);
}

/* Mark the unit's move and/or action as taken and end its turn once nothing is
 * left to do. -1 for an absent unit, 1 when the turn ended, otherwise 0. */
static s32 turn_set_move_act_taken(s32 unit_id, s32 move_flag, s32 act_flag) {
    unit_stats_t* unit = &g_unit_stats[unit_id];
    s32 ret;

    if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE)
        return -1;
    if (move_flag != 0) {
        unit->movement_taken = 1;
    }
    if (act_flag != 0) {
        unit->action_taken = 1;
    }
    ret = turn_should_end(unit);
    if (ret != 0) {
        ret = turn_end(unit_id);
    }
    return ret;
}

/* End the unit's turn: CT gains 20 when it neither moved nor has Don't Move,
 * and 20 when it neither acted nor has Don't Act or Performing; the result is
 * capped at 60. As the turn start already left CT % 100, a turn costs 100 CT,
 * 80 with the move or the action unused, or 60 with both unused, and never
 * leaves more than 60. Always returns 1. */
s32 turn_end(s32 unit_id) {
    unit_stats_t* unit = &g_unit_stats[unit_id];
    u16 ct;
    u8 status5;
    u8 movement_check;
    u8 action_check;
    u32 result;

    if (g_unit_stats[unit_id].active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE)
        return 1;
    if (unit->has_turn == 0)
        return 1;
    ct = unit->ct;
    status5 = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_MOVE)];
    movement_check = unit->movement_taken | (status5 & STATUS_BYTE_MASK(STATUS_ID_DONT_MOVE));
    action_check = (status5 & STATUS_BYTE_MASK(STATUS_ID_DONT_ACT)) | unit->action_taken
        | (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PERFORMING)] & STATUS_BYTE_MASK(STATUS_ID_PERFORMING));
    if (movement_check == 0) {
        ct += TURN_UNUSED_CT_REFUND;
    }
    result = ct;
    if (action_check == 0) {
        ct += TURN_UNUSED_CT_REFUND;
        result = ct;
    }
    if (result > TURN_END_CT_MAX)
        ct = TURN_END_CT_MAX;
    unit->ct = ct;
    unit->has_turn = 0;
    return 1;
}

/* Return 1 when the unit's turn should end: it can no longer move (moved, or
 * Don't Move) nor act (acted, or an action restriction), and it has used at
 * least one of the two. */
static s32 turn_should_end(const unit_stats_t* unit) {
    s32 cannot_act = unit_get_action_restriction(unit);
    u8 no_move;
    u8 no_action;
    u8 taken;

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
 * (turn_calculate_status_flags) ends it here. */
s32 turn_is_over(s32 unit_id) {
    unit_stats_t* unit = &g_unit_stats[unit_id];
    if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
        return 1;
    }
    if (!(turn_calculate_status_flags(unit) & TURN_STATUS_BLOCKS_TURN_MASK)) {
        return unit->has_turn == 0;
    }
    unit->has_turn = 0;
    return 1;
}

/* End the unit's Charging, Jump, Defending and Performing statuses and drop its charged ability
 * (action state none). Same body as crystals_and_treasure.c's static status_cancel_charge. */
void status_clear_action_state(unit_stats_t* unit) {
    status_set_action_state(unit, STATUS_ACTION_STATE_NONE);
}

/* Put the unit in the Defending action state, which ends Charging, Jump and Performing and drops
 * any charged ability. Used by the Defend command and the AI's simulation of it; always returns 0. */
s32 status_inflict_defending(s32 unit_id) {
    status_set_action_state(&g_unit_stats[unit_id], STATUS_ACTION_STATE_DEFENDING);
    return 0;
}

/* Marks a present unit that has a formation index for removal: the slot is
 * emptied and its existence set to pending removal. Returns -1 when the slot is
 * empty or has no formation index, else 0. No retail code calls it. */
s32 unit_mark_pending_removal(s32 unit_id) {
    unit_stats_t* unit = &g_unit_stats[unit_id];

    if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE)
        return -1;
    if (unit->formation_index == UNIT_FORMATION_INDEX_NONE)
        return -1;
    unit->active_battle_id = UNIT_ACTIVE_BATTLE_ID_NONE;
    unit->existence = UNIT_EXISTENCE_PENDING_REMOVAL;
    return 0;
}
