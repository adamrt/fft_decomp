#include "fft/world.h"
#include "psx/types.h"

/* Count copies of an item equipped across the active formation records.
 *
 * The comparison uses the stored 16-bit equipment value as-is; only the requested
 * ID is masked. */
s32 item_get_equipped_count_variant(s32 item_id) {
    s32 count;
    s32 unit_index;
    s32 slot;
    world_unit_t* unit;

    count = 0;
    item_id &= WORLD_ITEM_ID_MASK;
    for (unit_index = 0; unit_index < g_world_formation_record_count; unit_index++) {
        unit = g_world_unit_pointers[unit_index];
        for (slot = 0; slot < WORLD_EQUIPMENT_SLOT_COUNT; slot++) {
            if (unit->equipment[slot] == item_id) {
                count++;
            }
        }
        /* The target reloads the record count on every pass although nothing
         * in the loop stores; without this the load is hoisted. */
        __asm__("" : : : "memory");
    }
    return count;
}
