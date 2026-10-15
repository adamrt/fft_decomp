#include "fft/battle_unit.h"
#include "psx/types.h"

/* Store the deployment position and return its placement validation result.
 *
 * The caller at 0x8017f48c tests the returned value after this function's
 * final call to battle_unit_validate_placement. */
s32 battle_unit_set_placement_and_validate(s32 unit_idx, unit_deployment_coordinates_t* src) {
    unit_stats_t* unit = &g_battle_unit_stats[unit_idx];

    unit->position.higher_elevation = src->flags.upper_level;
    unit->position.y = src->y;
    unit->x = src->x;
    unit->position.facing = src->flags.facing;
    return battle_unit_validate_placement(unit_idx);
}
