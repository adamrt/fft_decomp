#include "fft/battle.h"
#include "fft/data.h"
#include "fft/require.h"
#include "fft/unit_slots.h"
#include "psx/types.h"

/* Apply permanent Brave/Faith changes and collect party-leaving warnings.
 *
 * The staged multiplication and divisions preserve the target's two signed
 * constant-division sequences. */
s32 require_apply_permanent_brave_faith_changes_and_collect_warnings(void) {
    s32 battle_index;
    s32 warning_count = 0;
    battle_stats_t* battle_unit;
    battle_stats_t* unit;
    s32 found_battle_index;
    s32 brave;
    s32 faith;
    s32 base;
    s32 raw;
    s32 delta1;
    s32 delta2;
    s32 value1;
    s32 value2;
    s32 value3;
    s32 value4;

    g_require_party_affected_unit_count = 0;
    for (battle_index = 0; battle_index < BATTLE_UNIT_SLOT_COUNT; battle_index++) {
        battle_unit = battle_unit_get_stats_from_battle_id(battle_index);
        unit = battle_find_unit_data_pointer_for_entd_unit_id(battle_unit->unit_id, &found_battle_index);
        if (found_battle_index < 0 || battle_index != found_battle_index
            || unit->formation_index == BATTLE_FORMATION_INDEX_NONE || unit->unit_id == 0) {
            continue;
        }

        base = unit->original_brave;
        raw = unit->brave;
        delta1 = raw - base;
        delta2 = delta1 * 250;
        raw = delta2 / 100;
        raw = raw / 10;
        brave = base + raw;
        unit->original_brave = brave;
        unit->brave = brave;
        base = unit->original_faith;
        raw = unit->faith;
        delta1 = raw - base;
        delta2 = delta1 * 250;
        raw = delta2 / 100;
        raw = raw / 10;
        faith = base + raw;
        unit->original_faith = faith;
        unit->faith = faith;

        if (unit->formation_index >= PARTY_GUEST_SLOT_FIRST
            || (unit->character_identity != CHARACTER_IDENTITY_ENTD_NONE
                && unit->character_identity <= CHARACTER_IDENTITY_RAMZA_CHAPTER_4)
            || (unit->unit_id != 0 && unit->unit_id < 4)) {
            continue;
        }

        if ((u32)(brave - 6) < 10) {
            if (unit->character_identity < 0x80) {
                s32 column;

                for (brave = 0; brave < 0x49; brave++) {
                    for (column = 0; column < 3; column++) {
                        if (unit->character_identity == g_require_gfx_formation_sprite_groups[brave][column]) {
                            break;
                        }
                    }
                    if (column != 3) {
                        break;
                    }
                }
                if (brave == 0x49) {
                    brave = 0;
                }
                if (brave == 0x47) {
                    brave = 0x48;
                }
                value1 = g_require_party_affected_unit_count;
                g_require_party_affected_unit_message_args[value1] = unit->character_identity;
                g_require_party_affected_unit_message_ids[value1] = brave + 0x818;
                g_require_party_affected_battle_unit_indices[value1] = battle_index;
                g_require_party_affected_unit_count = value1 + 1;
            } else {
                value2 = g_require_party_affected_unit_count;
                g_require_party_affected_unit_message_args[value2] = unit->unit_id;
                g_require_party_affected_unit_message_ids[value2]
                    = (unit->character_identity - 0x80) * 4 + (battle_index & 3) + 0x800;
                g_require_party_affected_battle_unit_indices[value2] = battle_index;
                g_require_party_affected_unit_count = value2 + 1;
            }
            warning_count++;
        }

        if ((u32)(faith - 0x55) < 10) {
            if (unit->character_identity < 0x80) {
                s32 group_index;
                s32 column;

                for (group_index = 0; group_index < 0x49; group_index++) {
                    for (column = 0; column < 3; column++) {
                        if (unit->character_identity == g_require_gfx_formation_sprite_groups[group_index][column]) {
                            break;
                        }
                    }
                    if (column != 3) {
                        break;
                    }
                }
                if (group_index == 0x49) {
                    group_index = 0;
                }
                if (group_index == 0x47) {
                    group_index = 0x48;
                }
                value3 = g_require_party_affected_unit_count;
                g_require_party_affected_unit_message_args[value3] = unit->unit_id;
                g_require_party_affected_unit_message_ids[value3] = group_index + 0x862;
                g_require_party_affected_battle_unit_indices[value3] = battle_index;
                g_require_party_affected_unit_count = value3 + 1;
            } else {
                value4 = g_require_party_affected_unit_count;
                g_require_party_affected_unit_message_args[value4] = unit->unit_id;
                g_require_party_affected_unit_message_ids[value4]
                    = (unit->character_identity - 0x80) * 4 + (battle_index & 3) + 0x80c;
                g_require_party_affected_battle_unit_indices[value4] = battle_index;
                g_require_party_affected_unit_count = value4 + 1;
            }
            warning_count++;
        }
    }
    return warning_count;
}
