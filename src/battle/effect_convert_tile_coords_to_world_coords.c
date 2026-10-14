#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/map.h"

/* Convert a tile position (x, layer, y) to world coordinates: the tile center (28
 * units per tile) at the tile's height, 12 units per level and negative upward.
 * The slope and the water depth are not counted. */
void effect_convert_tile_coords_to_world_coords(SVECTOR* in, VECTOR* out) {
    map_tile_t* tile;

    tile = map_get_tile_data_pointer(in->vx, in->vz, in->vy);
    out->vx = in->vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    out->vz = in->vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    out->vy = -(tile->height * 12);
}
