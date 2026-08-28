#include "fft/data.h"
#include "fft/world.h"

extern item_data_t g_main_item_primary_data[];

s32 world_item_get_price(s32 item_id) {
    return g_main_item_primary_data[item_id & WORLD_ITEM_ID_MASK].price;
}
