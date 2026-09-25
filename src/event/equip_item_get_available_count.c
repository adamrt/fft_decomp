#include "fft/event_equip.h"
#include "psx/types.h"

/* EQUIP.OUT 001c8188 - Number of copies of item id `item` still unequipped:
 * total owned (g_equip_item_inventory_totals) minus currently equipped (0-delta trick). */
s32 equip_item_get_available_count(s32 g_main_item_item_flags) {
    s32 item_id;

    item_id = g_main_item_item_flags & 0x3FF;
    return g_equip_item_inventory_totals[item_id] - equip_item_adjust_inventory_count(item_id, 0);
}
