#include "fft/data.h"
#include "fft/world.h"

extern item_data_t g_main_item_primary_data[];

/* Return half of the item-table price, clamped to at least 1. */
s32 world_item_get_half_price(s32 item_id) {
    u32 value = g_main_item_primary_data[item_id & WORLD_ITEM_ID_MASK].price >> 1;

    if (value == 0) {
        value = 1;
    }
    return value;
}
