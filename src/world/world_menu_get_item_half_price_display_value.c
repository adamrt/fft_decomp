#include "fft/world.h"

/* Half the menu item's price, with bit 30 copied from its 0x4000 entry flag. */
/* Target 0x801379cc. */
s32 world_menu_get_item_half_price_display_value(s32 entry_index) {
    s32 display_value = world_item_get_price(g_item_list_entries[entry_index]) >> 1;

    if ((g_item_list_entries[entry_index] & WORLD_ITEM_LIST_ENTRY_UNEQUIPPABLE) != 0)
        display_value |= MENU_NUMBER_VALUE_DISABLED;
    return display_value;
}
