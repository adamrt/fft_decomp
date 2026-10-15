#include "fft/battle_script.h"
#include "fft/event_require.h"
#include "fft/script.h"
#include "psx/types.h"

/*
 * End-of-battle party update. Returns the number of units that still need a
 * "join the party?" prompt.
 *
 * First pass, every occupied roster slot that fielded a battle unit: a unit
 * that is gone (disabled, absent or not found) and is not one of ENTD units
 * 1..3 is removed from the roster, its equipment going back to the inventory
 * unless it became a treasure; Ramza's identities are always saved; a guest
 * without the save-formation flag is dropped; anyone else is saved.
 *
 * Second pass, every present battle unit with no roster slot that is not a
 * Ramza identity, has unit id 4 or more and either carries the
 * join-after-event flag or is invited: one with the save-formation flag joins
 * at once (equipment over the inventory limit is sold); the others are queued
 * for the prompt with message 0x1800 + (identity - 0x80) * 8 + a variant from
 * the random script variable.
 */
s32 require_party_save_battle_units_and_find_join_candidates(void) {
    s32 prompt_count = 0;
    s32 party_index;
    s32 found_index;
    s32 i;

    g_require_party_affected_unit_count = 0;
    for (party_index = 0; party_index < PARTY_ROSTER_SLOT_COUNT; party_index++) {
        party_unit_t* party_unit = main_party_get_unit(party_index);

        if (party_unit->party_id != PARTY_ID_NONE) {
            unit_stats_t* unit;

            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                unit = battle_unit_get_stats_from_battle_id(i);
                if (unit->formation_index == party_index && unit->existence != BATTLE_UNIT_EXISTENCE_NONE
                    && unit->unit_id != 0) {
                    break;
                }
            }

            if (i != BATTLE_UNIT_ID_NONE) {
                unit = battle_unit_get_stats_from_battle_id(i);
                unit = battle_unit_find_stats_by_entd_unit_id(unit->unit_id, &found_index);
                if (unit->existence == BATTLE_UNIT_EXISTENCE_DISABLED || unit->existence == BATTLE_UNIT_EXISTENCE_NONE
                    || found_index == -3) {
                    if (unit->unit_id == 0 || unit->unit_id >= 4) {
                        if (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)]
                                & STATUS_BYTE_MASK(STATUS_ID_TREASURE)))
                            require_party_store_equipment_or_sell_overflow(i);
                        main_party_remove_unit(party_index);
                        require_party_update_bio_variable_for_unit_class(unit->character_identity, 0xf);
                    }
                } else if (unit->character_identity != CHARACTER_IDENTITY_ENTD_NONE
                    && unit->character_identity < CHARACTER_IDENTITY_RAMZA_END) {
                    main_party_save_unit(unit, 0);
                } else if (!(unit->unit_flags & UNIT_FLAG_SAVE_FORMATION)
                    && unit->formation_index >= PARTY_GUEST_SLOT_FIRST) {
                    main_party_remove_unit(unit->formation_index);
                    unit->formation_index = BATTLE_FORMATION_INDEX_NONE;
                } else {
                    main_party_save_unit(unit, unit->unit_flags & UNIT_FLAG_SAVE_FORMATION);
                }
            }
        }
    }

    for (party_index = 0; party_index < BATTLE_UNIT_SLOT_COUNT; party_index++) {
        unit_stats_t* unit;
        unit_stats_t* battle_unit;

        battle_unit = battle_unit_get_stats_from_battle_id(party_index);
        unit = battle_unit_find_stats_by_entd_unit_id(battle_unit->unit_id, &found_index);
        if (found_index >= 0 && party_index == found_index && unit->formation_index == BATTLE_FORMATION_INDEX_NONE
            && unit->unit_id != 0) {
            u32 base_class = unit->character_identity;
            if ((base_class == CHARACTER_IDENTITY_ENTD_NONE || base_class >= CHARACTER_IDENTITY_RAMZA_END)
                && unit->unit_id >= 4
                && ((unit->unit_flags & UNIT_FLAG_JOIN_AFTER_EVENT)
                    || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_INVITE)]
                        & STATUS_BYTE_MASK(STATUS_ID_INVITE)))) {
                if (unit->unit_flags & UNIT_FLAG_SAVE_FORMATION) {
                    s32 column;

                    /* Retail looks up the unit's formation sprite group and
                     * then does not use it. */
                    for (i = 0; i < 0x49; i++) {
                        for (column = 0; column < 3; column++) {
                            if (unit->character_identity == g_require_gfx_formation_sprite_groups[i][column]) {
                                break;
                            }
                        }
                        if (column != 3) {
                            break;
                        }
                    }
                    if (i == 0x49) {
                        i = 0;
                    }
                    if (i == 0x47) {
                        i = 0x48;
                    }
                    require_party_sell_equipment_over_limit(party_index);
                    main_party_save_unit(unit, unit->unit_flags & UNIT_FLAG_SAVE_FORMATION);
                } else {
                    s32 count;
                    s32 script_index = EVENT_SCRIPT_VAR_RANDOM_VALUE;
                    s32 script_variant;
                    unit->unit_flags &= ~(UNIT_FLAG_SAVE_FORMATION | UNIT_FLAG_LOAD_FORMATION);
                    g_require_party_affected_unit_message_args[g_require_party_affected_unit_count] = unit->unit_id;
                    script_variant = script_get_variable(script_index) & 7;
                    prompt_count++;
                    count = g_require_party_affected_unit_count;
                    g_require_party_affected_unit_message_ids[count]
                        = ((unit->character_identity - CHARACTER_IDENTITY_SELECTOR_FIRST) << 3) + script_variant
                        + 0x1800;
                    g_require_party_affected_battle_unit_indices[count] = party_index;
                    g_require_party_affected_unit_count = count + 1;
                }
            }
        }
    }
    return prompt_count;
}
