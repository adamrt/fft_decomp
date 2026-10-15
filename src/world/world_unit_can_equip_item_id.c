#include "fft/world.h"

/* Whether a unit's job can equip an item: 1 for no item or an item whose
 * type bit is set in the unit's equippable-types bitfield, -1 for an item
 * of the "other" menu category or a type the unit cannot use. The low ten
 * bits of the entry are the item id. */
s32 world_unit_can_equip_item_id(s16 unit_index, s16 item_id) {
    s32 id = item_id & WORLD_ITEM_ID_MASK;
    s32 result;

    if (id == ITEM_ID_NOTHING) {
        return 1;
    }
    if (item_get_menu_category(id) == ITEM_MENU_CATEGORY_OTHER) {
        return -1;
    }
    world_bit_cursor_set_primary(g_world_unit_pointers[unit_index]->equippable_item_types);
    bits_read_primary(world_item_get_type(id));
    if (bits_read_primary(1) == 0) {
        result = -1;
    } else {
        result = 1;
    }
    return result;
}
