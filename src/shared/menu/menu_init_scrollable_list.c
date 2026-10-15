#include "fft/event_bunit.h"
#include "fft/menu_script.h"
#include "psx/types.h"

/* Point the scrollable list menu at a new -1 terminated entry list with the
 * given cursor row and scroll base. */
void menu_init_scrollable_list(s16* entries, s32 selected_index, s32 scroll_base_index, u8* text_table) {
    menu_init_scrollable_list_core(entries, selected_index, text_table);
    g_menu_scroll_base_index = scroll_base_index;
}
