#include "fft/battle.h"
#include "psx/types.h"

void battle_map_store_max_coordinates(void* destination) {
    main_util_set_svector((SVECTOR*)destination, g_map_max_x, 0, g_map_max_y);
}
