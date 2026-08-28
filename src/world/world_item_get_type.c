#include "fft/data.h"
#include "psx/types.h"

extern item_data_t g_main_item_primary_data[];

u8 world_item_get_type(s32 item_id) {
    return g_main_item_primary_data[item_id & WORLD_ITEM_ID_MASK].type;
}
