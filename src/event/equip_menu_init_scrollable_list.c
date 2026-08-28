#include "fft/equip.h"
#include "psx/types.h"

void equip_menu_init_scrollable_list(
    const s16* entries, s32 selected_index, s32 scroll_base_index, const void* entry_data) {
    equip_menu_init_scrollable_list_core((s16*)entries, selected_index, entry_data);
    g_equip_menu_scroll_base_index = scroll_base_index;
}
