#include "psx/types.h"

void equip_menu_init_and_draw_scrollable_list(
    const s16* entries, s32 selected_index, s32 scroll_base_index, const void* entry_data, const u8* commands) {
    equip_menu_init_scrollable_list(entries, selected_index, scroll_base_index, entry_data);
    equip_menu_draw_scrollable_list(commands);
}
