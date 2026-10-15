#include "fft/event_bunit.h"

/* Build the unit-list selection-to-record index map.
 *
 * Linked entries encode a roster ID in their formation-order key; ordinary
 * entries retain their compact order among directly reorderable units.
 */
void bunit_unit_build_list_index_map(void) {
    s32 reorderable_index = 0;
    s32 unit_index;

    for (unit_index = 0; unit_index < g_bunit_unit_list_count; unit_index++) {
        bunit_unit_t* unit = g_bunit_unit_pointers[unit_index];

        if (!(unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT)) {
            g_bunit_unit_list_index_map[unit_index] = reorderable_index;
            reorderable_index++;
        } else {
            g_bunit_unit_list_index_map[unit_index]
                = bunit_unit_find_index_by_battle_id((unit->mount_info - BATTLE_MOUNT_INFO_FLAG_MOUNT));
        }
    }
}
