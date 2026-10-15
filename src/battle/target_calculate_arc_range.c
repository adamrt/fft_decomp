#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/*
 * Mark the longbow (arc) targeting panels: every tile at least three steps
 * away whose distance, less a quarter of the unit's height advantage, is
 * within range. Both elevation levels of each tile are tested.
 */
void target_calculate_arc_range(const unit_stats_t* unit, u8 range, u8 flags) {
    u8 height;
    u8 tile_height;
    s32 ux;
    s32 uy;
    s32 x;
    s32 y;
    s32 dy;
    s32 dist;
    s32 level;
    s32 index;
    map_tile_t* tile;

    height = battle_unit_get_effective_height(unit);
    ux = unit->x;
    uy = unit->position.y;
    for (y = 0; y < g_map_max_y; y++) {
        if (uy < y) {
            dy = y - uy;
        } else {
            dy = uy - y;
        }
        for (x = 0; x < g_map_max_x; x++) {
            if (ux < x) {
                dist = x - ux + dy;
            } else {
                dist = ux - x + dy;
            }
            if (dist >= 3) {
                for (level = 0; level < 2; level++) {
                    index = (level << 8) + y * g_map_max_x + x;
                    tile = &g_map_tile_data[index];
                    tile_height
                        = tile->height * 2 + tile->depth_half_height.half_height + tile->depth_half_height.depth * 2;
                    if (dist - (height - tile_height) / 4 <= range) {
                        g_target_panels[index].remaining_range = 1;
                    }
                }
            }
        }
    }
}
