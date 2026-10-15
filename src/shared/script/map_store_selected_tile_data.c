#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/map.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/types.h"

/* Fill g_map_selected_tile_data for the tile under the cursor, which the
 * tile information display shows: the surface type, the depth, the displayed
 * height in half-height units (height * 2 + slope half-height + depth * 2) and
 * the surface type's two bytes of g_main_terrain_tile_info_pairs. */
void map_store_selected_tile_data(void) {
    SVECTOR coords;
    s16* surface_type;
    s32 depth;

    map_store_selected_tile_coordinates(&coords);
    surface_type = &g_map_selected_tile_data.surface_type;
    *surface_type = map_get_tile_data_value(MAP_TILE_DATA_SURFACE_TYPE, coords.vx, coords.vz, coords.vy) & 0xFF;
    g_map_selected_tile_data.display_height
        = map_get_tile_data_value(MAP_TILE_DATA_HALF_HEIGHT, coords.vx, coords.vz, coords.vy) & 0xFF;
    g_map_selected_tile_data.display_height
        += (map_get_tile_data_value(MAP_TILE_DATA_HEIGHT, coords.vx, coords.vz, coords.vy) & 0xFF) * 2;
    depth = map_get_tile_data_value(MAP_TILE_DATA_DEPTH, coords.vx, coords.vz, coords.vy) & 0xFF;
    g_map_selected_tile_data.depth = depth;
    g_map_selected_tile_data.display_height += depth * 2;
    g_map_selected_tile_data._unknown_02 = g_main_terrain_tile_info_pairs[*surface_type][0];
    g_map_selected_tile_data._unknown_04 = g_main_terrain_tile_info_pairs[*surface_type][1];
}
