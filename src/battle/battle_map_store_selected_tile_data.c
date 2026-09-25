#include "fft/battle.h"
#include "psx/types.h"

void battle_map_store_selected_tile_data(void) {
    s16 coords[3];
    s16* surface_type;
    s32 depth;
    s32 table_index;

    battle_map_store_selected_tile_coordinates(coords);
    surface_type = &g_battle_map_selected_tile_data.surface_type;
    *surface_type = battle_map_get_tile_data_value(MAP_TILE_DATA_SURFACE_TYPE, coords[0], coords[2], coords[1]) & 0xFF;
    g_battle_map_selected_tile_data.display_height
        = battle_map_get_tile_data_value(MAP_TILE_DATA_HALF_HEIGHT, coords[0], coords[2], coords[1]) & 0xFF;
    g_battle_map_selected_tile_data.display_height
        += (battle_map_get_tile_data_value(MAP_TILE_DATA_HEIGHT, coords[0], coords[2], coords[1]) & 0xFF) * 2;
    depth = battle_map_get_tile_data_value(MAP_TILE_DATA_DEPTH, coords[0], coords[2], coords[1]) & 0xFF;
    g_battle_map_selected_tile_data.depth = depth;
    g_battle_map_selected_tile_data.display_height += depth * 2;
    table_index = *surface_type * 2;
    g_battle_map_selected_tile_data._unknown_02 = D_8005E950[table_index];
    g_battle_map_selected_tile_data._unknown_04 = D_8005E950[table_index + 1];
}
