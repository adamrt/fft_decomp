#include "fft/data.h"
#include "fft/main_unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

/*
 * Write each formation record's job, abilities, and equipment back into its
 * roster party_data_t, then rebuild the record from the refreshed unit.
 *
 * Records using monster skillsets keep their party equipment, learned
 * abilities, and JP. Shields go to the hand's shield byte.
 */
void world_formation_save_records_to_party_data(void) {
    battle_stats_t unit;
    party_data_t* party;
    s32 i;
    s16 slot;
    u32 item;
    u8 value;

    for (i = 0; i < g_world_formation_record_count; i++) {
        slot = g_world_formation_unit_pointers[i]->roster_slot;
        party = main_party_get_data_pointer(slot);
        party->job_id = g_world_formation_unit_pointers[i]->job_id;
        party->gender_flags = g_world_formation_unit_pointers[i]->gender_flags;
        party->secondary_skillset = g_world_formation_unit_pointers[i]->secondary_skillset;
        party->reaction_ability[0] = g_world_formation_unit_pointers[i]->reaction_ability;
        party->reaction_ability[1] = (u16)g_world_formation_unit_pointers[i]->reaction_ability >> 8;
        party->support_ability[0] = g_world_formation_unit_pointers[i]->support_ability;
        party->support_ability[1] = (u16)g_world_formation_unit_pointers[i]->support_ability >> 8;
        party->movement_ability[0] = g_world_formation_unit_pointers[i]->movement_ability;
        party->movement_ability[1] = (u16)g_world_formation_unit_pointers[i]->movement_ability >> 8;
        if (g_world_formation_unit_pointers[i]->uses_monster_skillset == 0) {
            item = g_world_formation_unit_pointers[i]->equipment[0];
            party->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = ITEM_ID_NONE;
            party->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = ITEM_ID_NONE;
            if (item != ITEM_ID_NOTHING) {
                if (g_main_item_primary_data[item].type == ITEM_TYPE_SHIELD) {
                    party->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = item;
                } else {
                    party->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = item;
                }
            }
            item = g_world_formation_unit_pointers[i]->equipment[1];
            party->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = ITEM_ID_NONE;
            party->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = ITEM_ID_NONE;
            if (item != ITEM_ID_NOTHING) {
                if (g_main_item_primary_data[item].type == ITEM_TYPE_SHIELD) {
                    party->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = item;
                } else {
                    party->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = item;
                }
            }
            value = ITEM_ID_NONE;
            if (g_world_formation_unit_pointers[i]->equipment[2] != ITEM_ID_NOTHING) {
                value = g_world_formation_unit_pointers[i]->equipment[2];
            }
            party->equipment[UNIT_EQUIPMENT_SLOT_HEAD] = value;
            value = ITEM_ID_NONE;
            if (g_world_formation_unit_pointers[i]->equipment[3] != ITEM_ID_NOTHING) {
                value = g_world_formation_unit_pointers[i]->equipment[3];
            }
            party->equipment[UNIT_EQUIPMENT_SLOT_BODY] = value;
            value = ITEM_ID_NONE;
            if (g_world_formation_unit_pointers[i]->equipment[4] != ITEM_ID_NOTHING) {
                value = g_world_formation_unit_pointers[i]->equipment[4];
            }
            party->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY] = value;
            bcopy(g_world_formation_unit_pointers[i]->learned_abilities, party->learned_abilities,
                sizeof(party->learned_abilities));
            bcopy(g_world_formation_unit_pointers[i]->job_points, party->job_points, sizeof(party->job_points));
            bcopy(g_world_formation_unit_pointers[i]->total_job_points, party->total_job_points,
                sizeof(party->total_job_points));
        }
        main_unit_init_job_data(&unit, slot, 1);
        world_formation_build_unit_record(&unit, g_world_formation_unit_pointers[i], party);
    }
}
