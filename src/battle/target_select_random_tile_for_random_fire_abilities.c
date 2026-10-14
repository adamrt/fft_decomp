#include "fft/battle_action.h"
#include "fft/battle_map.h"
#include "fft/battle_target.h"
#include "psx/types.h"

/*
 * Pick a random tile among those whose MAP_TILE_FLAG_TARGETED is set,
 * clearing the flag on every candidate and setting it again on the chosen
 * tile, then store that tile as the current action target.
 *
 * The candidate list reuses the targeting panels' mark byte for the tile
 * index, so upper-level candidates are recognized by their list position.
 */
void target_select_random_tile_for_random_fire_abilities(void) {
    s32 count;
    s32 lower_count;
    s32 i;
    s32 index;
    s32 tile;
    map_tile_marks_t marks;
    map_tile_t* tile_data;
    target_panel_t* panel;

    count = 0;
    for (i = 0; i < 0x200; i++) {
        tile_data = &g_map_tile_data[i];
        if (i == 0x100) {
            lower_count = count;
        }
        marks = tile_data->ceiling_depth_and_marks;
        if (marks.targeted) {
            tile_data->ceiling_depth_and_marks.targeted = 0;
            panel = &g_target_panels[count];
            count++;
            panel->mark = i;
        }
    }
    index = rand() * count / 32768;
    tile = g_target_panels[index].mark;
    if (index >= lower_count) {
        tile += 0x100;
    }
    g_map_tile_data[tile].ceiling_depth_and_marks.targeted = 1;
    g_action_current_ability.random_fire_flag = 1;
    g_action_current_ability.target_new_elevation = tile / 256;
    g_action_current_ability.target_new_y = (tile & 0xFF) / g_map_max_x;
    g_action_current_ability.target_new_x = (tile & 0xFF) % g_map_max_x;
    target_clear_panel_spread_flags();
}
