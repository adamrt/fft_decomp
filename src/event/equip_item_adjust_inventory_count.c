#include "fft/event_equip.h"
#include "psx/types.h"

/* EQUIP.OUT 001c684c - Add `delta` to the inventory count for the item id (low
 * 10 bits of `item`); clamped to 0. item_t id 0 is a no-op sentinel. */
s32 equip_item_adjust_inventory_count(s32 g_main_item_item_flags, s32 delta) {
    s32 item_id;
    s32 new_count;

    item_id = g_main_item_item_flags & 0x3FF;
    if (item_id == 0) {
        return 0;
    }
    new_count = g_main_item_quantities[item_id] + delta;
    if (new_count < 0) {
        new_count = 0;
    }
    g_main_item_quantities[item_id] = new_count;
    return new_count;
}
