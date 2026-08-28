#include "fft/battle.h"
#include "psx/types.h"

/* Mark the horizontal and vertical targeting runs on both map layers.
 *
 * The target reads the first layer's availability byte unsigned and marks
 * both layers when that byte is nonzero. Sharing the loop index and panel
 * temporaries across the sweeps preserves the target register allocation. */
void battle_target_apply_vertical_fixed(s32 x, s32 y) {
    s32 shared;
    s32 vertical;
    s32 tile_index;
    targeting_panel_entry_t* panel;
    s32 i;

    x &= 0xFF;
    y &= 0xFF;
    {
        vertical = y;
        for (i = -0x1F; i < 0x20; i++) {
            s32 mx;
            targeting_panel_entry_t* base = g_battle_target_panel_data;
            targeting_panel_entry_t* lvl1 = base + 0x100;
            shared = x + i;
            mx = g_map_max_x;
            tile_index = (vertical * mx) + shared;
            panel = &base[tile_index];
            if ((shared >= 0) && (shared < mx) && ((u8)panel->a != 0)) {
                panel->b = 1;
                lvl1[tile_index].b = 1;
            }
        }
    }
    {
        shared = x;
        for (i = -0x1F; i < 0x20; i++) {
            targeting_panel_entry_t* base = g_battle_target_panel_data;
            targeting_panel_entry_t* lvl1 = base + 0x100;
            vertical = y + i;
            tile_index = (vertical * g_map_max_x) + shared;
            panel = &base[tile_index];
            if ((vertical >= 0) && (vertical < g_map_max_y) && ((u8)panel->a != 0)) {
                panel->b = 1;
                lvl1[tile_index].b = 1;
            }
        }
    }
}
