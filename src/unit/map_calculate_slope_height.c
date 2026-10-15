#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Surface height for a slope rising (UP) or falling (DOWN) across the tile. */
#define SLOPE_PRODUCT(f) ((f) * (half * MAP_HEIGHT_STEP))
#define SLOPE_UP(f)      (base - SLOPE_PRODUCT(f) / MAP_TILE_SIZE)
#define SLOPE_DOWN(f)    (base - (half * MAP_HEIGHT_STEP - SLOPE_PRODUCT(f) / MAP_TILE_SIZE))

/*
 * Return the negated terrain height, in world units, under a point.
 *
 * coords->vx and coords->vz are the X and Z positions (tile index * 28 + offset). A tile without a
 * slope half-height, or with an unrecognized slope type, yields
 * -(height * 12); the recognized types interpolate half * 12 across the tile
 * along X, Z, or whichever of the two the corner type selects. x and z are
 * the offsets of the point inside its 28-unit tile.
 */
s32 map_calculate_slope_height(const SVECTOR* coords, s32 map_z) {
    s16 tile_pos[2];
    map_tile_t* tile;
    s32 half;
    s32 base;
    s16 result;
    s16 x;
    s16 z;
    s32 lhs;
    s32 rhs;

    tile_pos[0] = coords->vx / MAP_TILE_SIZE;
    tile_pos[1] = coords->vz / MAP_TILE_SIZE;
    tile = map_get_tile_data_pointer(tile_pos[0], tile_pos[1], (u8)map_z);
    base = -(tile->height * MAP_HEIGHT_STEP);
    half = tile->depth_half_height.half_height;
    result = base;
    if (half != 0) {
        switch (tile->slope_type) {
        case MAP_SLOPE_EDGE_EAST:
            x = coords->vx % MAP_TILE_SIZE;
            result = SLOPE_UP(x);
            break;
        case MAP_SLOPE_EDGE_WEST:
            x = coords->vx % MAP_TILE_SIZE;
            result = SLOPE_DOWN(x);
            break;
        case MAP_SLOPE_EDGE_SOUTH:
            z = coords->vz % MAP_TILE_SIZE;
            result = SLOPE_DOWN(z);
            break;
        case MAP_SLOPE_EDGE_NORTH:
            z = coords->vz % MAP_TILE_SIZE;
            result = SLOPE_UP(z);
            break;
        case MAP_SLOPE_LOW_CORNER_NORTH_EAST:
            x = coords->vx % MAP_TILE_SIZE;
            z = coords->vz % MAP_TILE_SIZE;
            lhs = x;
            rhs = z;
            if (lhs < rhs) {
                result = SLOPE_UP(lhs);
            } else {
                result = SLOPE_UP(rhs);
            }
            break;
        case MAP_SLOPE_LOW_CORNER_SOUTH_EAST:
            x = coords->vx % MAP_TILE_SIZE;
            z = coords->vz % MAP_TILE_SIZE;
            lhs = x;
            rhs = MAP_TILE_SIZE - z;
            if (lhs >= rhs) {
                result = SLOPE_DOWN(z);
            } else {
                result = SLOPE_UP(lhs);
            }
            break;
        case MAP_SLOPE_LOW_CORNER_SOUTH_WEST:
            x = coords->vx % MAP_TILE_SIZE;
            z = coords->vz % MAP_TILE_SIZE;
            lhs = MAP_TILE_SIZE - x;
            rhs = MAP_TILE_SIZE - z;
            if (lhs >= rhs) {
                result = SLOPE_DOWN(z);
            } else {
                result = SLOPE_DOWN(x);
            }
            break;
        case MAP_SLOPE_LOW_CORNER_NORTH_WEST:
            x = coords->vx % MAP_TILE_SIZE;
            z = coords->vz % MAP_TILE_SIZE;
            lhs = MAP_TILE_SIZE - x;
            rhs = z;
            if (lhs >= rhs) {
                result = SLOPE_UP(rhs);
            } else {
                result = SLOPE_DOWN(x);
            }
            break;
        case MAP_SLOPE_HIGH_CORNER_NORTH_EAST:
            x = coords->vx % MAP_TILE_SIZE;
            z = coords->vz % MAP_TILE_SIZE;
            lhs = x;
            rhs = z;
            if (lhs >= rhs) {
                result = SLOPE_UP(lhs);
            } else {
                result = SLOPE_UP(rhs);
            }
            break;
        case MAP_SLOPE_HIGH_CORNER_SOUTH_EAST:
            x = coords->vx % MAP_TILE_SIZE;
            z = coords->vz % MAP_TILE_SIZE;
            lhs = x;
            rhs = MAP_TILE_SIZE - z;
            if (lhs >= rhs) {
                result = SLOPE_UP(lhs);
            } else {
                result = SLOPE_DOWN(z);
            }
            break;
        case MAP_SLOPE_HIGH_CORNER_SOUTH_WEST:
            x = coords->vx % MAP_TILE_SIZE;
            z = coords->vz % MAP_TILE_SIZE;
            lhs = MAP_TILE_SIZE - x;
            rhs = MAP_TILE_SIZE - z;
            if (lhs < rhs) {
                result = SLOPE_DOWN(z);
            } else {
                result = SLOPE_DOWN(x);
            }
            break;
        case MAP_SLOPE_HIGH_CORNER_NORTH_WEST:
            x = coords->vx % MAP_TILE_SIZE;
            z = coords->vz % MAP_TILE_SIZE;
            lhs = MAP_TILE_SIZE - x;
            rhs = z;
            if (lhs >= rhs) {
                result = SLOPE_DOWN(x);
            } else {
                result = SLOPE_UP(rhs);
            }
            break;
        }
    }
    return result;
}
