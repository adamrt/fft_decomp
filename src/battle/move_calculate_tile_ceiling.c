#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/memory.h"
#include "psx/libetc.h"
#include "psx/types.h"

enum {
    TILE_CEILING_NONE = 0x7f,
};

/*
 * Returns the lowest ceiling above (x, y) across the two elevation layers, or
 * 0x7f when no tile above exit_height forms one.  exit_slope_shift selects the
 * exit side's two slope bits; exit_height is the exit side height (or tile
 * height plus unit size).
 */
u8 move_calculate_tile_ceiling(s32 x, s32 y, s32 exit_slope_shift, u8 exit_height) {
    s32 elevation;
    map_tile_t* tile;
    map_tile_depth_t depth;
    u8 ceiling;
    u8 tile_ceiling;

    ceiling = TILE_CEILING_NONE;
    for (elevation = 0; elevation < MAP_TERRAIN_LAYER_COUNT; elevation++) {
        tile = &g_map_tile_data[elevation * MAP_TILES_PER_LAYER + y * g_move_config_ptr->map_max_x + x];
        if ((tile->flags.blocked) == 0) {
            depth = tile->depth_half_height;
            tile_ceiling = (tile->height << 1) + (depth.half_height) * ((tile->slope_type >> exit_slope_shift) & 3)
                - ((tile->ceiling_depth_and_marks.ceiling_depth) << 1);
            if (tile_ceiling > exit_height) {
                if (ceiling < tile_ceiling) {
                    tile_ceiling = ceiling;
                }
                ceiling = tile_ceiling;
            }
        }
    }
    return ceiling;
}
