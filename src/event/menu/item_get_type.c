#include "fft/main.h"
#include "psx/types.h"

s32 item_get_type(s32 item_id) {
    return g_main_item_primary_data[item_id & 0x3ff].type;
}
