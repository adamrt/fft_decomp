#include "fft/world.h"

s32 world_unit_can_equip_item_id(s16 unit_index, s32 item_id) {
    s32 id = item_id & WORLD_ITEM_ID_MASK;
    s32 result;

    if (id == ITEM_ID_NOTHING) {
        return 1;
    }
    if (world_item_get_menu_category(id) == ITEM_MENU_CATEGORY_OTHER) {
        return -1;
    }
    world_bit_cursor_set_primary(g_world_formation_unit_pointers[unit_index]->equippable_item_types);
    /* The target uses v0 without the u8 return's mask. */
    world_bit_cursor_read_primary(((s32 (*)(s32))world_item_get_type)(id));
    if (world_bit_cursor_read_primary(1) == 0) {
        result = -1;
    } else {
        result = 1;
    }
    return result;
}
