#include "fft/battle_map.h"
#include "psx/types.h"

/* Load a terrain chunk (MAP_RESOURCE_TERRAIN): the map's width and depth in
 * tiles, then the 0x1000 bytes of the two 256-tile layers. */
/* A typed terrain pointer or local cursor removes four retail bytes. */
void map_copy_xy_coords_and_tile_data(u8* data) {
    g_map_max_x = *data++;
    g_map_max_y = *data++;
    memcpy(g_map_tile_data, data, 0x1000);
}
