#include "fft/equip.h"
#include "psx/types.h"

void equip_menu_set_selection_record(s32 index, s32 selected_index, s32 scroll_index, u16* items) {
    g_equip_menu_selection_entries[index].selected_index = selected_index;
    g_equip_menu_selection_entries[index].scroll_index = scroll_index;
    g_equip_menu_selection_entries[index].item_id = items[selected_index] & 0x3FF;
}
