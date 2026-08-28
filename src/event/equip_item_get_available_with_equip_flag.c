#include "fft/equip.h"

/* Get the available inventory count for an equipment-menu row.
 *
 * The high result bit preserves whether the row represents an equipped item.
 */
s32 equip_item_get_available_with_equip_flag(s32 row) {
    u16 entry = ((u16*)g_equip_item_list_entries)[row];
    s32 result = equip_item_get_available_count(entry & EQUIP_ITEM_LIST_ENTRY_ITEM_ID_MASK);
    if (((u16*)g_equip_item_list_entries)[row] & EQUIP_ITEM_LIST_ENTRY_EQUIPPED) {
        result |= EQUIP_ITEM_COUNT_EQUIPPED_FLAG;
    }
    return result;
}
