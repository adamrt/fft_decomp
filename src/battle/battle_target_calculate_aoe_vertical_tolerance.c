#include "fft/battle.h"
#include "psx/types.h"

void battle_target_calculate_aoe_vertical_tolerance(s32 x, s32 y, s32 lo, s32 hi) {
    s32 level;
    s32 idx;
    s32 height;
    map_tile_t* tile;
    targeting_panel_entry_t* panel;
    u32 flags;

    if ((x >= 0) && (x < g_map_max_x) && (y >= 0) && (y < g_map_max_y)) {
        level = 0;
        if (lo < 0) {
            lo = 0;
        }
        do {
            idx = (level << 8) + (y * g_map_max_x) + x;
            tile = &g_battle_map_tile_data[idx];
            panel = &g_battle_target_panel_data[idx];
            flags = tile->depth_half_height;
            height = (tile->height * 2) + (flags & MAP_TILE_HALF_HEIGHT_MASK) + ((flags >> MAP_TILE_DEPTH_SHIFT) * 2);
            level++;
            if ((height >= lo) && (hi >= height)) {
                panel->a = 1;
            }
        } while (level < 2);
    }
}
