#include "fft/battle_map.h"

/* Write the map's width and depth in tiles to `destination` as (x, 0, z). */
void map_store_max_coordinates(SVECTOR* destination) {
    main_util_set_svector(destination, g_map_max_x, 0, g_map_max_y);
}
