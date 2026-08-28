#include "fft/world.h"

/* Full-price twin of world_menu_get_item_half_price_display_value: the menu
 * item's price with bit 30 copied from its 0x4000 entry flag. */
s32 world_menu_get_item_price_display_value(s32 entry_index) {
    s32 display_value = world_item_get_price(g_world_menu_entry_ids[entry_index]);

    if ((g_world_menu_entry_ids[entry_index] & 0x4000) != 0)
        display_value |= 0x40000000;
    return display_value;
}
