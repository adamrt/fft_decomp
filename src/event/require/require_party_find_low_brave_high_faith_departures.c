#include "fft/event_require.h"
#include "psx/types.h"

/* Collect the party members that leave after the battle: Brave below 6 or
 * Faith of 95 and above. Story characters are named by their sprite group's
 * message; generic units by job and battle slot. Returns how many leave. */
s32 require_party_find_low_brave_high_faith_departures(void) {
    s32 battle_index;
    s32 departure_count = 0;
    unit_stats_t* battle_unit;
    unit_stats_t* unit;
    s32 found_battle_index;
    s32 brave;
    s32 faith;
    s32 group;
    s32 column;

    g_require_party_affected_unit_count = 0;
    for (battle_index = 0; battle_index < BATTLE_UNIT_SLOT_COUNT; battle_index++) {
        battle_unit = battle_unit_get_stats_from_battle_id(battle_index);
        unit = battle_unit_find_stats_by_entd_unit_id(battle_unit->unit_id, &found_battle_index);
        if (found_battle_index < 0 || battle_index != found_battle_index
            || unit->formation_index == BATTLE_FORMATION_INDEX_NONE || unit->unit_id == 0) {
            continue;
        }
        if (unit->formation_index >= PARTY_GUEST_SLOT_FIRST
            || (unit->character_identity != CHARACTER_IDENTITY_ENTD_NONE
                && unit->character_identity <= CHARACTER_IDENTITY_RAMZA_CHAPTER_4)
            || (unit->unit_id != 0 && unit->unit_id < 4)) {
            continue;
        }

        brave = unit->original_brave;
        faith = unit->original_faith;
        if (brave < 6) {
            if (unit->character_identity < CHARACTER_IDENTITY_SELECTOR_FIRST) {
                s32 count;

                for (group = 0; group < 0x49; group++) {
                    for (column = 0; column < 3; column++) {
                        if (unit->character_identity == g_require_gfx_formation_sprite_groups[group][column]) {
                            break;
                        }
                    }
                    if (column != 3) {
                        break;
                    }
                }
                if (group == 0x49) {
                    group = 0;
                }
                if (group == 0x47) {
                    group = 0x48;
                }
                count = g_require_party_affected_unit_count;
                g_require_party_affected_unit_message_args[count] = unit->character_identity;
                g_require_party_affected_unit_message_ids[count] = group + 0x1018;
                g_require_party_affected_battle_unit_indices[count] = battle_index;
                g_require_party_affected_unit_count = count + 1;
                require_party_update_bio_variable_for_unit_class(unit->character_identity, 0xd);
            } else {
                s32 count;

                count = g_require_party_affected_unit_count;
                g_require_party_affected_unit_message_args[count] = unit->unit_id;
                g_require_party_affected_unit_message_ids[count]
                    = (unit->character_identity - CHARACTER_IDENTITY_SELECTOR_FIRST) * 4 + (battle_index & 3) + 0x1000;
                g_require_party_affected_battle_unit_indices[count] = battle_index;
                g_require_party_affected_unit_count = count + 1;
            }
            /* The target passes three arguments the callee does not take. */
            require_party_report_brave_departure_stub(battle_index, unit->unit_id, unit->character_identity);
            departure_count++;
        } else if (faith >= 0x5f) {
            if (unit->character_identity < CHARACTER_IDENTITY_SELECTOR_FIRST) {
                s32 count;

                for (group = 0; group < 0x49; group++) {
                    for (column = 0; column < 3; column++) {
                        if (unit->character_identity == g_require_gfx_formation_sprite_groups[group][column]) {
                            break;
                        }
                    }
                    if (column != 3) {
                        break;
                    }
                }
                if (group == 0x49) {
                    group = 0;
                }
                if (group == 0x47) {
                    group = 0x48;
                }
                count = g_require_party_affected_unit_count;
                g_require_party_affected_unit_message_args[count] = unit->unit_id;
                g_require_party_affected_unit_message_ids[count] = group + 0x1062;
                g_require_party_affected_battle_unit_indices[count] = battle_index;
                g_require_party_affected_unit_count = count + 1;
                require_party_update_bio_variable_for_unit_class(unit->character_identity, 0xe);
            } else {
                s32 count;

                count = g_require_party_affected_unit_count;
                g_require_party_affected_unit_message_args[count] = unit->unit_id;
                g_require_party_affected_unit_message_ids[count]
                    = (unit->character_identity - CHARACTER_IDENTITY_SELECTOR_FIRST) * 4 + (battle_index & 3) + 0x100c;
                g_require_party_affected_battle_unit_indices[count] = battle_index;
                g_require_party_affected_unit_count = count + 1;
            }
            departure_count++;
        }
    }
    return departure_count;
}
