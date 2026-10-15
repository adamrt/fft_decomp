#include "fft/world.h"

/* Query the selected poached-stock entry with a 0 quantity adjustment.
 *
 * An empty menu returns 0. */
s32 world_shop_get_selected_poached_item_quantity(s32 unused_row) {
    if (g_world_menu_option_count != 0)
        return world_shop_adjust_poached_item_quantity(g_item_list_entries[g_world_menu_cursor_position], 0);
    return 0;
}
