#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_move.h"
#include "fft/battle_unit.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

s32 move_calculate_water_lift(battle_unit_t* unit, map_tile_t* tile) {
    s32 movement_flags;

    if (unit->stats != 0) {
        movement_flags = move_get_effective_flags(unit->stats);
    } else {
        movement_flags = 0;
    }
    if (tile->depth_half_height.depth) {
        if ((movement_flags & BATTLE_EFFECTIVE_MOVEMENT_FLOAT)
            || (movement_flags & BATTLE_EFFECTIVE_MOVEMENT_ON_WATER)) {
            return tile->depth_half_height.depth * 2;
        }
        if (movement_flags & BATTLE_EFFECTIVE_MOVEMENT_IN_WATER) {
            return (tile->depth_half_height.depth - 1) * 2;
        }
    }
    return 0;
}
