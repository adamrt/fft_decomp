#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Spread one targeting panel's remaining range to its four orthogonal
 * neighbors. A neighbor whose residual budget is lower than this panel's is
 * refreshed to one less, on both map levels, and marked as a new frontier when
 * enough budget is left to keep spreading. Returns 1 when any neighbor was
 * marked. */
s32 target_spread_panel_to_neighbors(s32 y, s32 x) {
    target_panel_t* panel;
    s32 changed;
    s32 i;
    s32 dx;
    s32 dy;
    s32 next_x;
    s32 next_y;
    s32 index;
    s32 upper_x;
    s32 range;
    s32 spread_range;

    changed = 0;
    range = g_target_panels[y * g_map_max_x + x].remaining_range;
    spread_range = range - 1;
    for (i = 0; i < 4; i++) {
        switch (i) {
        case 0:
            dx = 1;
            dy = 0;
            break;
        case 1:
            dx = 0;
            dy = 1;
            break;
        case 2:
            dx = -1;
            dy = 0;
            break;
        case 3:
            dx = 0;
            dy = -1;
            break;
        }
        next_x = x + dx;
        next_y = y + dy;
        if ((next_x >= 0) && (next_y >= 0) && (next_x < g_map_max_x) && (next_y < g_map_max_y)) {
            index = next_y * g_map_max_x + next_x;
            panel = &g_target_panels[index];
            if (panel->remaining_range < range) {
                panel->remaining_range = spread_range;
                upper_x = next_x + 0x100;
                g_target_panels[next_y * g_map_max_x + upper_x].remaining_range = spread_range;
                if (range >= 3) {
                    panel->mark = 1;
                    changed = 1;
                }
            }
        }
    }
    return changed;
}
