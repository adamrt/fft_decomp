#include "fft/battle.h"
#include "fft/world.h"
#include "psx/types.h"

/* Target 0x800f2ee0; byte-identical twin of battle_map_store_selected_tile_data. */
void world_map_store_selected_tile_data(void) {
    s16 coords[3];
    s16* selected;
    s32 depth;
    s32 surface_offset;

    battle_map_store_selected_tile_coordinates(coords);
    selected = &g_world_map_selected_tile_data.surface_type;
    *selected = battle_map_get_tile_data_value(MAP_TILE_DATA_SURFACE_TYPE, coords[0], coords[2], coords[1]) & 0xFF;
    g_world_map_selected_tile_data.display_height
        = battle_map_get_tile_data_value(MAP_TILE_DATA_HALF_HEIGHT, coords[0], coords[2], coords[1]) & 0xFF;
    g_world_map_selected_tile_data.display_height
        += (battle_map_get_tile_data_value(MAP_TILE_DATA_HEIGHT, coords[0], coords[2], coords[1]) & 0xFF) * 2;
    depth = battle_map_get_tile_data_value(MAP_TILE_DATA_DEPTH, coords[0], coords[2], coords[1]) & 0xFF;
    g_world_map_selected_tile_data.depth = depth;
    g_world_map_selected_tile_data.display_height += depth * 2;
    surface_offset = *selected * 2;
    g_world_map_selected_tile_data._unknown_02 = D_8005E950[surface_offset];
    g_world_map_selected_tile_data._unknown_04 = D_8005E950[surface_offset + 1];
}
