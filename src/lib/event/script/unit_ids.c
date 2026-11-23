/* Resolve event-script unit ids to map unit ids, and copy a map unit's tile coordinates. */

#include "fft/character_identity.h"
#include "fft/entd.h"
#include "fft/map_unit.h"
#include "fft/script.h"
#include "fft/script_instruction.h"
#include "fft/script_variable.h"
#include "fft/status.h"
#include "fft/unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* map_unit_get_map_coords without its result: the unit's tile as x, z, y. */
void map_unit_copy_map_coords(u32 map_unit_id, SVECTOR* coords) {
    map_unit_get_map_coords(map_unit_id, coords);
}

/* Empty. script_resolve_map_unit_id passes it each unit id it cannot resolve before returning
 * EVENT_MAP_UNIT_ID_NONE. */
static void report_missing_unit_stub(s32 unused_unit_id) {
}

/* Resolve an event-script unit identifier to its active map unit ID.
 *
 * - A character identity becomes that unit's ENTD unit id when it is one of the
 *   deployed ids (EVENT_UNIT_ID_DEPLOYED_FIRST up).
 * - EVENT_UNIT_SELECTOR_IMPORTANT_UNIT names the first ENTD unit with an importance flag.
 * - EVENT_UNIT_SELECTOR_ACTIVE_TURN names the unit whose turn it is, or failing that
 *   ENTD unit 1 or 2.
 * - In the Ramza game-over event every id names the first unit with a Ramza identity.
 * - The four event unit slots return the map unit id stored in them.
 * - Any other id is looked up as an ENTD unit id.
 *
 * Missing units and units with Treasure status return EVENT_MAP_UNIT_ID_NONE, except
 * that the game-over path returns -1 when no Ramza is on the field (retail).
 */
s32 script_resolve_map_unit_id(s32 unit_id) {
    s32 battle_unit_id;
    s32 unit_state;
    s32 original_unit_id;
    s32 map_unit_id;
    battle_unit_t* unit;

    if (unit_id >= CHARACTER_IDENTITY_RAMZA_CHAPTER_1 && unit_id < CHARACTER_IDENTITY_GENERIC_FIRST) {
        battle_unit_id = battle_unit_find_id_by_character_identity(unit_id);
        if (battle_unit_id >= 0) {
            unit = battle_unit_get(battle_unit_id);
            if (unit->unit_id >= EVENT_UNIT_ID_DEPLOYED_FIRST && unit->unit_id < EVENT_UNIT_ID_DEPLOYED_END)
                unit_id = unit->unit_id;
        }
    }
    original_unit_id = unit_id;

    if (unit_id == EVENT_UNIT_SELECTOR_IMPORTANT_UNIT) {
        for (battle_unit_id = 0; battle_unit_id < ENTD_UNITS_PER_ENCOUNTER; battle_unit_id++) {
            unit = battle_unit_get(battle_unit_id);
            if (unit->team_flags & UNIT_TEAM_IMPORTANCE_MASK)
                break;
        }
        if (battle_unit_id == ENTD_UNITS_PER_ENCOUNTER) {
            report_missing_unit_stub(original_unit_id);
            return EVENT_MAP_UNIT_ID_NONE;
        }
        unit_id = unit->unit_id;
    } else if (unit_id == EVENT_UNIT_SELECTOR_ACTIVE_TURN) {
        for (battle_unit_id = 0; battle_unit_id < BATTLE_UNIT_SLOT_COUNT; battle_unit_id++) {
            if (map_unit_exists(battle_unit_id)) {
                s32 active_battle_unit_id = map_unit_get_battle_unit_id(battle_unit_id);

                unit_id = active_battle_unit_id;
                if (active_battle_unit_id != -1) {
                    unit = battle_unit_get(active_battle_unit_id);
                    if (unit->has_turn)
                        break;
                }
            }
        }
        if (battle_unit_id != UNIT_ID_NONE) {
            unit_id = unit->unit_id;
            battle_unit_find_by_unit_id(unit_id, &battle_unit_id);
        } else {
            battle_unit_id = -1;
        }

        if (battle_unit_id < 0) {
            for (unit_id = 1; unit_id < 3; unit_id++) {
                unit = battle_unit_find_by_unit_id(unit_id, &battle_unit_id);
                if (battle_unit_id >= 0)
                    break;
            }
            /* Retail compares the lookup result rather than the loop counter with 3, so
             * finding neither unit is not caught here. */
            if (battle_unit_id == 3
                || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_JUMP)] & STATUS_BYTE_MASK(STATUS_ID_JUMP))) {
                report_missing_unit_stub(original_unit_id);
                return EVENT_MAP_UNIT_ID_NONE;
            }
        }
    }

    if (script_variable_get(EVENT_SCRIPT_VAR_CURRENT_EVENT) == EVENT_ID_GAME_OVER_RAMZA) {
        for (battle_unit_id = 0; battle_unit_id < BATTLE_UNIT_SLOT_COUNT; battle_unit_id++) {
            unit = battle_unit_get(battle_unit_id);
            if (unit->character_identity != CHARACTER_IDENTITY_ENTD_NONE
                && unit->character_identity < CHARACTER_IDENTITY_RAMZA_END)
                break;
        }
        if (battle_unit_id == UNIT_ID_NONE)
            return -1;
        unit_id = unit->unit_id;
    }

    if (unit_id >= EVENT_UNIT_SELECTOR_SLOT_FIRST
        && unit_id < EVENT_UNIT_SELECTOR_SLOT_FIRST + EVENT_UNIT_SELECTOR_SLOT_COUNT) {
        map_unit_id = (u16)g_script_unit_slots[unit_id - EVENT_UNIT_SELECTOR_SLOT_FIRST];
        if (map_unit_id == EVENT_UNIT_SLOT_EMPTY) {
            report_missing_unit_stub(original_unit_id);
            return EVENT_MAP_UNIT_ID_NONE;
        }
        return map_unit_id;
    }

    unit = battle_unit_find_by_unit_id(unit_id, &unit_state);
    if (unit_state < 0 && unit_state != -2
        && script_variable_get(EVENT_SCRIPT_VAR_CURRENT_EVENT) != EVENT_ID_GAME_OVER_RAMZA) {
        report_missing_unit_stub(original_unit_id);
        return EVENT_MAP_UNIT_ID_NONE;
    }
    for (battle_unit_id = 0; battle_unit_id < BATTLE_UNIT_SLOT_COUNT; battle_unit_id++) {
        if (battle_unit_get(battle_unit_id) == unit)
            break;
    }
    unit_id = map_unit_get_id_by_battle_unit_id(battle_unit_id);
    if (unit_id == -1
        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
        report_missing_unit_stub(original_unit_id);
        return EVENT_MAP_UNIT_ID_NONE;
    }
    return unit_id;
}
