#include "fft/battle.h"

/* A tile is 28 units wide, so max * 14 is the middle of the map. */
#define HALF_TILE_UNITS 14

/* Writes the map's centre as an X/Z/Y word triple with Z (height) zero
 * (effect camera target). */
void battle_effect_store_map_center_coordinates(VECTOR* destination) {
    SVECTOR map_max;
    s32 max_y;

    battle_map_store_max_coordinates(&map_max);
    destination->vx = map_max.vx * HALF_TILE_UNITS;
    max_y = map_max.vz;
    destination->vy = 0;
    destination->vz = max_y * HALF_TILE_UNITS;
}
