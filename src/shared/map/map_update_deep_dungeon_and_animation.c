#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "psx/types.h"

/* Per-frame body of the two Deep Dungeon mesh game states, which run while the
 * map reloads for a new crystal count: handle the free cursor, update the menus
 * and advance the map load. Once the load has finished, refresh the display of
 * unit units 0-15, clear g_map_deep_dungeon_load_in_progress and return 1;
 * until then return 0. */
s32 map_update_deep_dungeon_and_animation(void) {
    s32 runtime_id;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    if (map_poll_data_load() != 0) {
        return 0;
    }
    for (runtime_id = 0; runtime_id < BATTLE_UNIT_RUNTIME_SLOT_COUNT; runtime_id++) {
        battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xFFFF);
        if (unit != 0) {
            battle_unit_update_display_by_runtime_id(unit->runtime_id);
        }
    }
    g_map_deep_dungeon_load_in_progress = 0;
    return 1;
}

/* padding */
