/* Unit map coordinate copies by runtime id and the missing unit stub. */

#include "fft/battle_unit.h"
#include "fft/battle_unit_animation.h"
#include "fft/battle_unit_encounter.h"
#include "fft/data_status.h"
#include "fft/script.h"
#include "fft/script_instruction.h"
#include "fft/script_variable.h"
#include "fft/unit.h"
#include "fft/unit_entd.h"
#include "fft/unit_identity.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* battle_unit_get_map_coords_by_runtime_id without its result: the unit's tile as x, z, y. */
void unit_copy_map_coords_by_runtime_id(u32 runtime_id, SVECTOR* coords) {
    battle_unit_get_map_coords_by_runtime_id(runtime_id, coords);
}

/* Empty. script_resolve_runtime_id passes it each unit id it cannot resolve before returning
 * EVENT_RUNTIME_ID_NONE. */
static void script_report_missing_unit_stub(s32 unused_unit_id) {
}

/* Resolve an event-script unit identifier to its active runtime ID.
 *
 * - A character identity becomes that unit's ENTD unit id when it is one of the
 *   deployed ids (EVENT_UNIT_ID_DEPLOYED_FIRST up).
 * - EVENT_UNIT_SELECTOR_IMPORTANT_UNIT names the first ENTD unit with an importance flag.
 * - EVENT_UNIT_SELECTOR_ACTIVE_TURN names the unit whose turn it is, or failing that
 *   ENTD unit 1 or 2.
 * - In the Ramza game-over event every id names the first unit with a Ramza identity.
 * - The four event unit slots return the runtime id stored in them.
 * - Any other id is looked up as an ENTD unit id.
 *
 * Missing units and units with Treasure status return EVENT_RUNTIME_ID_NONE, except
 * that the game-over path returns -1 when no Ramza is on the field (retail).
 */
s32 script_resolve_runtime_id(s32 unit_id) {
    s32 battle_id;
    s32 unit_state;
    s32 original_unit_id;
    s32 runtime_id;
    unit_stats_t* unit;

    if (unit_id >= CHARACTER_IDENTITY_RAMZA_CHAPTER_1 && unit_id < CHARACTER_IDENTITY_GENERIC_FIRST) {
        battle_id = battle_unit_find_slot_by_character_identity(unit_id);
        if (battle_id >= 0) {
            unit = battle_unit_get_stats_from_battle_id(battle_id);
            if (unit->unit_id >= EVENT_UNIT_ID_DEPLOYED_FIRST && unit->unit_id < EVENT_UNIT_ID_DEPLOYED_END)
                unit_id = unit->unit_id;
        }
    }
    original_unit_id = unit_id;

    if (unit_id == EVENT_UNIT_SELECTOR_IMPORTANT_UNIT) {
        for (battle_id = 0; battle_id < ENTD_UNITS_PER_ENCOUNTER; battle_id++) {
            unit = battle_unit_get_stats_from_battle_id(battle_id);
            if (unit->team_flags & BATTLE_TEAM_IMPORTANCE_MASK)
                break;
        }
        if (battle_id == ENTD_UNITS_PER_ENCOUNTER) {
            script_report_missing_unit_stub(original_unit_id);
            return EVENT_RUNTIME_ID_NONE;
        }
        unit_id = unit->unit_id;
    } else if (unit_id == EVENT_UNIT_SELECTOR_ACTIVE_TURN) {
        for (battle_id = 0; battle_id < BATTLE_UNIT_SLOT_COUNT; battle_id++) {
            if (battle_unit_has_runtime_id(battle_id)) {
                s32 active_battle_id = battle_unit_get_battle_id_by_runtime_id(battle_id);

                unit_id = active_battle_id;
                if (active_battle_id != -1) {
                    unit = battle_unit_get_stats_from_battle_id(active_battle_id);
                    if (unit->has_turn)
                        break;
                }
            }
        }
        if (battle_id != BATTLE_UNIT_ID_NONE) {
            unit_id = unit->unit_id;
            battle_unit_find_stats_by_entd_unit_id(unit_id, &battle_id);
        } else {
            battle_id = -1;
        }

        if (battle_id < 0) {
            for (unit_id = 1; unit_id < 3; unit_id++) {
                unit = battle_unit_find_stats_by_entd_unit_id(unit_id, &battle_id);
                if (battle_id >= 0)
                    break;
            }
            /* Retail compares the lookup result rather than the loop counter with 3, so
             * finding neither unit is not caught here. */
            if (battle_id == 3
                || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_JUMP)] & STATUS_BYTE_MASK(STATUS_ID_JUMP))) {
                script_report_missing_unit_stub(original_unit_id);
                return EVENT_RUNTIME_ID_NONE;
            }
        }
    }

    if (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) == EVENT_ID_GAME_OVER_RAMZA) {
        for (battle_id = 0; battle_id < BATTLE_UNIT_SLOT_COUNT; battle_id++) {
            unit = battle_unit_get_stats_from_battle_id(battle_id);
            if (unit->character_identity != CHARACTER_IDENTITY_ENTD_NONE
                && unit->character_identity < CHARACTER_IDENTITY_RAMZA_END)
                break;
        }
        if (battle_id == BATTLE_UNIT_ID_NONE)
            return -1;
        unit_id = unit->unit_id;
    }

    if (unit_id >= EVENT_UNIT_SELECTOR_SLOT_FIRST
        && unit_id < EVENT_UNIT_SELECTOR_SLOT_FIRST + EVENT_UNIT_SELECTOR_SLOT_COUNT) {
        runtime_id = (u16)g_script_unit_slots[unit_id - EVENT_UNIT_SELECTOR_SLOT_FIRST];
        if (runtime_id == EVENT_UNIT_SLOT_EMPTY) {
            script_report_missing_unit_stub(original_unit_id);
            return EVENT_RUNTIME_ID_NONE;
        }
        return runtime_id;
    }

    unit = battle_unit_find_stats_by_entd_unit_id(unit_id, &unit_state);
    if (unit_state < 0 && unit_state != -2
        && script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) != EVENT_ID_GAME_OVER_RAMZA) {
        script_report_missing_unit_stub(original_unit_id);
        return EVENT_RUNTIME_ID_NONE;
    }
    for (battle_id = 0; battle_id < BATTLE_UNIT_SLOT_COUNT; battle_id++) {
        if (battle_unit_get_stats_from_battle_id(battle_id) == unit)
            break;
    }
    unit_id = battle_unit_get_runtime_id_by_battle_id(battle_id);
    if (unit_id == -1
        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
        script_report_missing_unit_stub(original_unit_id);
        return EVENT_RUNTIME_ID_NONE;
    }
    return unit_id;
}
