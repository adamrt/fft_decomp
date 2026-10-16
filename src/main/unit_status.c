/* A source-first byte copy (a second copy), equipment quantity totals, position and rewards init, the highest order
 * status effect, zodiac symbols, memory zeroing, casualty counters, status changes, the last ability CT, the action
 * state setter and the battle load exception handler. */

#include "fft/battle_status.h"
#include "fft/battle_unit.h"
#include "fft/data_job.h"
#include "fft/data_status.h"
#include "fft/main.h"
#include "fft/main_item.h"
#include "fft/main_unit.h"
#include "fft/script_variable.h"
#include "fft/unit.h"
#include "fft/unit_entd.h"
#include "fft/unit_party.h"
#include "psx/types.h"

/* The original wrote the body of main_mem_copy_bytes_source_first a second time as
 * main_mem_copy_byte_data_source_first. */
void main_mem_copy_byte_data_source_first(const void* source, void* destination, int count) {
    const u8* src = source;
    u8* dst = destination;
    int i;

    for (i = 0; i < count; i++) {
        *dst++ = *src++;
    }
}

/* Count the copies of an item the player owns: the inventory stock plus every roster human's
 * equipped copies. With include_battle set, roster humans deployed in the battle are counted from
 * their battle records instead, along with every other non-Mime human that started on the blue
 * team. */
s32 main_item_get_total_equipment_quantity(u8 item_id, s32 include_battle) {
    s32 total = g_main_item_quantities[item_id];
    s32 i;
    s32 j;
    party_unit_t* party;
    unit_stats_t* unit;
    s32 deployed;
    s32 party_id;

    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        party = main_party_get_unit(i);
        party_id = party->party_id;
        if (party_id == PARTY_ID_NONE) {
            continue;
        }
        if (party->gender_flags & UNIT_FLAG_MONSTER) {
            continue;
        }
        if (include_battle != 0) {
            deployed = 0;
            for (j = 0; j < BATTLE_UNIT_SLOT_COUNT; j++) {
                unit = &g_battle_unit_stats[j];
                if (unit->existence != BATTLE_UNIT_EXISTENCE_NONE && !(unit->unit_flags & UNIT_FLAG_MONSTER)
                    && unit->formation_index == party_id) {
                    deployed = 1;
                    break;
                }
            }
            if (deployed != 0) {
                continue;
            }
        }
        for (j = 0; j < UNIT_EQUIPMENT_SLOT_COUNT; j++) {
            if (party->equipment[j] == item_id) {
                total++;
            }
        }
    }

    if (include_battle == 0) {
        return total;
    }

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->existence != BATTLE_UNIT_EXISTENCE_NONE && !(unit->unit_flags & UNIT_FLAG_MONSTER)
            && !(unit->initial_team_flags & BATTLE_TEAM_MASK) && unit->job_id != JOB_ID_MIME) {
            for (j = 0; j < UNIT_EQUIPMENT_SLOT_COUNT; j++) {
                if (unit->equipment[j] == item_id) {
                    total++;
                }
            }
        }
    }
    return total;
}

/* Copy the ENTD unit's map X and Y, elevation, facing and spell-quote bits, war trophy and bonus
 * money into the battle unit. */
void main_unit_init_position_and_rewards(unit_stats_t* unit, entd_unit_t* entd) {
    unit->x = entd->x;
    unit->position.y = entd->position.y;
    unit->position.higher_elevation = entd->position.higher_elevation;
    unit->position.facing = entd->position.facing;
    unit->position.spell_quote_skillsets = entd->position.spell_quote_skillsets;

    unit->war_trophy = entd->war_trophy;
    unit->bonus_money_modifier = entd->bonus_money_modifier;
}

/*
 * Return the highest-order status changed by an action. Bits 0-6 encode the
 * one-based status index, 0x80 marks removal, and 0x100 reflects flags_2 bit 3.
 */
s32 main_status_find_action_highest_order_effect(const action_data_t* action) {
    s32 highest_order = -1;
    s32 result = 0;
    s32 status_index;
    u8 mask;
    s32 byte_index;
    u8 current_order;

    for (status_index = 0; status_index < STATUS_COUNT; status_index++) {
        byte_index = status_index / 8;
        mask = 0x80 >> (status_index & 7);
        if (action->status_changes.removal[byte_index] & mask) {
            current_order = g_main_status_effect_data[status_index].order;
            if (current_order > highest_order) {
                highest_order = current_order;
                result = status_index + 1 + MAIN_ACTION_STATUS_RESULT_REMOVAL;
                if (g_main_status_effect_data[status_index].flags_2 & STATUS_EFFECT_FLAG_2_SCRIPT_FILTER_EXCLUDED) {
                    result = status_index + 1 + MAIN_ACTION_STATUS_RESULT_REMOVAL
                        + MAIN_ACTION_STATUS_RESULT_SCRIPT_FILTER_EXCLUDED;
                }
            }
        }
        if (action->status_changes.infliction[byte_index] & mask) {
            current_order = g_main_status_effect_data[status_index].order;
            if (current_order > highest_order) {
                highest_order = current_order;
                result = status_index + 1;
                if (g_main_status_effect_data[status_index].flags_2 & STATUS_EFFECT_FLAG_2_SCRIPT_FILTER_EXCLUDED) {
                    result = status_index + 1 + MAIN_ACTION_STATUS_RESULT_SCRIPT_FILTER_EXCLUDED;
                }
            }
        }
    }
    return result;
}

/* Return the zodiac sign for a day-of-year birthday.
 *
 * Counts the sign boundaries at or before the day, then rotates the count
 * into zodiac order with (count + 9) % 12. */
s32 main_unit_calculate_zodiac_symbol(u16 birthday) {
    s32 zodiac = 0;
    s32 i;

    for (i = 0; i < ZODIAC_SIGN_ORDINARY_COUNT; i++) {
        zodiac += birthday >= g_main_zodiac_day_limits[i];
    }
    zodiac = (zodiac + 9) % ZODIAC_SIGN_ORDINARY_COUNT;
    return (u16)zodiac;
}

/* Clear count bytes one at a time; a count of 0 or less clears nothing. */
void main_mem_zero(void* destination, s32 count) {
    u8* dst = destination;
    s32 index = 0;

    while (index < count) {
        *dst++ = 0;
        index++;
    }
}

/* Count a KO in the script variables: a unit that started on the blue team adds to
 * EVENT_SCRIPT_VAR_CASUALTIES, any other unit to EVENT_SCRIPT_VAR_INJURED; both stop at 9999. */
void main_unit_increase_casualty_counters(unit_stats_t* unit) {
    s32 id;
    s32 count;

    id = (unit->initial_team_flags & BATTLE_TEAM_MASK) != BATTLE_TEAM_BLUE ? EVENT_SCRIPT_VAR_INJURED
                                                                           : EVENT_SCRIPT_VAR_CASUALTIES;
    count = script_get_variable(id);
    if (count < EVENT_CASUALTY_COUNT_MAX) {
        count++;
    }
    script_set_variable(id, count);
}

/* Add, remove, or replace a status-set flag and refresh the unit's current status. */
void main_status_change_unit(unit_stats_t* unit, s32 status_byte_index, u8 status_flag, s32 change_type) {
    u8 updated;

    /* An enum selector removes the native signed range test. */
    switch (change_type) {
    case MAIN_STATUS_ADD:
        updated = unit->inflicted_status[status_byte_index] | status_flag;
        break;
    case MAIN_STATUS_REMOVE:
        updated = unit->inflicted_status[status_byte_index] & ~status_flag;
        break;
    case MAIN_STATUS_REPLACE:
        updated = status_flag;
        break;
    }

    unit->inflicted_status[status_byte_index] = updated;
    unit->status_sets.current[status_byte_index] = updated | unit->status_sets.innate[status_byte_index];
}

/* Recompute the current status set as innate | inflicted. */
void main_status_store_current(unit_stats_t* unit) {
    s32 status_set;

    for (status_set = 0; status_set < STATUS_BYTE_COUNT; status_set++) {
        unit->status_sets.current[status_set]
            = unit->status_sets.innate[status_set] | unit->inflicted_status[status_set];
    }
}

/* Charge time of the unit's last ability: the low seven bits of the ability
 * record's CT byte. */
void main_unit_copy_last_ability_ct(unit_stats_t* unit) {
    unit->ability_ct = g_main_ability_range_data[unit->action_command.ability_id].ct & 0x7f;
}

/* Make Charging, Jump, Defending or Performing the unit's only action status, or clear all four
 * with MAIN_UNIT_ACTION_STATE_NONE, and enable or disable the special flags of each status that
 * changed. NONE and DEFENDING also clear the charging ability CT; PERFORMING sets it from the last
 * ability's CT. KEEP_PERFORMING keeps Performing if the unit has it and otherwise acts as NONE. */
void main_status_set_action_state(unit_stats_t* unit, u8 action_state) {
    u8 old_status = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)];
    s32 battle_id = unit->battle_id;
    u8 status;
    u8 cleared;
    s32 status_id;
    u8 mask;
    u8 changed;

    if (action_state == MAIN_UNIT_ACTION_STATE_KEEP_PERFORMING) {
        /* The Performing bit (0x01) shifted left 3 is PERFORMING (8); without it, NONE (0). */
        action_state = (old_status & STATUS_BYTE_MASK(STATUS_ID_PERFORMING)) << 3;
    }
    switch (action_state) {
    case MAIN_UNIT_ACTION_STATE_NONE:
        status = 0;
        unit->charged_ability_ct = 0xff;
        break;
    case MAIN_UNIT_ACTION_STATE_CHARGING:
        status = STATUS_BYTE_MASK(STATUS_ID_CHARGING);
        break;
    case MAIN_UNIT_ACTION_STATE_JUMPING:
        status = STATUS_BYTE_MASK(STATUS_ID_JUMP);
        break;
    case MAIN_UNIT_ACTION_STATE_DEFENDING:
        status = STATUS_BYTE_MASK(STATUS_ID_DEFENDING);
        unit->charged_ability_ct = 0xff;
        break;
    case MAIN_UNIT_ACTION_STATE_PERFORMING:
        status = STATUS_BYTE_MASK(STATUS_ID_PERFORMING);
        main_unit_copy_last_ability_ct(unit);
        unit->charged_ability_ct = unit->ability_ct;
        break;
    }
    cleared = status
        ^ (STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_JUMP)
            | STATUS_BYTE_MASK(STATUS_ID_DEFENDING) | STATUS_BYTE_MASK(STATUS_ID_PERFORMING));
    main_status_change_unit(unit, STATUS_BYTE_INDEX(STATUS_ID_CHARGING), status, MAIN_STATUS_ADD);
    main_status_change_unit(unit, STATUS_BYTE_INDEX(STATUS_ID_CHARGING), cleared, MAIN_STATUS_REMOVE);

    changed = old_status ^ unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)];
    mask = STATUS_BYTE_MASK(STATUS_ID_CHARGING);
    for (status_id = STATUS_HANDLER_INDEX(STATUS_ID_CHARGING); status_id <= STATUS_HANDLER_INDEX(STATUS_ID_PERFORMING);
        status_id++) {
        if (changed & mask) {
            if ((old_status & mask) != 0) {
                battle_status_queue_graphics_change_if_executing(status_id, 0, battle_id);
            } else {
                battle_status_queue_graphics_change_if_executing(status_id, 1, battle_id);
            }
        }
        mask >>= 1;
    }
}

/* Pass a BATTLE.BIN unit-setup error to the empty main_system_report_error hook as
 * source 3. Callers add a failure base to the battle id: 100 no spritesheet slot left, 200 enemy
 * limit reached, 300 or 400 placement rejected, 500 roster unit load failed, 600 ENTD unit init
 * failed; 0x80 alone marks a map tile-data retry that gave up after 1000 tries. */
void main_system_handle_battle_load_exception(s32 value) {
    main_system_report_error(3, value);
}
