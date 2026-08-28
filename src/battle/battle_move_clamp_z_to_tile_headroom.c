#include "fft/battle.h"
#include "fft/map.h"
#include "psx/types.h"

/*
 * Clamps a camera/height value into the walkable band above one tile: the
 * lower bound subtracts the layer step clearance from battle_move_calculate_tile_layer_step_offset, the
 * upper bound is the tile surface height alone less 0x30.
 */
s32 battle_move_clamp_z_to_tile_headroom(s32 value, s32 x, s32 y, u32 layer) {
    map_tile_t* tile;
    s32 clearance;
    s32 height;
    s32 lo;
    s32 hi;
    u32 sub;

    tile = battle_map_get_tile_data_pointer(x, y, layer);
    clearance = battle_move_calculate_tile_layer_step_offset(x, y, layer);
    sub = tile->depth_half_height;
    height = ((tile->height + (sub >> MAP_TILE_DEPTH_SHIFT)) * 2) + (sub & MAP_TILE_HALF_HEIGHT_MASK);
    lo = -(height + clearance) * 6;
    if (value < lo) {
        return lo;
    }
    hi = (-height * 6) - 0x30;
    if (value >= hi) {
        if (hi < lo) {
            return lo;
        }
        return hi;
    }
    return value;
}
