#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Remove the range from every panel, on both levels, within `range` tiles
 * (Manhattan distance) of (x, y). */
void target_remove_close_range(s32 x, s32 y, s32 range) {
    s32 i;
    s32 j;
    s32 dy;
    s32 dx;
    s32 k;

    for (i = 0; i < g_map_max_y; i++) {
        dy = (y < i) ? (i - y) : (y - i);
        for (j = 0; j < g_map_max_x; j++) {
            dx = (x < j) ? (j - x) : (x - j);
            if (range >= (dy + dx)) {
                g_target_panel_data[(i * g_map_max_x) + j].remaining_range = 0;
                k = j + 0x100;
                g_target_panel_data[(i * g_map_max_x) + k].remaining_range = 0;
            }
        }
    }
}
