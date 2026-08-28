/* Evaluates one scenario condition record for process_scenario_conditionals
 * (0x801425b0). Twin of world_script_check_scenario_condition (WORLD 0x800f5f0c),
 * which documents the condition types; BATTLE is built without the divide
 * checks. The unit_id range test is spelled as two comparisons so GCC reloads
 * the byte as the target does. */
#include "fft/battle.h"
#include "fft/event.h"
#include "fft/script_variables.h"
#include "fft/unit_slots.h"

s32 battle_script_check_scenario_condition(s32 type, s32 id, s32 value, s32 tile_y, s32 elevation) {
    battle_stats_t* unit;
    s32 i;
    s32 j; /* equipment slot, then battle index: one variable (a0 in both loops) */

    if (type == 1 && battle_script_get_variable(id) == value)
        return 1;
    if (type == 2 && battle_script_get_variable(id) >= value)
        return 1;
    if (type == 3 && value >= battle_script_get_variable(id))
        return 1;
    if ((u32)(type - 4) < 7) {
        unit = battle_unit_get_by_unit_id(id);
        if (unit != (battle_stats_t*)-1) {
            if (type == 4)
                return 1;
            if (type == 5 && unit->hp >= value)
                return 1;
            if (type == 6 && value >= unit->hp)
                return 1;
            if (type == 7 && unit->hp * 100 / (unit->max_hp + 1) >= value)
                return 1;
            if (type == 8 && value >= unit->hp * 100 / (unit->max_hp + 1))
                return 1;
            if (type == 9 && unit->mp >= value)
                return 1;
            if (type == 10 && value >= unit->mp)
                return 1;
        }
    }
    if (type == 11 && g_battle_script_condition_unit_battle_id != 0xff) {
        unit = battle_unit_get_stats_from_battle_id(g_battle_script_condition_unit_battle_id);
        if (unit->unit_id == id
            || (unit->unit_id >= EVENT_UNIT_ID_DEPLOYED_FIRST && unit->unit_id < EVENT_UNIT_ID_DEPLOYED_END
                && unit->character_identity == id && (u32)id < CHARACTER_IDENTITY_GENERIC_FIRST
                && id != CHARACTER_IDENTITY_ENTD_NONE))
            return 1;
        return 0;
    }
    if (type == 13) {
        for (i = 1; i < 4; i++) {
            unit = battle_unit_get_by_unit_id(i);
            if (unit != (battle_stats_t*)-1) {
                for (j = 0; j < 7; j++) {
                    if (unit->equipment[j] == id)
                        return 1;
                }
            }
        }
    }
    if (type == 14 && battle_script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) >= value)
        return 1;
    if (type == 15 && value >= battle_script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS))
        return 1;
    if (type == 16) {
        if (battle_script_get_variable(EVENT_SCRIPT_VAR_MONTH) < id)
            return 1;
        if (battle_script_get_variable(EVENT_SCRIPT_VAR_MONTH) == id
            && value >= battle_script_get_variable(EVENT_SCRIPT_VAR_DAY))
            return 1;
    }
    if (type == 17) {
        if (id < battle_script_get_variable(EVENT_SCRIPT_VAR_MONTH))
            return 1;
        if (battle_script_get_variable(EVENT_SCRIPT_VAR_MONTH) == id
            && battle_script_get_variable(EVENT_SCRIPT_VAR_DAY) >= value)
            return 1;
    }
    if (type == 18 && battle_script_get_variable(EVENT_SCRIPT_VAR_CASUALTIES) >= id)
        return 1;
    if (type == 19 && id >= battle_script_get_variable(EVENT_SCRIPT_VAR_CASUALTIES))
        return 1;
    if (type == 22 && battle_action_check_battle_outcome() == 0)
        return 1;
    if (type == 24) {
        unit = battle_unit_get_by_unit_id(id);
        if (unit != (battle_stats_t*)-1 && unit->x == value && unit->position.bits.y == tile_y
            && unit->position.bits.higher_elevation == elevation)
            return 1;
    }
    if (type == 37) {
        for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
            if (battle_unit_has_misc_id(i)) {
                j = battle_unit_get_battle_index_by_misc_id(i);
                if (j != -1) {
                    unit = battle_unit_get_stats_from_battle_id(j);
                    if (unit->x == value && unit->position.bits.y == tile_y
                        && unit->position.bits.higher_elevation == elevation && unit->sprite_palette == id)
                        return 1;
                }
            }
        }
    }
    if (type == 25) {
        battle_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, id);
        return 2;
    }
    return 0;
}
