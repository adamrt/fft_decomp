#include "fft/event_equip.h"

/* Whether a unit's job can equip an item: EQUIP_SLOT_ITEM_ALLOWED for no item or an item whose
 * type bit is set in the unit's equippable-types bitfield, EQUIP_SLOT_ITEM_CANNOT_EQUIP for an item
 * of the "other" menu category or a type the unit cannot use. The low ten
 * bits of the entry are the item id. */
s32 equip_unit_can_equip_item_id(s16 unit_index, s16 item_entry) {
    s32 item_id = item_entry & 0x3FF;
    s32 result;

    if (item_id == 0) {
        return EQUIP_SLOT_ITEM_ALLOWED;
    }
    if (item_get_menu_category(item_id) == ITEM_MENU_CATEGORY_OTHER) {
        return EQUIP_SLOT_ITEM_CANNOT_EQUIP;
    }
    equip_bits_init_primary_reader(g_equip_unit_data[unit_index]->equipment_categories);
    bits_read_primary(equip_item_get_type(item_id));
    if (bits_read_primary(1) == 0) {
        result = EQUIP_SLOT_ITEM_CANNOT_EQUIP;
    } else {
        result = EQUIP_SLOT_ITEM_ALLOWED;
    }
    return result;
}
