#include "fft/world.h"

/* Menu callback: half price of the highlighted item, or 0 for an empty list.
 * The row argument is not read. */
s32 world_item_get_selected_half_price(s32 unused_row) {
    s32 result;

    if (g_world_menu_option_count == 0) {
        result = 0;
    } else {
        result = world_item_get_half_price(g_item_list_entries[g_world_menu_cursor_position]);
    }
    return result;
}
