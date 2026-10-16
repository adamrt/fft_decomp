/* The number of copies of an item the party has in all. */

#include "fft/data_item.h"
#include "fft/event_equip.h"
#include "psx/types.h"

/* Return how many of an item the party has in all, from EQUIP's inventory totals table; only the
 * low ten bits of item_id are used. */
s32 item_get_total_count(s16 item_id) {
    return g_equipment_item_inventory_totals[item_id & ITEM_ENTRY_ID_MASK];
}
