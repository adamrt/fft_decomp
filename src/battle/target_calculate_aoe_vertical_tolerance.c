#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Give range 1 to the panels of map position (x, y), on either level, whose
 * height lies in lo..hi. Heights are in half steps (height * 2 + slope half
 * height + depth * 2); a negative lo counts as 0 and a position off the map is
 * ignored. The strike and lunge weapon ranges are built from it. */
void target_calculate_aoe_vertical_tolerance(s32 x, s32 y, s32 lo, s32 hi) {
    s32 level;
    s32 idx;
    s32 height;
    map_tile_t* tile;
    target_panel_t* panel;
    map_tile_depth_t flags;

    if ((x >= 0) && (x < g_map_max_x) && (y >= 0) && (y < g_map_max_y)) {
        level = 0;
        if (lo < 0) {
            lo = 0;
        }
        do {
            idx = (level << 8) + (y * g_map_max_x) + x;
            tile = &g_map_tile_data[idx];
            panel = &g_target_panel_data[idx];
            flags = tile->depth_half_height;
            height = (tile->height * 2) + (flags.half_height) + ((flags.depth) * 2);
            level++;
            if ((height >= lo) && (hi >= height)) {
                panel->remaining_range = 1;
            }
        } while (level < 2);
    }
}
