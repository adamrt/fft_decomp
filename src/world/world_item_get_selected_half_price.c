#include "fft/world.h"

s32 world_item_get_selected_half_price(void) {
    s32 result;

    if (g_world_menu_option_count == 0) {
        result = 0;
    } else {
        result = world_item_get_half_price(g_world_menu_entry_ids[g_world_menu_cursor_position]);
    }
    return result;
}
