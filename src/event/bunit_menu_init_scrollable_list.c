#include "fft/event_bunit.h"
#include "psx/types.h"

void bunit_menu_init_scrollable_list(s32 entries, s32 selected_index, s16 scroll_base_index, s32 text_table) {
    bunit_menu_init_scrollable_list_core((s16*)entries, selected_index, text_table);
    g_bunit_menu_scroll_base_index = scroll_base_index;
}
