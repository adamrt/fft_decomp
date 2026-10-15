#include "fft/event_equip.h"

/* Get the equipped count of the item in an equipment-menu row.
 *
 * MENU_NUMBER_VALUE_DISABLED is set when the unit cannot equip the item. */
s32 menu_get_entry_item_equipped_count(s32 row) {
    u16 entry = g_item_list_entries[row];
    s32 result = item_get_equipped_count_variant(entry & EQUIP_ITEM_LIST_ENTRY_ITEM_ID_MASK);
    if (g_item_list_entries[row] & EQUIP_ITEM_LIST_ENTRY_UNEQUIPPABLE) {
        result |= MENU_NUMBER_VALUE_DISABLED;
    }
    return result;
}
