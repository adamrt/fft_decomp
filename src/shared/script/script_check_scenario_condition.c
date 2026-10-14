#include "fft/battle_action.h"
#include "fft/battle_unit.h"
#include "fft/script.h"

/* Evaluate one scenario condition record for
 * script_run_scenario_conditions.
 *
 * The record's 16-bit values arrive as type (event_scenario_condition_e), id and
 * value; the two tile conditions also read a y and an elevation, with value as
 * the x. Returns EVENT_SCENARIO_CONDITION_RESULT_PASS when the condition holds,
 * RESULT_RUN_EVENT for type RUN_EVENT after storing id as the current event, and
 * RESULT_FAIL otherwise, including for every type that has no test here.
 *
 * ACTING_UNIT looks at the unit the check runs for
 * (g_script_condition_unit_battle_id): it passes when that unit's unit id
 * is id, or when it is a deployed unit whose character identity is id and id is a
 * named (non-generic) identity. While the battle id is 0xff the condition fails.
 * ITEM_EQUIPPED searches the seven equipment slots of the units with unit ids 1
 * to 3. The HP percentage is hp * 100 / (max_hp + 1). The casualty conditions
 * compare with id, not value.
 *
 * Keep the two unit-id range comparisons: retail reloads the byte. */
event_scenario_condition_result_e script_check_scenario_condition(
    event_scenario_condition_e type, s32 id, s32 value, s32 tile_y, s32 elevation) {
    unit_stats_t* unit;
    s32 i;
    s32 j; /* equipment slot, then battle index: one variable (a0 in both loops) */

    if (type == EVENT_SCENARIO_CONDITION_VARIABLE_EQUAL && script_get_variable(id) == value)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_VARIABLE_AT_LEAST && script_get_variable(id) >= value)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_VARIABLE_AT_MOST && value >= script_get_variable(id))
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type >= 4 && type <= 10) {
        unit = unit_get_stats_by_unit_id(id);
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
    if (type == EVENT_SCENARIO_CONDITION_ACTING_UNIT && g_script_condition_unit_battle_id != 0xff) {
        unit = battle_unit_get_stats_from_battle_id(g_script_condition_unit_battle_id);
        if (unit->unit_id == id
            || (unit->unit_id >= EVENT_UNIT_ID_DEPLOYED_FIRST && unit->unit_id < EVENT_UNIT_ID_DEPLOYED_END
                && unit->character_identity == id && id >= 0 && id < CHARACTER_IDENTITY_GENERIC_FIRST
                && id != CHARACTER_IDENTITY_ENTD_NONE))
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
        return EVENT_SCENARIO_CONDITION_RESULT_FAIL;
    }
    if (type == EVENT_SCENARIO_CONDITION_ITEM_EQUIPPED) {
        for (i = 1; i < 4; i++) {
            unit = unit_get_stats_by_unit_id(i);
            if (unit != (unit_stats_t*)-1) {
                for (j = 0; j < 7; j++) {
                    if (unit->equipment[j] == id)
                        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
                }
            }
        }
    }
    if (type == EVENT_SCENARIO_CONDITION_WAR_FUNDS_AT_LEAST && script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) >= value)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_WAR_FUNDS_AT_MOST && value >= script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS))
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_DATE_ON_OR_BEFORE) {
        if (script_get_variable(EVENT_SCRIPT_VAR_MONTH) < id)
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
        if (script_get_variable(EVENT_SCRIPT_VAR_MONTH) == id && value >= script_get_variable(EVENT_SCRIPT_VAR_DAY))
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    }
    if (type == EVENT_SCENARIO_CONDITION_DATE_ON_OR_AFTER) {
        if (id < script_get_variable(EVENT_SCRIPT_VAR_MONTH))
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
        if (script_get_variable(EVENT_SCRIPT_VAR_MONTH) == id && script_get_variable(EVENT_SCRIPT_VAR_DAY) >= value)
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    }
    if (type == EVENT_SCENARIO_CONDITION_CASUALTIES_AT_LEAST && script_get_variable(EVENT_SCRIPT_VAR_CASUALTIES) >= id)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_CASUALTIES_AT_MOST && id >= script_get_variable(EVENT_SCRIPT_VAR_CASUALTIES))
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_ENEMIES_DEFEATED && action_check_battle_outcome() == 0)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_UNIT_AT_TILE) {
        unit = unit_get_stats_by_unit_id(id);
        if (unit != (unit_stats_t*)-1 && unit->x == value && unit->position.y == tile_y
            && unit->position.higher_elevation == elevation)
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    }
    if (type == EVENT_SCENARIO_CONDITION_PALETTE_UNIT_AT_TILE) {
        for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
            if (battle_unit_has_runtime_id(i)) {
                j = battle_unit_get_battle_id_by_runtime_id(i);
                if (j != -1) {
                    unit = battle_unit_get_stats_from_battle_id(j);
                    if (unit->x == value && unit->position.y == tile_y && unit->position.higher_elevation == elevation
                        && unit->sprite_palette == id)
                        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
                }
            }
        }
    }
    if (type == EVENT_SCENARIO_CONDITION_RUN_EVENT) {
        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, id);
        return EVENT_SCENARIO_CONDITION_RESULT_RUN_EVENT;
    }
    return EVENT_SCENARIO_CONDITION_RESULT_FAIL;
}
