#include "fft/world.h"

/* Returns the owned-and-equipped count for the selected item. */
s32 world_item_count_selected_owned_and_equipped(void) {
    s32 result;

    if (g_world_menu_option_count != 0) {
        result = world_item_count_owned_and_equipped(
            *(u16*)&g_world_menu_entry_ids[g_world_menu_cursor_position] & WORLD_ITEM_ID_MASK);
    } else {
        result = 0;
    }
    return result;
}
