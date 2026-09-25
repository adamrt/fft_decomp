#include "fft/battle.h"
#include "psx/types.h"

enum {
    TILE_CEILING_NONE = 0x7f,
    TILE_ELEVATION_STRIDE = 0x100,
};

/*
 * Returns the lowest ceiling above (x, y) across the two elevation layers, or
 * 0x7f when no tile above exit_height forms one.  exit_slope_shift selects the
 * exit side's two slope bits; exit_height is the exit side height (or tile
 * height plus unit size).
 */
u8 battle_move_calculate_tile_ceiling(s32 x, s32 y, s32 exit_slope_shift, s32 exit_height) {
    s32 row_base;
    s32 elevation;
    map_tile_t* tile;
    s32 ceiling;
    s32 tile_ceiling;

    row_base = y * g_battle_move_config_ptr->map_max_x;
    ceiling = TILE_CEILING_NONE;
    for (elevation = 0; elevation < 2; elevation++) {
        tile = &g_battle_map_tile_data[row_base + x];
        /* Keeps the (u8)exit_height mask in the loop; loop.c otherwise hoists it. */
        __asm__("" : "=r"(exit_height) : "0"(exit_height));
        if ((tile->flags_06.value & MAP_TILE_FLAG_BLOCKED) == 0) {
            tile_ceiling = (tile->height << 1)
                + (tile->depth_half_height & MAP_TILE_HALF_HEIGHT_MASK) * ((tile->slope_type >> exit_slope_shift) & 3)
                - ((tile->ceiling_depth_and_marks & MAP_TILE_CEILING_DEPTH_MASK) << 1);
            if ((u8)tile_ceiling > (u8)exit_height) {
                if ((u8)ceiling < (u8)tile_ceiling) {
                    tile_ceiling = ceiling;
                }
                ceiling = tile_ceiling;
            }
        }
        row_base += TILE_ELEVATION_STRIDE;
    }
    return (u8)ceiling;
}
