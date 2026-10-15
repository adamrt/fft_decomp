#include "fft/world.h"

/* Inventory quantity plus the copies equipped by the party. */
s32 item_get_total_count_variant(s16 item_id) {
    item_id &= WORLD_ITEM_ID_MASK;
    return item_adjust_inventory_count(item_id, 0) + item_get_equipped_count_variant(item_id);
}
