#include "fft/battle.h"

void battle_target_store_cursor_unit_as_preview_target(void) {
    battle_unit_misc_data_t* misc
        = battle_unit_get_selectable_misc_data_at_map_coords(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    battle_unit_get_source_misc_data();
    if (misc != 0) {
        battle_menu_store_unit_names_and_event_block_data(4, 0xFF, misc->battle_data->misc_unit_id);
    } else {
        battle_menu_store_unit_names_and_event_block_data(1, 0xFF, 0xFF);
    }
}
