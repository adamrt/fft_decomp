#include "fft/battle.h"
#include "psx/types.h"

s32 battle_action_get_elemental_ability_id(battle_stats_t* unit) {
    return g_geomancy_terrain_ability_table[g_battle_map_tile_data[battle_map_calculate_location(unit)].surface.value
        & MAP_SURFACE_MASK];
}
