#include "fft/job.h"
#include "fft/main_runtime.h"
#include "fft/main_unit.h"
#include "fft/unit_slots.h"
#include "psx/types.h"

/* Counts how many of `item_id` the player owns: the inventory stock plus the
 * copies equipped on roster units.  With `include_battle` set, deployed
 * roster units are skipped and the battle units on the player team (except
 * mimes) are counted instead. */
s32 main_item_get_total_equipment_quantity(u8 item_id, s32 include_battle) {
    s32 total;
    s32 i;
    s32 j;
    party_data_t* party;
    battle_stats_t* unit;
    s32 deployed;
    s32 party_id;

    total = g_main_item_quantities[item_id];
    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        party = main_party_get_data_pointer(i);
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
                if (unit->existence != 0xff && !(unit->unit_flags & UNIT_FLAG_MONSTER)
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
        if (unit->existence != 0xff && !(unit->unit_flags & UNIT_FLAG_MONSTER)
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
