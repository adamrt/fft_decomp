#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/battle_status.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/script.h"
#include "psx/types.h"

s32 status_resolve_unit_changes(s32 unit_id, s32 removal_only) {
    unit_action_status_changes_t saved;

    if (g_battle_unit_stats[unit_id].entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    mem_copy_byte_data_source_first(&g_action_target_data->status_changes, &saved, sizeof(saved));
    g_action_current_ability.target_id = unit_id;
    g_action_target = &g_battle_unit_stats[unit_id];
    g_action_target_data = &g_battle_unit_stats[unit_id].action;
    status_modify_inflictions(removal_only);
    status_set_inflicted_ct_and_transfer_last_used_ct(unit_id);
    status_store_for_current_attack(unit_id, removal_only);
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD)) {
        g_action_target->hp = 0;
    }
    if (!(g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
            & (STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_JUMP)
                | STATUS_BYTE_MASK(STATUS_ID_PERFORMING)))) {
        g_action_target->charged_ability_ct = 0xFF;
    }
    status_remove_control(g_action_target);
    mem_copy_byte_data_source_first(&saved, &g_action_target_data->status_changes, sizeof(saved));
    return 0;
}

/* Confusion, Blood Suck, Berserk, Chicken and Charm take the unit out of
 * player control; without them the control bit comes back from the initial
 * team flags. Without Charm the team bits come back from the initial team as
 * well. */
void status_remove_control(unit_stats_t* unit) {
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CONFUSION)]
            & (STATUS_BYTE_MASK(STATUS_ID_CONFUSION) | STATUS_BYTE_MASK(STATUS_ID_BLOOD_SUCK)))
        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_BERSERK)]
            & (STATUS_BYTE_MASK(STATUS_ID_BERSERK) | STATUS_BYTE_MASK(STATUS_ID_CHICKEN)))
        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARM)] & STATUS_BYTE_MASK(STATUS_ID_CHARM))) {
        unit->team_flags = unit->team_flags & ~BATTLE_TEAM_FLAG_PLAYER_CONTROLLED;
    } else {
        unit->team_flags = unit->team_flags | (unit->initial_team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARM)] & STATUS_BYTE_MASK(STATUS_ID_CHARM)) == 0) {
        unit->team_flags = (unit->team_flags & ~BATTLE_TEAM_MASK) | (unit->initial_team_flags & BATTLE_TEAM_MASK);
    }
}

/* Apply the removals of the unit's action result: each removed status leaves
 * inflicted_status, has its CT cleared and, when executing, its graphics flag
 * switched off; then the current set is rebuilt as innate | inflicted. */
void status_set_inflicted_ct_and_transfer_last_used_ct(s32 unit_idx) {
    unit_stats_t* unit;
    s32 i;
    s32 mask;
    s32 idx;

    unit = &g_battle_unit_stats[unit_idx];
    for (i = 0; i < STATUS_COUNT; i++) {
        idx = i / 8;
        mask = 0x80 >> (i & 7);
        if (unit->action.status_changes.removal[idx] & mask) {
            unit->inflicted_status[idx] = unit->inflicted_status[idx] & ~mask;
            if (main_status_set_ct(unit, i, 1) == 0) {
                status_enable_special_flags(STATUS_HANDLER_INDEX(i), 0, unit_idx);
            }
        }
    }
    main_status_store_current(unit);
}

/* Set when a player-team unit becomes Crystal or Dead. */

/* Applies the pending status inflictions of unit_id's current action.
 *
 * For each of the 40 statuses the action inflicts that none of the unit's
 * inflicted statuses blocks, the status's cancel list becomes the action's
 * removal set, the pending changes are filtered, the status is recorded as
 * inflicted and given its CT, and its special flags are enabled (mode 2 when
 * re-inflicting anything but Crystal or Oil). Transparent clears the removal
 * flag; Invite and Charm copy the attacker's team, Invite permanently. Ends by
 * storing the unit's current statuses. */
void status_store_for_current_attack(s32 unit_id, s32 removal_only) {
    s32 i;
    s32 j;
    s32 byte;
    u8 mask;
    s32 blocked;
    u8 previous;

    g_action_current_ability.target_id = unit_id;
    g_action_target = &g_battle_unit_stats[unit_id];
    g_action_target_data = &g_battle_unit_stats[unit_id].action;
    for (i = 0; i < STATUS_COUNT; i++) {
        byte = i / 8;
        /* Signed shift (srav); STATUS_BYTE_MASK shifts unsigned. */
        mask = 0x80 >> (i & 7);
        if (!(g_action_target_data->status_changes.infliction[byte] & mask)) {
            continue;
        }
        blocked = 0;
        for (j = 0; j < STATUS_BYTE_COUNT; j++) {
            if (g_action_target->inflicted_status[j] & g_main_status_effect_data[i].cant_stack[j]) {
                blocked = 1;
                break;
            }
        }
        if (blocked != 0) {
            continue;
        }
        for (j = 0; j < STATUS_BYTE_COUNT; j++) {
            g_action_target_data->status_changes.removal[j] = g_main_status_effect_data[i].cancels[j];
        }
        status_modify_inflictions(removal_only);
        status_set_inflicted_ct_and_transfer_last_used_ct(unit_id);
        previous = g_action_target->inflicted_status[byte];
        g_action_target->inflicted_status[byte] = previous | mask;
        if (main_status_set_ct(g_action_target, i, 0) != 0) {
            continue;
        }
        if ((previous & mask) && i != STATUS_ID_CRYSTAL && i != STATUS_ID_OIL) {
            status_enable_special_flags(STATUS_HANDLER_INDEX(i), 2, unit_id);
        } else {
            status_enable_special_flags(STATUS_HANDLER_INDEX(i), 1, unit_id);
        }
        if (i == STATUS_ID_TRANSPARENT) {
            g_action_target->transparent_removal_flag = 0;
        }
        if (i == STATUS_ID_INVITE || i == STATUS_ID_CHARM) {
            previous = (g_action_target->team_flags & ~BATTLE_TEAM_MASK)
                | (g_action_attacker->team_flags & BATTLE_TEAM_MASK);
            g_action_target->team_flags = previous;
            if (i == STATUS_ID_INVITE) {
                previous = (g_action_target->initial_team_flags & ~BATTLE_TEAM_OR_PLAYER_CONTROL_MASK)
                    | (g_action_attacker->team_flags & BATTLE_TEAM_MASK);
                g_action_target_data->reaction_id = 0;
                g_action_target->initial_team_flags = previous;
                g_action_target->auto_battle_setting = AI_AUTOBATTLE_MANUAL;
                g_action_target->auto_battle_target = 0;
            }
        }
        if (!(g_action_target->initial_team_flags & BATTLE_TEAM_MASK) && (i == 1 || i == 2)) {
            g_battle_player_unit_fallen = 1;
        }
    }
    main_status_store_current(g_action_target);
}

/* Enter AI action simulation, save the current attacker and clear Hamedo state. */
void ai_begin_action_simulation(void) {
    g_action_state = ACTION_STATE_AI_SIMULATION;
    g_action_hamedo_flag = 0;
    mem_copy_byte_data_source_first(
        &g_action_current_attacker, &g_ai_current_attacker_backup, sizeof(g_action_current_attacker));
}

/* End an AI simulation: copy the saved current-attacker block back and return
 * the action state to execute. */
void ai_end_action_simulation(void) {
    g_action_state = ACTION_STATE_EXECUTE;
    mem_copy_byte_data_source_first(
        &g_ai_current_attacker_backup, &g_action_current_attacker, sizeof(g_action_current_attacker));
}

/* Current weather as seen by damage and movement rules: none when the map
 * ignores weather (indoors), otherwise the {3C} strength shifted into the
 * snow range when the map's snow flag is set. */
s32 map_get_effective_weather(void) {
    s32 weather;
    u32 flags;

    weather = script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
    flags = g_map_weather_flags;
    if (flags & MAP_WEATHER_FLAG_IGNORE_WEATHER) {
        return BATTLE_WEATHER_NONE;
    }
    if (weather < BATTLE_WEATHER_RAIN) {
        return weather;
    }
    if (flags & MAP_WEATHER_FLAG_SNOW) {
        weather += BATTLE_WEATHER_SNOW_OFFSET;
    }
    return weather;
}

enum {
    BATTLE_LEARN_ON_HIT_ROLL_MAXIMUM = 100,
    BATTLE_LEARN_ON_HIT_ROW_BYTES = 3,
    BATTLE_LEARN_ON_HIT_ABILITY_COUNT = 0x18,
    BATTLE_LEARNED_ABILITIES_OFFSET = 0x99
};

/* Find a unit that learns the ability it was just hit with.
 *
 * Every hit unit whose ability outcome is still pending is demoted to outcome 4
 * first.  Monsters, units whose status prevents a reaction, and units that
 * already know the ability are skipped; the remaining candidates roll the
 * ability's learn rate out of 100.  Returns the learning unit's slot and writes
 * the ability id through `ability_id`, or -1 when nobody learns it. */
s32 battle_ability_find_learn_on_hit_unit(s32 attacker_id, u16* ability_id) {
    u8 known[BATTLE_LEARN_ON_HIT_ROW_BYTES];
    ability_data_t* ability;
    unit_stats_t* unit;
    s32 i;
    s32 j;
    s32 row;
    s32 byte_index;
    s32 bit;
    u8* scratch;
    u8* src;
    s32 k;
    u8 skillset;

    if (g_action_hamedo_flag != 0) {
        return -1;
    }
    if (g_battle_acting_unit_used_ability_id == 0) {
        return -1;
    }

    ability = &g_main_ability_data[g_battle_acting_unit_used_ability_id];
    i = 0;
    if (!(ability->type_flags & ABILITY_TYPE_FLAG_LEARN_ON_HIT)) {
        return -1;
    }

    scratch = known;
    do {
        if (attacker_id != i) {
            unit = &g_battle_unit_stats[i];
            if (unit->ability_outcome & BATTLE_ABILITY_OUTCOME_AFFECTED) {
                unit->ability_outcome = BATTLE_ABILITY_OUTCOME_LEARN_CHECKED;
                if (!(unit->unit_flags & UNIT_FLAG_MONSTER)
                    && main_unit_has_status_in_set(unit, MAIN_STATUS_CHECK_SET_PREVENT_REACTION) == 0) {
                    skillset = unit->primary_skillset;
                    row = unit->job_id;
                    if (row < JOB_ID_SQUIRE) {
                        row = 0;
                    } else {
                        row = row - JOB_ID_SQUIRE;
                    }
                    /* Typed field access reassociates the 0x99 field offset
                     * with the row and demonstrably breaks the byte-exact
                     * address calculation. Preserve the target's row-offset,
                     * unit-base, then field-offset order. */
                    src = (u8*)((row * BATTLE_LEARN_ON_HIT_ROW_BYTES) + (u32)unit + BATTLE_LEARNED_ABILITIES_OFFSET);
                    for (k = 0; k < BATTLE_LEARN_ON_HIT_ROW_BYTES; k++) {
                        scratch[k] = src[k];
                    }

                    for (j = 0; j < BATTLE_LEARN_ON_HIT_ABILITY_COUNT; j++) {
                        byte_index = j / 8;
                        bit = 0x80 >> (j - byte_index * 8);
                        if (scratch[byte_index] & bit) {
                            continue;
                        }
                        if ((main_ability_get_id_from_skillset(skillset, j) & 0xffff)
                            != g_battle_acting_unit_used_ability_id) {
                            continue;
                        }
                        if (main_util_roll_pass_fail(BATTLE_LEARN_ON_HIT_ROLL_MAXIMUM, ability->learn_rate) == 0) {
                            *ability_id = g_battle_acting_unit_used_ability_id;
                            return i;
                        }
                    }
                }
            }
        }
        i++;
    } while (i < BATTLE_UNIT_SLOT_COUNT);
    return -1;
}

/* Integer address arithmetic preserves the retail instruction order here. */
#define LEARNED_ABILITIES_OFFSET ((u32) & ((unit_stats_t*)0)->career.learned_abilities)

/* Learn-on-hit: marks the acting unit's used ability as learned in the unit's
 * current job.
 *
 * Walks the 24 abilities of the unit's primary skillset; the one whose id equals
 * g_battle_acting_unit_used_ability_id and is not yet learned gets its bit set
 * (MSB first, three bytes per job). A job below Squire uses job slot 0. */
void battle_unit_learn_used_ability(unit_stats_t* unit) {
    u8 learned[3];
    u8 skillset;
    s32 job;
    s32 i;
    s32 byte_idx;
    s32 mask;
    u8* src;
    u8* dst;
    u8* base;
    u8* learned_byte;

    job = unit->job_id;
    skillset = unit->primary_skillset;
    if (job < JOB_ID_SQUIRE) {
        job = 0;
    } else {
        job -= JOB_ID_SQUIRE;
    }
    /* Three learned-ability bytes per job, from learned_abilities (0x99). */
    dst = learned;
    src = (u8*)(((job * 3) + (s32)unit) + LEARNED_ABILITIES_OFFSET);
    do {
        *dst = *src;
        dst += 1;
        src += 1;
    } while ((s32)dst < (s32)&learned[3]);
    i = 0;
    base = (u8*)(((job * 3) + (s32)unit) + LEARNED_ABILITIES_OFFSET);
    do {
        byte_idx = i / 8;
        mask = 0x80 >> (i - (byte_idx * 8));
        if (!(learned[byte_idx] & mask)) {
            if ((u16)main_ability_get_id_from_skillset(skillset, i) == g_battle_acting_unit_used_ability_id) {
                learned_byte = base + byte_idx;
                *learned_byte = mask | *learned_byte;
            }
        }
        i += 1;
    } while (i < SKILLSET_ABILITY_LIST_COUNT);
}

/* Passes a status change on to the unit's display flags
 * (status_queue_runtime_graphics_flag_change), but only while an action is
 * executing: previews and AI simulations change no display state.
 *
 * status_id is the status handler index (status id + 1). */
void status_enable_special_flags(s32 status_id, s32 enabled, s32 battle_id) {
    if (g_action_state == ACTION_STATE_EXECUTE) {
        status_queue_runtime_graphics_flag_change(status_id, enabled, battle_id);
    }
}

s32 action_increment_item_quantity_for_steal_break(unit_stats_t* unit, u8 item) {
    u8 item_id;

    if (g_action_state != ACTION_STATE_EXECUTE) {
        return 0;
    }
    if ((unit->initial_team_flags & BATTLE_TEAM_MASK) != BATTLE_TEAM_BLUE) {
        return 0;
    }
    item_id = item & 0xFF;
    if (item_id == ITEM_ID_NOTHING) {
        return 0;
    }
    if (item_id == ITEM_ID_NONE) {
        return 0;
    }
    if (main_item_get_total_equipment_quantity(item_id, 1) >= 0x63) {
        return action_add_war_funds(unit, 0, item_id);
    }
    g_main_item_quantities[item_id] += 1;
    return 0;
}

/* Add total to the war funds, plus a quarter of the item's price when an item
 * is given (action_increment_item_quantity_for_steal_break passes one
 * the stock is full of). Only for a unit on team 0 and only when executing;
 * the funds stay within 0..99,999,999. Returns the amount added. */
s32 action_add_war_funds(unit_stats_t* unit, s32 total, u8 item_id) {
    s32 value;

    if (g_action_state != ACTION_STATE_EXECUTE) {
        return 0;
    }
    if (unit->team_flags & BATTLE_TEAM_MASK) {
        return 0;
    }
    if (item_id != ITEM_ID_NOTHING) {
        total += g_main_item_primary_data[item_id].price >> 2;
    }
    value = script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) + total;
    if (value > 99999999) {
        value = 99999999;
    } else if (value < 0) {
        value = 0;
    }
    script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS, value);
    return total;
}

/* Apply an action result's EXP change when executing. Without bit 0x80 the
 * amount is gained: EXP saturates at 255 and the level-up check runs. With it
 * the low seven bits are lost, down to 0. */
void action_handle_steal_exp(unit_stats_t* unit, u8 amount) {
    s32 total;
    u8 capped;
    s16 reduced;

    if (g_action_state != ACTION_STATE_EXECUTE) {
        return;
    }
    if ((amount & 0x80) == 0) {
        total = unit->experience + amount;
        capped = total;
        if (total >= 0x100) {
            capped = 0xFF;
        }
        unit->experience = capped;
        if (main_unit_check_level_up(unit) != 0) {
            if (unit->battle_id == g_battle_acting_unit_id) {
                g_action_current_ability.level_gained_flag = 1;
            }
        }
    } else {
        reduced = unit->experience - (amount & 0x7F);
        if (reduced < 0) {
            reduced = 0;
        }
        unit->experience = reduced;
    }
}

/* Resolve a unit's status changes with the action state set to preview (2). */
void status_resolve_unit_changes_in_preview(s32 unit_id, s32 removal_only) {
    g_action_state = ACTION_STATE_PREVIEW;
    status_resolve_unit_changes(unit_id, removal_only);
    g_action_state = ACTION_STATE_EXECUTE;
}

/*
 * Remove the statuses the unit's action marks for removal.
 *
 * Only statuses the unit currently has are kept in the removal mask; each one
 * gets its CT cleared and its special flags dropped, then the bits are cleared
 * from both the current and innate status sets.
 */
void status_apply_unit_action_removal(unit_stats_t* unit) {
    s32 i;
    s32 battle_id;
    s32 idx;
    s32 mask;

    g_action_target_data = &unit->action;
    battle_id = unit->battle_id;
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        g_action_target_data->status_changes.removal[i] &= unit->status_sets.current[i];
    }
    for (i = 0; i < STATUS_COUNT; i++) {
        idx = i / 8;
        mask = 0x80 >> (i & 7);
        if (g_action_target_data->status_changes.removal[idx] & mask) {
            main_status_set_ct(unit, i, 0);
            status_enable_special_flags(STATUS_HANDLER_INDEX(i), 0, battle_id);
        }
    }
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        unit->status_sets.current[i] &= ~g_action_target_data->status_changes.removal[i];
        unit->status_sets.innate[i] &= ~g_action_target_data->status_changes.removal[i];
    }
}
