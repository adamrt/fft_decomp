#include "fft/world.h"
#include "psx/types.h"

/*
 * Change an ability slot and optionally replace incompatible equipment.
 *
 * Equipment changes return old items to inventory before reserving the
 * replacement set. Return 1 on success and -2 when the unit cannot change.
 */
s32 world_formation_set_unit_ability_slot(s16 unit_id, s16 slot, s16 ability, s32 update_equipment) {
    s16 equipment[WORLD_EQUIPMENT_SLOT_COUNT];
    s32 i;
    if (world_formation_get_unit_lock(unit_id) == WORLD_FORMATION_UNIT_UNLOCKED) {
        world_unit_t* unit = g_world_unit_pointers[unit_id];
        unit->abilities.slots[slot] = ability;
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
    return -2;
}
