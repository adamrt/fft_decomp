#include "fft/world.h"
#include "psx/types.h"

/*
 * Change the unit's job and optionally select replacement equipment.
 *
 * Mime removes all equipment. Slot 20 does not exchange inventory items;
 * ordinary slots return their old equipment before reserving replacements.
 */
s32 world_formation_change_unit_job(s16 unit_id, s16 job_id, s32 update_equipment) {
    u16 equipment[5];
    s32 i;
    g_world_formation_unit_pointers[unit_id]->job_id = job_id;
    if (job_id == JOB_ID_MIME) {
        for (i = 0; i < 5; i++) {
            if (unit_id != 20) {
                world_item_change_quantity_on_equip((s16)g_world_formation_unit_pointers[unit_id]->equipment[i], 1);
            }
            g_world_formation_unit_pointers[unit_id]->equipment[i] = ITEM_ID_NOTHING;
        }
    }
    world_formation_save_records_to_party_data();
    world_formation_remove_invalid_unit_loadout(unit_id);
    if (update_equipment != 0) {
        if (unit_id != 20) {
            if (g_world_formation_unit_pointers[unit_id]->job_id != JOB_ID_MIME) {
                world_unit_build_best_fit_equipment(unit_id, (s16*)equipment);
                for (i = 0; i < 5; i++) {
                    world_item_change_quantity_on_equip((s16)g_world_formation_unit_pointers[unit_id]->equipment[i], 1);
                }
                for (i = 0; i < 5; i++) {
                    g_world_formation_unit_pointers[unit_id]->equipment[i] = equipment[i];
                    world_item_change_quantity_on_equip((s16)equipment[i], -1);
                }
                world_formation_save_records_to_party_data();
                world_menu_run_unit_status_banner_countdown(0xD811, 0x1E);
            }
        }
    }
    return 1;
}
