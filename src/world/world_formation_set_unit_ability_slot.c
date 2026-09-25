#include "fft/data.h"
#include "fft/job.h"
#include "fft/world.h"
#include "psx/types.h"

/*
 * Change an ability slot and optionally replace incompatible equipment.
 *
 * Equipment changes return old items to inventory before reserving the
 * replacement set. Return 1 on success and -2 when the unit cannot change.
 */
s32 world_formation_set_unit_ability_slot(s16 unit_id, s16 slot, s16 ability, s32 update_equipment) {
    u16 equipment[5];
    s32 i;
    if (world_formation_lock_equipment_and_abilities(unit_id) == 1) {
        world_formation_unit_ability_slots_t* unit
            = (world_formation_unit_ability_slots_t*)g_world_formation_unit_pointers[unit_id];
        unit->ability_slots[slot] = ability;
        world_formation_save_records_to_party_data();
        world_formation_remove_invalid_unit_loadout(unit_id);
        if (update_equipment != 0) {
            if (unit_id != 20) {
                if (g_world_formation_unit_pointers[unit_id]->job_id != JOB_ID_MIME) {
                    world_unit_build_best_fit_equipment(unit_id, (s16*)equipment);
                    for (i = 0; i < 5; i++) {
                        world_item_change_quantity_on_equip(
                            (s16)g_world_formation_unit_pointers[unit_id]->equipment[i], 1);
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
    return -2;
}
