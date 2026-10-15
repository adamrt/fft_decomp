#include "fft/world.h"

/* Returns the party-equipped count for the selected item. */
s32 world_item_count_selected_equipped_by_party(s32 unused_row) {
    s32 result;

    if (g_world_menu_option_count != 0) {
        result
            = item_get_equipped_count_variant(g_item_list_entries[g_world_menu_cursor_position] & WORLD_ITEM_ID_MASK);
    } else {
        result = 0;
    }
    return result;
}
