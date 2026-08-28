#include "fft/equip.h"

s32 equip_unit_can_equip_item(s16 unit_index, s32 g_main_item_item_flags) {
    s32 item_id = g_main_item_item_flags & 0x3FF;
    s32 result;

    if (item_id == 0) {
        return 1;
    }
    if (equip_item_get_category(item_id) == ITEM_MENU_CATEGORY_OTHER) {
        return -1;
    }
    equip_bits_init_primary_reader((u8*)g_equip_unit_data[unit_index] + 0x70);
    equip_bits_read_primary(equip_item_get_type(item_id));
    if (equip_bits_read_primary(1) == 0) {
        result = -1;
    } else {
        result = 1;
    }
    return result;
}
