/* The number of copies of an item the party has equipped. */

#include "fft/event_equip.h"
#include "fft/item.h"
#include "psx/types.h"

/* Number of copies of the item the party has equipped: the total held
 * (g_equip_item_inventory_totals) minus the inventory count (read with a zero delta). */
s32 item_get_equipped_count(s32 item_entry) {
    s32 item_id = item_entry & 0x3FF;

    return g_equip_item_inventory_totals[item_id] - item_adjust_inventory_count(item_id, 0);
}
