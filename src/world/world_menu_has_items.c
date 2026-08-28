#include "fft/data.h"
#include "fft/main_runtime.h"
#include "fft/world.h"

/* Test for inventory or equipment entries in the current menu.
 *
 * Mode 0 combines both sources, mode 1 selects inventory, and other modes select equipment.
 */
s32 world_menu_has_items(s32 mode) {
    s32 has_inventory;
    s32 has_equipment;
    s32 i;
    s32 slot;

    has_inventory = 0;
    has_equipment = 0;
    for (i = 1; i < 254; i++) {
        if (g_main_item_quantities[i]) {
            has_inventory = 1;
            break;
        }
    }
    for (i = 0; i < g_world_formation_record_count; i++) {
        for (slot = 0; slot < 5; slot++) {
            if (g_world_formation_unit_pointers[i]->equipment[slot]) {
                has_equipment = 1;
                break;
            }
        }
    }
    if (mode == 0) {
        return has_equipment || has_inventory;
    }
    if (mode == 1) {
        return has_inventory;
    }
    return has_equipment;
}
