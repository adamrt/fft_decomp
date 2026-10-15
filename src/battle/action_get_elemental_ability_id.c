#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* The Elemental ability that the surface type of the unit's tile selects. */
s32 action_get_elemental_ability_id(const unit_stats_t* unit) {
    return g_main_terrain_geomancy_abilities[g_map_tile_data[map_calculate_location(unit)].surface.type];
}
