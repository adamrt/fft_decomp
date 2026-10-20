/* Adding to an item's held count, never going below zero. */

#include "fft/item.h"
#include "psx/types.h"

/* Add `delta` to the held count of an item (the low 10 bits of `item_entry`),
 * not going below 0, and return the new count. Item id 0 is ignored and
 * returns 0. */
s32 item_inventory_count_adjust(s16 item_entry, s32 delta) {
    s32 item_id = item_entry & 0x3FF;
    s32 new_count;

    if (item_id == 0) {
        return 0;
    }
    new_count = g_item_quantities[item_id] + delta;
    if (new_count < 0) {
        new_count = 0;
    }
    g_item_quantities[item_id] = new_count;
    return new_count;
}
