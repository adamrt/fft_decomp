#include "fft/battle.h"
#include "psx/types.h"

/* Store the deployment position and return its placement validation result.
 *
 * The caller at 0x8017f48c tests the returned value after this function's
 * final call to battle_unit_validate_placement. */
s32 battle_unit_set_placement_and_validate(s32 unit_idx, battle_deployed_coords_t* src) {
    battle_stats_t* unit = &g_battle_unit_stats[unit_idx];
    u16* position = &unit->position.raw;
    u32 higher_elevation;
    u32 facing;

    higher_elevation = src->facing_elevation_flags;
    higher_elevation = (higher_elevation >> 7) << 15;
    *position = (*position & 0x7FFF) | higher_elevation;
    *(u8*)position = src->y;
    unit->x = src->x;
    facing = (src->facing_elevation_flags & 0xF) << 8;
    *position = (*position & 0xF0FF) | facing;
    return battle_unit_validate_placement(unit_idx);
}
