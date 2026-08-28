#include "fft/battle.h"
#include "fft/battle_ai.h"
#include "fft/main_unit.h"
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
s32 battle_action_run_between_turn_control(s32 mode) {
    battle_stats_t* unit;
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
    if (mode == 1) {
        g_battle_between_turn_state = 0;
        g_battle_turn_unit_id = -1;
        for (i = 3; i >= 0; i--) {
            g_battle_team_golem[i] = 0;
        }
        g_current_ability_attacker.data_initialized = 0;
        for (i = 0; i < 21; i++) {
            unit = &g_battle_unit_stats[i];
            unit->ct = 0;
            unit->has_turn = 0;
            unit->ability_outcome = 0;
            unit->charged_ability_ct = 0xff;
        }
        battle_ai_restore_considered_action_data();
        return 0xe000;
    }
    if (mode == 2) {
        for (i = 0; i < 21; i++) {
            unit = &g_battle_unit_stats[i];
            if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
                if (!(unit->status_sets.current[0] & 0x64)) {
                    if (!(unit->status_sets.current[1] & 0x81)) {
                        return i | 0xd000;
                    }
                }
            }
        }
        return 0xdf00;
    }
    while (result == 0) {
        switch (g_battle_between_turn_state) {
        case 0:
            for (i = 0; i < 21; i++) {
                unit = &g_battle_unit_stats[i];
                flags = battle_status_check_unit(unit);
                if (!(flags & 1)) {
                    speed = unit->attributes[2];
                    if (flags & 0x20) {
                        speed >>= 1;
                    } else if (flags & 0x10) {
                        speed += speed >> 1;
                    }
                    ct = unit->ct + speed;
                    if (ct >= 0xff) {
                        ct = 0xfe;
                    }
                    unit->ct = ct;
                }
            }
            g_battle_between_turn_state = 1;
            break;
        case 1:
            max_ct = 99;
            best = 0xff;
            for (i = 0; i < 21; i++) {
                unit = &g_battle_unit_stats[i];
                flags = battle_status_check_unit(unit);
                if (!(flags & 1)) {
                    if (max_ct < unit->ct) {
                        best = i;
                        max_ct = unit->ct;
                    }
                }
            }
            if (best != 0xff) {
                unit = &g_battle_unit_stats[best];
                if (unit->status_sets.current[0] & 4) {
                    unit->ct = 99;
                    continue;
                }
                if (unit->ct == 0xff) {
                    unit->ct = 0;
                } else {
                    unit->ct = max_ct % 100;
                }
                ret = battle_status_build_upkeep_action(best, unit);
                if (ret != 0) {
                    result = best | 0x300;
                    if (ret & 1) {
                        if (ret == 1) {
                            main_unit_increase_casualty_counters(unit);
                        }
                        return result;
                    }
                }
                flags = battle_status_check_unit(unit);
                if (!(flags & 7) || ret == 2) {
                    unit->has_turn = 1;
                    unit->movement_taken = 0;
                    unit->action_taken = 0;
                    unit->transparent_removal_flag = 1;
                    if (ret == 0) {
                        result = best | 0x100;
                    }
                    g_battle_between_turn_resume_state = g_battle_between_turn_state;
                    g_battle_turn_unit_id = best;
                    g_battle_between_turn_state = 10;
                    return result;
                }
                continue;
            }
            g_battle_between_turn_state = 2;
            break;
        case 2:
            i = 0;
            do {
                unit = &g_battle_unit_stats[i];
                flags = battle_status_check_unit(unit);
                if (!(flags & 0xf)) {
                    value = unit->charged_ability_ct;
                    if (value != 0xff && value != 0) {
                        value--;
                        unit->charged_ability_ct = value;
                    }
                }
                /* clang-format off */
            } while (++i < 21); g_battle_between_turn_state = 3;
            /* clang-format on */
            break;
        case 3:
            for (i = 0; i < 21; i++) {
                unit = &g_battle_unit_stats[i];
                flags = battle_status_check_unit(unit);
                if (!(flags & 0xf) && unit->charged_ability_ct == 0) {
                    if (unit->status_sets.current[0] & 1) {
                        main_unit_copy_last_ability_ct(unit);
                        unit->charged_ability_ct = unit->ability_ct;
                    } else {
                        unit->charged_ability_ct = 0xff;
                    }
                    g_battle_between_turn_state = 9;
                    result = i | 0x200;
                    return result;
                }
            }
            g_battle_between_turn_state = 4;
            break;
        case 4:
            i = 0;
            do {
                unit = &g_battle_unit_stats[i];
                if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE && !(unit->status_sets.current[0] & 4)) {
                    battle_status_update_expiring(i);
                }
                /* clang-format off */
            } while (++i < 21); g_battle_between_turn_state = 5;
            /* clang-format on */
            break;
        case 5:
            i = 0;
            do {
                unit = &g_battle_unit_stats[i];
                g_battle_mimic_pending_flags[i] = 0;
                if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE && !(unit->status_sets.current[0] & 4)) {
                    g_battle_mimic_pending_flags[i] = unit->action.attack_type;
                }
                /* clang-format off */
            } while (++i < 21); g_battle_between_turn_state = 6;
            /* clang-format on */
            break;
        case 6:
            for (i = 0; i < 21; i++) {
                if (g_battle_mimic_pending_flags[i] != 0) {
                    g_battle_mimic_pending_flags[i] = 0;
                    result = i | 0x300;
                    return result;
                }
            }
            g_battle_between_turn_state = 0;
            break;
        case 9:
            max_ct = 0xfe;
            best = 0xff;
            for (i = 0; i < 21; i++) {
                flags = battle_status_check_unit(&g_battle_unit_stats[i]);
                if (!(flags & 1)) {
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
                g_battle_between_turn_resume_state = 9;
                g_battle_turn_unit_id = best;
                g_battle_between_turn_state = 10;
                result = best | 0x100;
                return result;
            }
            g_battle_between_turn_resume_state = 3;
            g_battle_between_turn_state = 13;
            break;
        case 10:
            for (i = 0; i < 21; i++) {
                unit = &g_battle_unit_stats[i];
                if (unit->has_turn != 0) {
                    s32 unit_status;
                    s32 end_turn;

                    unit_status = battle_status_check_unit(unit);
                    end_turn = battle_action_should_end_unit_turn(unit);
                    if ((unit_status & 1) || end_turn != 0) {
                        battle_action_end_turn(i);
                        unit->has_turn = 0;
                    } else {
                        result = i | 0x100;
                        return result;
                    }
                }
            }
            g_battle_between_turn_state = 19;
            break;
        case 19:
            g_battle_between_turn_state = 15;
            if (g_battle_turn_unit_id != -1) {
                unit = &g_battle_unit_stats[g_battle_turn_unit_id];
                if (battle_status_apply_poison_and_regen(unit)) {
                    result = g_battle_turn_unit_id | 0x300;
                    return result;
                }
            }
            break;
        case 15:
            g_battle_between_turn_state = 17;
            if (g_battle_turn_unit_id != -1) {
                unit = &g_battle_unit_stats[g_battle_turn_unit_id];
                if (battle_prepare_terrain_poison(unit)) {
                    result = g_battle_turn_unit_id | 0x300;
                    return result;
                }
            }
            break;
        case 17:
            g_battle_between_turn_state = 13;
            if (g_battle_turn_unit_id != -1) {
                unit = &g_battle_unit_stats[g_battle_turn_unit_id];
                if (battle_status_remove_transparent(unit)) {
                    result = g_battle_turn_unit_id | 0x300;
                    return result;
                }
            }
            break;
        case 13:
            if (g_current_ability_attacker.data_initialized != 0) {
                i = 0;
                do {
                    unit = &g_battle_unit_stats[i];
                    g_battle_mimic_pending_flags[i] = 0;
                    if (unit->job_id == JOB_ID_MIME && unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
                        g_battle_mimic_pending_flags[i] = 1;
                    }
                    /* clang-format off */
                } while (++i < 21); g_battle_between_turn_state = 14;
                /* clang-format on */
            } else {
                g_battle_between_turn_state = g_battle_between_turn_resume_state;
            }
            break;
        case 14:
            g_current_ability_attacker.data_initialized = 0;
            for (i = 0; i < 21; i++) {
                unit = &g_battle_unit_stats[i];
                if (g_battle_mimic_pending_flags[i] != 0 && unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
                    g_battle_mimic_pending_flags[i] = 0;
                    ret = battle_action_set_mimic_ability(unit);
                    if (ret == 1) {
                        result = i | 0x200;
                        return result;
                    }
                    if (ret == -1) {
                        result = i | 0x500;
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
    return 0xff00;
}
