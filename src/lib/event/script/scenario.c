/* Evaluate scenario victory and defeat conditions against units, teams and the party.
 * Matching blocks choose the operation that finishes the current event. */

#include "fft/battle_state.h"
#include "fft/script.h"
#include "fft/script_instruction.h"
#include "fft/script_variable.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "fft/unit_runtime.h"
#include "psx/types.h"

/* Spell announcements and scenario handoff. */
extern s32 g_script_next_event_id;

/* Operand-halfword counts through condition 37. The compiler aligns the
 * following weather table; the original declared extent is unknown. */
static u8 g_scenario_condition_param_lengths[38] = { 0, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1, 1, 1, 2, 2, 3, 3, 2, 2, 2, 2,
    0, 3, 4, 2, 8, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4 };
static event_scenario_condition_result_e condition_check(
    event_scenario_condition_e type, s32 id, s32 value, s32 tile_y, s32 elevation);

/* Resolves an event unit id to battle stats for the scenario conditions; returns
 * -1 for none.
 *
 * An id found among the ENTD unit ids gives that unit when it has a unit record.
 * Otherwise an id in the special-character identity range is looked up by
 * character identity and accepted only for a deployed party member (unit id
 * 0x78..0x7c). */
static unit_stats_t* scenario_unit_stats_get(s32 unit_id) {
    s32 index;
    unit_stats_t* stats;

    unit_entd_find_stats(unit_id, &index);
    if (index < 0) {
        if (unit_id < CHARACTER_IDENTITY_RAMZA_CHAPTER_1 || unit_id >= CHARACTER_IDENTITY_GENERIC_FIRST) {
            return (unit_stats_t*)-1;
        }
        index = unit_find_slot_by_identity(unit_id);
        if (index >= 0) {
            stats = unit_stats_get(index);
            if (stats->unit_id >= EVENT_UNIT_ID_DEPLOYED_FIRST && stats->unit_id < EVENT_UNIT_ID_DEPLOYED_END) {
                return stats;
            }
        }
        return (unit_stats_t*)-1;
    }
    if (unit_battle_runtime_id_get(index) >= 0) {
        return unit_stats_get(index);
    }
    return (unit_stats_t*)-1;
}

/* Walk the scenario's condition chains and report whether one selects an event.
 *
 * The runner inspects the first ten of sixteen byte offsets into
 * g_scenario_condition_data, one per chain; a zero offset after the first
 * ends the list. A chain is a run of records, each a 16-bit type followed by
 * g_scenario_condition_param_lengths[type] 16-bit operands. Records are
 * evaluated while they pass and the first failure drops the chain; the RUN_EVENT
 * record that closes a chain stores its event id in the current-event variable,
 * and the function returns 1. It returns 0 when no chain gets that far.
 *
 * The checker always receives the five 16-bit values at the record, whatever the
 * type's length. Called at EventEnd and by the scenario-check command of
 * the scene's menu system-function dispatcher. */
s32 scenario_conditions_run(void) {
    s16* condition;
    s32 i;
    s32 shifted_offset;
    s32 offset;
    s32 result;

    for (i = 0; i < 10; i++) {
        /* The entry is a 16-bit byte offset into the script area; the
         * target sign-extends it with a live `<< 16` value, taking the
         * division's sign bit from that value rather than from the
         * sign-extended one. Spelled as `offset = (s16)g_scenario_condition_offsets[i]` with
         * `offset / 2`, combine folds the shift pair into a single `lh` and the
         * function comes out three instructions short, so the shift pair
         * and the by-two division are spelled out here. */
        shifted_offset = g_scenario_condition_offsets[i] << 16;
        offset = shifted_offset >> 16;
        if (i != 0 && offset == 0) {
            return 0;
        }
        condition = &g_scenario_condition_data[(offset + (s32)((u32)shifted_offset >> 31)) >> 1];
        do {
            result = condition_check(condition[0], condition[1], condition[2], condition[3], condition[4]);
            condition += g_scenario_condition_param_lengths[condition[0]] + 1;
        } while (result == EVENT_SCENARIO_CONDITION_RESULT_PASS);
        if (result == EVENT_SCENARIO_CONDITION_RESULT_RUN_EVENT) {
            return 1;
        }
    }
    return 0;
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * scenario_conditions_run and condition_check. Nothing
 * on the disc calls it or points at it; the name is a neutral placeholder
 * until a reference turns up. */
void script_noop_9(void) {
}

/* Evaluate one scenario condition record for
 * scenario_conditions_run.
 *
 * The record's 16-bit values arrive as type (event_scenario_condition_e), id and
 * value; the two tile conditions also read a y and an elevation, with value as
 * the x. Returns EVENT_SCENARIO_CONDITION_RESULT_PASS when the condition holds,
 * RESULT_RUN_EVENT for type RUN_EVENT after storing id as the current event, and
 * RESULT_FAIL otherwise, including for every type that has no test here.
 *
 * ACTING_UNIT looks at the unit the check runs for
 * (g_scenario_condition_unit_battle_id): it passes when that unit's unit id
 * is id, or when it is a deployed unit whose character identity is id and id is a
 * named (non-generic) identity. While the battle id is 0xff the condition fails.
 * ITEM_EQUIPPED searches the seven equipment slots of the units with unit ids 1
 * to 3. The HP percentage is hp * 100 / (max_hp + 1). The casualty conditions
 * compare with id, not value.
 *
 * Keep the two unit-id range comparisons: retail reloads the byte. */
static event_scenario_condition_result_e condition_check(
    event_scenario_condition_e type, s32 id, s32 value, s32 tile_y, s32 elevation) {
    unit_stats_t* unit;
    s32 i;
    s32 j; /* equipment slot, then battle index: one variable (a0 in both loops) */

    if (type == EVENT_SCENARIO_CONDITION_VARIABLE_EQUAL && script_variable_get(id) == value)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_VARIABLE_AT_LEAST && script_variable_get(id) >= value)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_VARIABLE_AT_MOST && value >= script_variable_get(id))
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type >= EVENT_SCENARIO_CONDITION_UNIT_PRESENT && type <= EVENT_SCENARIO_CONDITION_UNIT_MP_AT_MOST) {
        unit = scenario_unit_stats_get(id);
        if (unit != (unit_stats_t*)-1) {
            if (type == EVENT_SCENARIO_CONDITION_UNIT_PRESENT)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_HP_AT_LEAST && unit->hp >= value)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_HP_AT_MOST && value >= unit->hp)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_HP_PERCENT_AT_LEAST
                && unit->hp * 100 / (unit->max_hp + 1) >= value)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_HP_PERCENT_AT_MOST
                && value >= unit->hp * 100 / (unit->max_hp + 1))
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_MP_AT_LEAST && unit->mp >= value)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_MP_AT_MOST && value >= unit->mp)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
        }
    }
    if (type == EVENT_SCENARIO_CONDITION_ACTING_UNIT && g_scenario_condition_unit_battle_id != 0xff) {
        unit = unit_stats_get(g_scenario_condition_unit_battle_id);
        if (unit->unit_id == id
            || (unit->unit_id >= EVENT_UNIT_ID_DEPLOYED_FIRST && unit->unit_id < EVENT_UNIT_ID_DEPLOYED_END
                && unit->character_identity == id && id >= 0 && id < CHARACTER_IDENTITY_GENERIC_FIRST
                && id != CHARACTER_IDENTITY_ENTD_NONE))
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
        return EVENT_SCENARIO_CONDITION_RESULT_FAIL;
    }
    if (type == EVENT_SCENARIO_CONDITION_ITEM_EQUIPPED) {
        for (i = 1; i < 4; i++) {
            unit = scenario_unit_stats_get(i);
            if (unit != (unit_stats_t*)-1) {
                for (j = 0; j < 7; j++) {
                    if (unit->equipment[j] == id)
                        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
                }
            }
        }
    }
    if (type == EVENT_SCENARIO_CONDITION_WAR_FUNDS_AT_LEAST && script_variable_get(EVENT_SCRIPT_VAR_WAR_FUNDS) >= value)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_WAR_FUNDS_AT_MOST && value >= script_variable_get(EVENT_SCRIPT_VAR_WAR_FUNDS))
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_DATE_ON_OR_BEFORE) {
        if (script_variable_get(EVENT_SCRIPT_VAR_MONTH) < id)
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
        if (script_variable_get(EVENT_SCRIPT_VAR_MONTH) == id && value >= script_variable_get(EVENT_SCRIPT_VAR_DAY))
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    }
    if (type == EVENT_SCENARIO_CONDITION_DATE_ON_OR_AFTER) {
        if (id < script_variable_get(EVENT_SCRIPT_VAR_MONTH))
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
        if (script_variable_get(EVENT_SCRIPT_VAR_MONTH) == id && script_variable_get(EVENT_SCRIPT_VAR_DAY) >= value)
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    }
    if (type == EVENT_SCENARIO_CONDITION_CASUALTIES_AT_LEAST && script_variable_get(EVENT_SCRIPT_VAR_CASUALTIES) >= id)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_CASUALTIES_AT_MOST && id >= script_variable_get(EVENT_SCRIPT_VAR_CASUALTIES))
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_ENEMIES_DEFEATED && event_check_battle_outcome() == 0)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_UNIT_AT_TILE) {
        unit = scenario_unit_stats_get(id);
        if (unit != (unit_stats_t*)-1 && unit->x == value && unit->position.y == tile_y
            && unit->position.higher_elevation == elevation)
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    }
    if (type == EVENT_SCENARIO_CONDITION_PALETTE_UNIT_AT_TILE) {
        for (i = 0; i < UNIT_SLOT_COUNT; i++) {
            if (unit_has_runtime_id(i)) {
                j = unit_runtime_battle_id_get(i);
                if (j != -1) {
                    unit = unit_stats_get(j);
                    if (unit->x == value && unit->position.y == tile_y && unit->position.higher_elevation == elevation
                        && unit->sprite_palette == id)
                        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
                }
            }
        }
    }
    if (type == EVENT_SCENARIO_CONDITION_RUN_EVENT) {
        script_variable_set(EVENT_SCRIPT_VAR_CURRENT_EVENT, id);
        return EVENT_SCENARIO_CONDITION_RESULT_RUN_EVENT;
    }
    return EVENT_SCENARIO_CONDITION_RESULT_FAIL;
}

/* Return the current event's 16-bit finish-operation value, or 0 while no scenario event
 * is active. Bits 0-7 and 10-11 hold the next scenario id and the remaining bits
 * the after-event code. */
s32 scenario_finish_operation_get(void) {
    if (g_scenario_event_active == 0) {
        return 0;
    }
    return g_scenario_event_finish_operations[script_variable_get(EVENT_SCRIPT_VAR_CURRENT_EVENT)];
}

/* Return the after-event code of the pending event (g_script_next_event_id): bits
 * 8-9 and 12-15 of its 16-bit finish-operation value, shifted down by 8. Zero means the
 * event has none. */
u32 script_event_finish_operation_get(void) {
    return (g_scenario_event_finish_operations[g_script_next_event_id] & 0xF300) >> 8;
}
