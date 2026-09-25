#include "fft/battle.h"
#include "fft/data.h"
#include "fft/main_runtime.h"
#include "fft/main_unit.h"
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
    world_formation_unit_t record;
    battle_stats_t unit;
    s32 roster_slot;
    s32 i;
    s32 item;

    g_world_formation_selected_unit_index_latch = g_world_formation_selected_unit_index;
    roster_slot = g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->roster_slot;
    main_unit_init_job_data(&unit, roster_slot, 1);
    for (i = 0; i < 7; i++) {
        unit.equipment[i] = ITEM_ID_NONE;
    }
    item = g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment[0] & WORLD_ITEM_ID_MASK;
    if (item != ITEM_ID_NOTHING) {
        if (g_main_item_primary_data[item].type == ITEM_TYPE_SHIELD) {
            unit.equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = item;
        } else {
            unit.equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = item;
        }
    }
    item = g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment[1] & WORLD_ITEM_ID_MASK;
    if (item != ITEM_ID_NOTHING) {
        if (g_main_item_primary_data[item].type == ITEM_TYPE_SHIELD) {
            unit.equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = item;
        } else {
            unit.equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = item;
        }
    }
    unit.equipment[UNIT_EQUIPMENT_SLOT_HEAD]
        = g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment[2];
    unit.equipment[UNIT_EQUIPMENT_SLOT_BODY]
        = g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment[3];
    unit.equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY]
        = g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment[4];
    for (i = 0; i < 7; i++) {
        item = unit.equipment[i];
        if (item == ITEM_ID_NOTHING || item >= 0xfe) {
            unit.equipment[i] = ITEM_ID_NONE;
        }
    }
    main_unit_update_formation_stats(&unit);
    world_formation_build_unit_record(&unit, &record, main_party_get_data_pointer(roster_slot));
    bcopy(g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment, record.equipment, 0xa);
    record.two_handed
        = world_item_check_two_hands_for_weapons((struct weapon_pair*)record.equipment, record.support_sets_3 & 2);
    bcopy(&record, &g_world_selected_unit_stat_summary, 0x22);
    bcopy(record.name_index, &g_world_selected_unit_identity, 0xe);
    bcopy(&record.move, &g_world_selected_unit_stat_detail, 0x40);
    world_item_sum_equipment_stat_details(&g_world_selected_unit_stat_detail,
        g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment);
    world_formation_copy_unit_to_temp(g_world_formation_selected_unit_index, 20);
    world_menu_enable_all_order_entries();
}
