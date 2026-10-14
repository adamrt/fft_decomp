#include "fft/battle.h"
#include "fft/battle_target.h"
#include "fft/menu.h"

/* Start the hovered-unit stats panel for the unit on the cursor tile, or the tile
 * info panel when the tile is empty. */
void target_store_cursor_unit_name_and_data(void) {
    battle_unit_t* unit;
    s32 mode;
    s32 unit_id;

    unit = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    if (unit != 0) {
        mode = UNIT_VIEW_MODE_HOVERED_UNIT;
        unit_id = unit->stats->battle_id;
    } else {
        mode = UNIT_VIEW_MODE_TILE_INFO;
        unit_id = 0xFF;
    }
    menu_start_unit_view_thread(mode, unit_id, 0xFF);
}
