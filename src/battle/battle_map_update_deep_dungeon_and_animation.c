#include "fft/battle.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

/* Per-frame tick while the Deep Dungeon overworld map is up: poll the
   cursor, render the menu, drive the map-load state machine, then when
   the map is fully loaded refresh every live unit's display and clear
   the load-in-progress flag (g_battle_map_deep_dungeon_load_in_progress). */
s32 battle_map_update_deep_dungeon_and_animation(void) {
    s32 misc_id;

    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    if (battle_map_load_data_stage_0x76() != 0) {
        return 0;
    }
    misc_id = 0;
    do {
        battle_unit_misc_data_t* misc = battle_unit_get_misc_data_by_misc_id(misc_id & 0xFFFF);
        misc_id++;
        if (misc != 0) {
            battle_unit_update_display_by_misc_id(misc->unit_id);
        }
    } while (misc_id < 0x10);
    g_battle_map_deep_dungeon_load_in_progress = 0;
    return 1;
}

/* padding */
