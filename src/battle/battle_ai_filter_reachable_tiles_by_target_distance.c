#include "fft/battle.h"

/*
 * Build a reachable-tile mask within the target-distance limit.
 *
 * Filter scenario 0 into the selected scenario, accepting distances equal
 * to the limit. Return 1 if any tile qualifies; otherwise copy all scenario
 * 0 rows and return 0. The destination is cleared before reading the source,
 * so callers select scenario 1 to preserve the scenario 0 mask.
 */
s32 battle_ai_filter_reachable_tiles_by_target_distance(s32 distance_limit) {
    s32 g_main_item_move_find_flags = 0;
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 x, y, level;
    u16 row;

    for (x = 0; x < 18; x++) {
        ai->reachable_tiles[ai->movement_scenario][0][x] = 0;
        ai->reachable_tiles[ai->movement_scenario][1][x] = 0;
    }
    for (level = 0; level < 2; level++) {
        for (y = 0; y < ai->map_max_y; y++) {
            row = ai->reachable_tiles[0][level][y];
            if (row != 0) {
                for (x = 0; x < ai->map_max_x; x++) {
                    if (((row << x) & 0x8000) && distance_limit >= ai->tile_target_distance[level][y][x]) {
                        ai->reachable_tiles[ai->movement_scenario][level][y] |= 0x8000 >> x;
                        g_main_item_move_find_flags = 1;
                    }
                }
            }
        }
    }
    if (g_main_item_move_find_flags == 0) {
        battle_ai_transfer_halfword_values((u16*)ai->reachable_tiles[ai->movement_scenario],
            (u16*)ai->reachable_tiles[0], sizeof(ai->reachable_tiles[0]));
        return 0;
    }
    return 1;
}
