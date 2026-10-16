/* The number of copies of an item the party has in all. */

#include "fft/event_equip.h"
#include "psx/types.h"

/* Return how many of an item the party has in all, from EQUIP's inventory totals table; only the
 * low ten bits of item_id are used. */
s32 item_get_total_count_variant(s16 item_id) {
    return g_equip_item_inventory_totals[item_id & 0x3FF];
}
