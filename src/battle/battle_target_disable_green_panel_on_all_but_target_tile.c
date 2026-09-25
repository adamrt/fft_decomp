#include "fft/battle.h"
#include "psx/types.h"

/* Leave only the action's target tile marked as targeted. Returns 1, or -1
 * when that tile is off the map or blocked. */
s32 battle_target_disable_green_panel_on_all_but_target_tile(const u8* action) {
    u8 action_copy[20];
    u8 x;
    u8 y;
    u8 level;
    map_tile_t* tile;

    main_util_copy_action_data(action, action_copy);
    x = action_copy[0xC];
    y = action_copy[0x10];
    level = action_copy[0xE];
    if (x < g_map_max_x) {
        if (y < g_map_max_y) {
            if (level < 2) {
                tile = &g_battle_map_tile_data[(level << 8) + y * g_map_max_x + x];
                if ((tile->flags_06.value & MAP_TILE_FLAG_BLOCKED) == 0) {
                    battle_target_disable_green_panel_flags();
                    tile->ceiling_depth_and_marks |= MAP_TILE_FLAG_TARGETED;
                    return 1;
                }
            }
        }
    }
    return -1;
}
