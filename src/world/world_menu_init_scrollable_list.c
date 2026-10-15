#include "fft/world.h"

/* Point the scrollable list menu at a new -1 terminated entry list with the
 * given cursor row and scroll base. */
void world_menu_init_scrollable_list(const s16* values, s32 value, s32 selected, u8* text_table) {
    world_menu_init_scrollable_list_core(values, value, text_table);
    g_menu_scroll_base_index = selected;
}
