#include "fft/data.h"
#include "psx/types.h"

/* Count copies of an item equipped across the active formation records.
 *
 * The comparison deliberately uses the stored equipment halfword as-is;
 * only the requested ID is masked. The signed record count is deliberately
 * reloaded for each unit to preserve the target access pattern. */
s32 world_count_item_equipped_by_party(s32 item_id) {
    s32 count;
    s32 unit_index;
    s32 slot;
    s32 record_count;
    world_formation_unit_t** unit_pointer;
    world_formation_unit_t* unit;
    volatile s32 stack_pad; /* Preserves the target's otherwise-unused 8-byte leaf frame. */

    unit_index = 0;
    count = 0;
    item_id &= WORLD_ITEM_ID_MASK;
    if (g_world_formation_record_count > 0) {
        unit_pointer = g_world_formation_unit_pointers;
        do {
            slot = 0;
            unit = *unit_pointer;
            do {
                if (unit->equipment[slot] == item_id) {
                    count++;
                }
                slot++;
            } while (slot < 5);
            /* Reloads the count each pass instead of hoisting it out of the loop. */
            __asm__("" : : : "memory");
            record_count = g_world_formation_record_count;
            /* Keeps the reload ahead of unit_index++, which fills its load delay. */
            __asm__("" : : "r"(record_count));
            unit_index++;
            unit_pointer++;
        } while (unit_index < record_count);
    }
    return count;
}
