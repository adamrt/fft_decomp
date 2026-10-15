#include "fft/world.h"
#include "psx/types.h"

/* Equip-menu marker for the selected item: 8 when the unit already wears it
 * or cannot equip it, -1 when it is a fresh equippable choice. */
s32 world_item_get_equip_candidate_marker(s32 unit_index) {
    world_unit_t* unit = g_world_unit_pointers[unit_index];
    s32 i;

    for (i = 0; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
        if (unit->equipment[i] == g_world_shop_equip_candidate_item) {
            return MENU_UNIT_READOUT_EQUIP_LABEL;
        }
    }
    if (world_unit_can_equip_item_id(unit_index, g_world_shop_equip_candidate_item) != 1) {
        return MENU_UNIT_READOUT_EQUIP_LABEL;
    }
    return -1;
}
