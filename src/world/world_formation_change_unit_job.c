#include "fft/world.h"
#include "psx/types.h"

/*
 * Change the unit's job and optionally select replacement equipment.
 *
 * Mime removes all equipment. The preview record does not exchange inventory
 * items; ordinary slots return their old equipment before reserving replacements.
 */
s32 world_formation_change_unit_job(s16 unit_id, s16 job_id, s32 update_equipment) {
    s16 equipment[WORLD_EQUIPMENT_SLOT_COUNT];
    s32 i;
    g_world_unit_pointers[unit_id]->identity.job_id = job_id;
    if (job_id == JOB_ID_MIME) {
        for (i = 0; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
            if (unit_id != WORLD_FORMATION_PREVIEW_UNIT_INDEX) {
                item_adjust_inventory_count(g_world_unit_pointers[unit_id]->equipment[i], 1);
            }
            g_world_unit_pointers[unit_id]->equipment[i] = ITEM_ID_NOTHING;
        }
    }
    world_formation_save_records_to_party();
    world_formation_remove_invalid_unit_loadout(unit_id);
    if (update_equipment != 0) {
        if (unit_id != WORLD_FORMATION_PREVIEW_UNIT_INDEX) {
            if (g_world_unit_pointers[unit_id]->identity.job_id != JOB_ID_MIME) {
                world_unit_build_best_fit_equipment(unit_id, equipment);
                for (i = 0; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
                    item_adjust_inventory_count(g_world_unit_pointers[unit_id]->equipment[i], 1);
                }
                for (i = 0; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
                    g_world_unit_pointers[unit_id]->equipment[i] = equipment[i];
                    item_adjust_inventory_count(equipment[i], -1);
                }
                world_formation_save_records_to_party();
                world_menu_run_unit_status_banner_countdown(0xD811, 0x1E);
            }
        }
    }
    return 1;
}
