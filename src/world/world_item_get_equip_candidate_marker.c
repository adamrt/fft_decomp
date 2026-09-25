#include "fft/world.h"
#include "psx/types.h"

/* Equip-menu marker for the selected item: 8 when the unit already wears it
 * or cannot equip it, -1 when it is a fresh equippable choice. */
s32 world_item_get_equip_candidate_marker(s32 unit_index) {
    world_formation_unit_t* unit = g_world_formation_unit_pointers[unit_index];
    s32 result;
    s32 i;

    for (i = 0; i < 5; i++) {
        if (unit->equipment[i] == g_world_shop_equip_candidate_item) {
            return 8;
        }
    }
    result = world_unit_can_equip_item_id((s16)unit_index, g_world_shop_equip_candidate_item);
    if (result != 1) {
        return 8;
    }
    return -1;
}
