#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"

/*
 * Rebuild the selected formation unit's derived stats from its job and
 * equipment, then refresh the three display snapshots.
 *
 * Shields in either hand go to the battle record's shield slots. Like
 * world_formation_stage_selected_unit, this needs O1 for the
 * target's store order and unreduced loop addressing.
 */
void world_formation_recalculate_selected_unit_stats(void) {
    /* Preserve the target's unused 128-byte stack area. */
    u8 unused[128];
    world_unit_t record;
    unit_stats_t unit;
    s32 roster_slot;
    s32 i;
    s32 item;

    g_world_formation_selected_unit_index_latch = g_unit_selected_index;
    roster_slot = g_world_unit_pointers[g_unit_selected_index]->identity.roster_slot;
    main_unit_init_job_data(&unit, roster_slot, 1);
    for (i = 0; i < UNIT_EQUIPMENT_SLOT_COUNT; i++) {
        unit.equipment[i] = ITEM_ID_NONE;
    }
    item
        = g_world_unit_pointers[g_unit_selected_index]->equipment[WORLD_EQUIPMENT_SLOT_RIGHT_HAND] & WORLD_ITEM_ID_MASK;
    if (item != ITEM_ID_NOTHING) {
        if (g_main_item_primary_data[item].type == ITEM_TYPE_SHIELD) {
            unit.equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = item;
        } else {
            unit.equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = item;
        }
    }
    item = g_world_unit_pointers[g_unit_selected_index]->equipment[WORLD_EQUIPMENT_SLOT_LEFT_HAND] & WORLD_ITEM_ID_MASK;
    if (item != ITEM_ID_NOTHING) {
        if (g_main_item_primary_data[item].type == ITEM_TYPE_SHIELD) {
            unit.equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = item;
        } else {
            unit.equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = item;
        }
    }
    unit.equipment[UNIT_EQUIPMENT_SLOT_HEAD]
        = g_world_unit_pointers[g_unit_selected_index]->equipment[WORLD_EQUIPMENT_SLOT_HEAD];
    unit.equipment[UNIT_EQUIPMENT_SLOT_BODY]
        = g_world_unit_pointers[g_unit_selected_index]->equipment[WORLD_EQUIPMENT_SLOT_BODY];
    unit.equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY]
        = g_world_unit_pointers[g_unit_selected_index]->equipment[WORLD_EQUIPMENT_SLOT_ACCESSORY];
    for (i = 0; i < UNIT_EQUIPMENT_SLOT_COUNT; i++) {
        item = unit.equipment[i];
        if (item == ITEM_ID_NOTHING || item >= ITEM_ID_END) {
            unit.equipment[i] = ITEM_ID_NONE;
        }
    }
    main_unit_update_formation_stats(&unit);
    world_formation_build_unit_record(&unit, &record, main_party_get_unit(roster_slot));
    bcopy(g_world_unit_pointers[g_unit_selected_index]->equipment, record.equipment, sizeof(record.equipment));
    record.two_handed = unit_is_two_handing_weapon(
        (s16*)record.equipment, record.support_abilities[2] & BATTLE_SUPPORT_SET_3_TWO_HANDS);
    bcopy(&record.status, &g_world_selected_unit_stat_summary, sizeof(menu_unit_status_record_t));
    bcopy(&record.identity, &g_world_selected_unit_identity, sizeof(menu_unit_status_identity_t));
    bcopy(&record.move, &g_world_selected_unit_stat_detail, 0x40);
    item_sum_equipment_stat_details(
        &g_world_selected_unit_stat_detail, g_world_unit_pointers[g_unit_selected_index]->equipment);
    world_formation_copy_unit_to_temp(g_unit_selected_index, WORLD_FORMATION_PREVIEW_UNIT_INDEX);
    world_menu_enable_all_order_entries();
}
