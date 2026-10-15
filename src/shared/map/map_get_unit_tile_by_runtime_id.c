#include "fft/battle_map.h"
#include "psx/types.h"

/* The tile a unit stands on, by runtime id. The runtime record lookup is not checked:
 * an id without one is read through address 0. */
map_tile_t* map_get_unit_tile_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    return map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z);
}
