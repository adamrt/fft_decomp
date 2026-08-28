#include "fft/battle_ai.h"

/*
 * Count candidate tiles within the map bounds on both levels.
 *
 * Each level has 18 halfword rows, with bit 15 representing x = 0.
 */
s32 battle_ai_count_flagged_tiles_within_map_bounds(u16* rows) {
    battle_ai_data_t* ai;
    s32 count;
    s32 level;
    s32 y;
    s32 x;
    u16 row;

    count = 0;
    ai = &g_battle_ai_data_base;
    level = 0;
    do {
        for (y = 0; y < ai->map_max_y; y++) {
            row = ((u16(*)[18])rows)[level][y];
            if (row != 0) {
                for (x = 0; x < ai->map_max_x; x++) {
                    if (((row << x) & 0x8000) != 0) {
                        count++;
                    }
                }
            }
        }
        level++;
    } while (level < 2);
    return count;
}
