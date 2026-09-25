#include "fft/battle.h"
#include "fft/world.h"

/* Biased base: encoded event-unit IDs 0x64..0x67 reach the four halfwords at
 * g_world_event_unit_slots without subtracting 0x64 in the target. */
extern u16 g_world_event_unit_slots_by_encoded_id[];

/* Resolve an event-script unit identifier to its active Misc Unit ID.
 *
 * Character identities, Ramza and active-turn aliases, and the four event-unit
 * slots require separate lookup paths. Crystal/treasure records and missing
 * units return EVENT_MISC_ID_NONE.
 */
s32 world_get_misc_id(s32 unit_id) {
    s32 battle_id;
    s32 unit_state;
    /* Pinned so cse keeps this copy out of the 0x68/0x69 compares. */
    register s32 original_unit_id __asm__("$18");
    s32 misc_id;
    battle_stats_t* unit;

    if ((u32)(unit_id - CHARACTER_IDENTITY_RAMZA_CHAPTER_1)
        < CHARACTER_IDENTITY_GENERIC_FIRST - CHARACTER_IDENTITY_RAMZA_CHAPTER_1) {
        battle_id = battle_classify_character_identity_slot(unit_id);
        if (battle_id >= 0) {
            unit = battle_unit_get_stats_from_battle_id(battle_id);
            if (unit->unit_id >= EVENT_UNIT_ID_DEPLOYED_FIRST && unit->unit_id < EVENT_UNIT_ID_DEPLOYED_END)
                unit_id = unit->unit_id;
        }
    }
    original_unit_id = unit_id;

    if (unit_id == 0x68) {
        for (battle_id = 0; battle_id < 16; battle_id++) {
            unit = battle_unit_get_stats_from_battle_id(battle_id);
            if (unit->team_flags & 3)
                break;
        }
        if (battle_id == 16) {
            world_noop_800e7808(original_unit_id);
            return EVENT_MISC_ID_NONE;
        }
        unit_id = unit->unit_id;
    } else if (unit_id == EVENT_UNIT_SELECTOR_ACTIVE_TURN) {
        for (battle_id = 0; battle_id < BATTLE_UNIT_SLOT_COUNT; battle_id++) {
            if (battle_unit_has_misc_id(battle_id)) {
                s32 active_battle_id = battle_unit_get_battle_index_by_misc_id(battle_id);

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
            battle_find_unit_data_pointer_for_entd_unit_id(unit_id, &battle_id);
        } else {
            battle_id = -1;
        }

        if (battle_id < 0) {
            for (unit_id = 1; unit_id < 3; unit_id++) {
                unit = battle_find_unit_data_pointer_for_entd_unit_id(unit_id, &battle_id);
                if (battle_id >= 0)
                    break;
            }
            if (battle_id == 3 || (unit->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_JUMP))) {
                world_noop_800e7808(original_unit_id);
                return EVENT_MISC_ID_NONE;
            }
        }
    }

    if (world_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) == 0x194) {
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

    if ((u32)(unit_id - EVENT_UNIT_SELECTOR_SLOT_FIRST) < EVENT_UNIT_SELECTOR_SLOT_COUNT) {
        misc_id = g_world_event_unit_slots_by_encoded_id[unit_id];
        if (misc_id != EVENT_UNIT_SLOT_EMPTY) {
            return misc_id;
        }
    } else {
        unit = battle_find_unit_data_pointer_for_entd_unit_id(unit_id, &unit_state);
        if (unit_state >= 0 || unit_state == -2 || world_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) == 0x194) {
            for (battle_id = 0; battle_id < BATTLE_UNIT_SLOT_COUNT; battle_id++) {
                if (battle_unit_get_stats_from_battle_id(battle_id) == unit)
                    break;
            }
            unit_id = battle_unit_get_misc_id_by_battle_id(battle_id);
            if (unit_id != -1 && !(unit->status_sets.current[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TREASURE)))
                return unit_id;
        }
    }

    world_noop_800e7808(original_unit_id);
    return EVENT_MISC_ID_NONE;
}
