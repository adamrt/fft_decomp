#include "fft/world.h"
#include "psx/types.h"

u8 world_item_get_type(s32 item_id) {
    return g_main_item_primary_data[item_id & WORLD_ITEM_ID_MASK].type;
}
