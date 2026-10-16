#include "fft/event_equip.h"
#include "psx/types.h"

s32 item_get_total_count_variant(s16 item_id) {
    return g_equip_item_inventory_totals[item_id & 0x3FF];
}
