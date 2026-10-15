#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Mark the horizontal and vertical targeting runs on both map layers.
 *
 * The target reads the first layer's availability byte unsigned and marks
 * both layers when that byte is nonzero. Sharing the loop index and panel
 * temporaries across the sweeps preserves the target register allocation. */
void target_apply_vertical_fixed(s32 x, s32 y) {
    s32 shared;
    s32 vertical;
    s32 tile_index;
    target_panel_t* panel;
    s32 i;

    x &= 0xFF;
    y &= 0xFF;
    {
        vertical = y;
        for (i = -0x1F; i < 0x20; i++) {
            s32 mx;
            target_panel_t* base = g_target_panel_data;
            target_panel_t* lvl1 = base + 0x100;
            shared = x + i;
            mx = g_map_max_x;
            tile_index = (vertical * mx) + shared;
            panel = &base[tile_index];
            if ((shared >= 0) && (shared < mx) && (panel->remaining_range != 0)) {
                panel->mark = 1;
                lvl1[tile_index].mark = 1;
            }
        }
    }
    {
        shared = x;
        for (i = -0x1F; i < 0x20; i++) {
            target_panel_t* base = g_target_panel_data;
            target_panel_t* lvl1 = base + 0x100;
            vertical = y + i;
            tile_index = (vertical * g_map_max_x) + shared;
            panel = &base[tile_index];
            if ((vertical >= 0) && (vertical < g_map_max_y) && (panel->remaining_range != 0)) {
                panel->mark = 1;
                lvl1[tile_index].mark = 1;
            }
        }
    }
}
