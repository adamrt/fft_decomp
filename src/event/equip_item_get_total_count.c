#include "fft/equip.h"

s32 equip_item_get_total_count(s32 item_id) {
    return g_equip_item_inventory_totals[item_id & 0x3FF];
}
