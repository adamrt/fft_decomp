#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/map.h"

/* A tile is 28 units wide, so max * 14 is the middle of the map. */
/* Writes the map's center as three 32-bit X/Z/Y components with Z (height) zero
 * (effect camera target). */
void effect_store_map_center_coordinates(VECTOR* destination) {
    SVECTOR map_max;
    s32 max_y;

    map_store_max_coordinates(&map_max);
    destination->vx = map_max.vx * (MAP_TILE_SIZE / 2);
    max_y = map_max.vz;
    destination->vy = 0;
    destination->vz = max_y * (MAP_TILE_SIZE / 2);
}
