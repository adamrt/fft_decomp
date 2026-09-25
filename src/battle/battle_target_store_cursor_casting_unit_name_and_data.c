#include "fft/battle.h"
#include "psx/types.h"

void battle_target_store_cursor_casting_unit_name_and_data(void) {
    battle_unit_misc_data_t* target;
    battle_unit_misc_data_t* source;

    target
        = battle_unit_get_selectable_misc_data_at_map_coords(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    source = battle_unit_get_source_misc_data();
    if (target != 0) {
        battle_menu_store_unit_names_and_event_block_data(
            5, source->battle_data->misc_unit_id, target->battle_data->misc_unit_id);
    } else {
        battle_menu_store_unit_names_and_event_block_data(2, source->battle_data->misc_unit_id, 0xFF);
    }
}
