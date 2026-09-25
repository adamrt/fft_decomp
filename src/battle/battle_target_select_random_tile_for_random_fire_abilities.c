#include "fft/battle.h"
#include "psx/types.h"

/*
 * Pick a random tile among those whose MAP_TILE_FLAG_TARGETED is set,
 * clearing the flag on every candidate and setting it again on the chosen
 * tile, then store that tile as the current action target.
 *
 * The candidate list reuses the targeting panels' mark byte for the tile
 * index, so upper-level candidates are recognised by their list position.
 */
void battle_target_select_random_tile_for_random_fire_abilities(void) {
    s32 count;
    s32 lower_count;
    s32 i;
    s32 index;
    s32 tile;
    u8 flags;
    map_tile_t* tile_data;
    battle_target_panel_t* panel;

    count = 0;
    for (i = 0; i < 0x200; i++) {
        tile_data = &g_battle_map_tile_data[i];
        if (i == 0x100) {
            lower_count = count;
        }
        flags = tile_data->ceiling_depth_and_marks;
        if (flags & MAP_TILE_FLAG_TARGETED) {
            tile_data->ceiling_depth_and_marks = flags & ~MAP_TILE_FLAG_TARGETED;
            panel = &g_battle_target_panels[count];
            count++;
            panel->mark = i;
        }
    }
    index = rand() * count / 32768;
    tile = g_battle_target_panels[index].mark;
    if (index >= lower_count) {
        tile += 0x100;
    }
    g_battle_map_tile_data[tile].ceiling_depth_and_marks |= MAP_TILE_FLAG_TARGETED;
    g_current_ability.random_fire_flag = 1;
    g_current_ability.target_elevation = tile / 256;
    g_current_ability.target_y = (tile & 0xFF) / g_map_max_x;
    g_current_ability.target_x = (tile & 0xFF) % g_map_max_x;
    battle_target_clear_panel_spread_flags();
}
