#include "fft/battle_move.h"

s32 battle_move_validate_float_fly(battle_unit_misc_data_t* unit) {
    map_tile_t* tile;
    s32 status;
    u32 flags;

    tile = battle_map_get_tile_data_pointer(unit->screen.vx / 28, unit->screen.vz / 28, unit->map_z);
    if (unit->battle_data != 0) {
        status = battle_move_get_effective_flags(unit->battle_data);
        if (status & BATTLE_EFFECTIVE_MOVEMENT_ON_WATER) {
            return 0;
        }
        flags = unit->status_flags_5_6;
        if (flags & BATTLE_MISC_STATUS_FLOAT) {
            return 0;
        }
        if (flags & (BATTLE_MISC_STATUS_CHICKEN | BATTLE_MISC_STATUS_FROG | BATTLE_MISC_STATUS_TREASURE)) {
            return 1;
        }
        if (status & BATTLE_EFFECTIVE_MOVEMENT_IN_WATER) {
            return 1;
        }
        if (flags & BATTLE_MISC_STATUS_CRYSTAL) {
            return 0;
        }
    }
    return tile->depth_half_height >> MAP_TILE_DEPTH_SHIFT;
}
