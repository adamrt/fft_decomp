#include "fft/battle.h"

/*
 * Remove a randomly selected tile from a candidate mask.
 *
 * The supplied count determines the random ordinal. Return 0 when it is 0
 * or no corresponding set bit is found; otherwise write coordinates and return 1.
 */
s32 battle_ai_take_random_tile(u16* tiles, battle_ai_coords_t* coordinates, s32 count) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 selected;
    s32 level, y, x;
    u16 row;

    if (count == 0) {
        return 0;
    }
    selected = rand() % count;
    /* Each level has 18 rows; inline row views preserve the target addressing. */
    for (level = 0; level < 2; level++) {
        for (y = 0; y < ai->map_max_y; y++) {
            row = ((u16(*)[18])tiles)[level][y];
            if (row != 0) {
                for (x = 0; x < ai->map_max_x; x++) {
                    if ((row << x) & 0x8000) {
                        if (selected == 0) {
                            coordinates->bytes.x = x;
                            coordinates->bytes.y = y;
                            coordinates->bytes.elevation = level;
                            coordinates->bytes.zero = 0;
                            ((u16(*)[18])tiles)[level][y] ^= 0x8000 >> x;
                            return 1;
                        }
                        selected--;
                    }
                }
            }
        }
    }
    return 0;
}
