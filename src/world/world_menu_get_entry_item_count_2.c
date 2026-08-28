#include "fft/world.h"

s32 world_menu_get_entry_item_count_2(s32 entry_index) {
    s32 result;

    result = world_count_item_equipped_by_party(*(u16*)&g_world_menu_entry_ids[entry_index] & WORLD_ITEM_ID_MASK);
    if (*(u16*)&g_world_menu_entry_ids[entry_index] & 0x4000) {
        result |= 0x40000000;
    }
    return result;
}
