#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Mark the panels along a straight line from (x, y) in one compass direction
 * (TARGET_LINE_*), the starting tile included.
 *
 * On both map levels a target_panel_t with remaining_range gets mark set;
 * the table is g_target_panels. The walk stops
 * at the map edge or after 32 tiles, and any other direction value marks nothing.
 * `dir` is reused as the panel index. */
void target_calculate_linear_attack_tiles(s32 dir, s32 x, s32 y) {
    s32 i;
    s32 dx;
    s32 dy;
    s32 map_width;
    target_panel_t* panel;

    switch (dir) {
    case TARGET_LINE_EAST:
        dx = 1;
        dy = 0;
        break;
    case TARGET_LINE_WEST:
        dx = -1;
        dy = 0;
        break;
    case TARGET_LINE_NORTH:
        dx = 0;
        dy = 1;
        break;
    case TARGET_LINE_SOUTH:
        dx = 0;
        dy = -1;
        break;
    default:
        return;
    }

    i = 0;
    do {
        if (x < 0) {
            return;
        }
        map_width = g_map_max_x;
        if (x >= map_width) {
            return;
        }
        if (y < 0) {
            return;
        }
        if (y >= g_map_max_y) {
            return;
        }
        dir = y * map_width + x;
        panel = &g_target_panel_data[dir];
        if (panel->remaining_range != 0) {
            panel->mark = 1;
        }
        dir += 0x100;
        panel = &g_target_panel_data[dir];
        y += dy;
        if (panel->remaining_range != 0) {
            panel->mark = 1;
        }
        i += 1;
        x += dx;
    } while (i < 0x20);
}
