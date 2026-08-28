#include "fft/battle_ai.h"
#include "fft/map.h"
#include "psx/types.h"

/*
 * Mark the longbow (arc) targeting panels: every tile at least three steps
 * away whose distance, less a quarter of the unit's height advantage, is
 * within range. Both elevation levels of each tile are tested.
 */
void battle_target_calculate_arc_range(battle_stats_t* unit, u8 range, u8 flags) {
    u8 height;
    s32 ux;
    s32 uy;
    s32 x;
    s32 y;
    s32 dy;
    s32 dx;
    s32 dist;
    s32 level;
    s32 index;
    map_tile_t* tile;

    height = battle_unit_get_effective_height(unit);
    ux = unit->x;
    uy = unit->position.bits.y;
    for (y = 0; y < g_map_max_y; y++) {
        if (uy < y) {
            dy = y - uy;
        } else {
            dy = uy - y;
            /* Zero-instruction use of dy: without it ux and dy swap $t3/$t4. */
            __asm__("" : : "r"(dy));
        }
        for (x = 0; x < g_map_max_x; x++) {
            if (ux < x) {
                dx = x - ux;
            } else {
                dx = ux - x;
            }
            dist = dx + dy;
            if (dist >= 3) {
                for (level = 0; level < 2; level++) {
                    index = (level << 8) + y * g_map_max_x + x;
                    tile = &g_battle_map_tile_data[index];
                    if (dist
                            - (height
                                  - (u8)(tile->height * 2 + (tile->depth_half_height & MAP_TILE_HALF_HEIGHT_MASK)
                                      + (tile->depth_half_height >> MAP_TILE_DEPTH_SHIFT) * 2))
                                / 4
                        <= range) {
                        g_battle_target_panels[index].remaining_range = 1;
                    }
                }
            }
        }
    }
}
