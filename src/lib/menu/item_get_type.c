/* An item's type from the item data table. */

#include "fft/item.h"
#include "psx/types.h"

/* Return an item's type from the item data table; only the low ten bits of item_id are used. */
s32 item_get_type(s32 item_id) {
    return g_item_primary_data[item_id & 0x3ff].type;
}
