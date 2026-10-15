/* Drawn position, shadow quad, cursor tile polygon and slope height. */

#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* The unit's drawn position: its integer world position plus the event and SEQ offsets,
 * with the Float bob added to the height. */
void battle_unit_copy_draw_coords(SVECTOR* destination, const battle_unit_t* unit) {
    /* float_bob_offset is read signed (lh) here. */
    main_util_set_svector(destination, unit->world_coords.vx + unit->event_offset.vx + unit->seq_offset.vx,
        unit->world_coords.vy + unit->event_offset.vy + unit->seq_offset.vy + (s16)unit->float_bob_offset,
        unit->world_coords.vz + unit->event_offset.vz + unit->seq_offset.vz);
}

/* Lays the unit's shadow quad on the terrain surface under the unit.
 *
 * The quad is a 20 x 20 square of world units centerd on the unit's position
 * (with its event, SEQ and float offsets), at the surface of the tile it stands
 * on: height plus depth, 12 world units per level, negative y upwards. On a
 * sloped tile (non-zero slope half-height, rising 12 units per step across the
 * 28-unit tile) the quad follows the slope at the unit's offset inside the tile:
 *
 * - an edge slope interpolates along its axis and tilts the quad by 6 units up
 *   and down across the two edges, whatever the steepness;
 * - a corner slope picks the triangle the unit stands on, by comparing its
 *   offsets from the two sides, and puts the flat quad at that triangle's height
 *   under the unit.
 *
 * battle_gfx_draw_unit_shadow calls it before projecting the quad. */
void battle_gfx_calculate_sprite_shadow_from_tile_slope(battle_unit_t* unit) {
    SVECTOR coords;
    SVECTOR unused; /* unreferenced; keeps the 0x30-byte target frame */
    map_tile_t* tile;
    SVECTOR* quad;
    u8 layer;
    u8 height;
    map_tile_depth_t depth;
    u8 slope_type;
    s32 slope;
    s16 tx;
    s16 ty;
    s32 lhs;
    s32 rhs;
    s32 rise;

    layer = unit->map_z;
    battle_unit_copy_draw_coords(&coords, unit);
    tile = map_get_tile_data_pointer((coords.vx / MAP_TILE_SIZE), (coords.vz / MAP_TILE_SIZE), layer);
    depth = tile->depth_half_height;
    height = tile->height + depth.depth;
    slope_type = tile->slope_type;
    quad = unit->shadow_quad;
    quad[0].vx = coords.vx - 10;
    quad[0].vz = coords.vz + 10;
    quad[1].vx = coords.vx + 10;
    quad[1].vz = coords.vz + 10;
    quad[2].vx = coords.vx - 10;
    quad[2].vz = coords.vz - 10;
    quad[3].vx = coords.vx + 10;
    quad[3].vz = coords.vz - 10;
    quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = -(height * 12);
    slope = depth.half_height;
    if (slope == 0) {
        return;
    }
    switch (slope_type) {
    case MAP_SLOPE_EDGE_EAST: /* edge up to east */
        tx = coords.vx % MAP_TILE_SIZE;
        rise = tx * 12;
        quad[0].vy = quad[1].vy = quad[3].vy = quad[2].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        quad[0].vy = quad[2].vy = quad[2].vy + 6;
        quad[1].vy = quad[3].vy = quad[3].vy - 6;
        break;
    case MAP_SLOPE_EDGE_WEST: /* edge up to west */
        tx = coords.vx % MAP_TILE_SIZE;
        rise = tx * 12;
        quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - (slope * 12 - rise * slope / MAP_TILE_SIZE);
        quad[1].vy = quad[3].vy = quad[3].vy + 6;
        quad[0].vy = quad[2].vy = quad[2].vy - 6;
        break;
    case MAP_SLOPE_EDGE_SOUTH: /* edge up to south */
        ty = coords.vz % MAP_TILE_SIZE;
        rise = ty * 12;
        quad[0].vy = quad[2].vy = quad[3].vy = quad[1].vy = quad[3].vy - (slope * 12 - rise * slope / MAP_TILE_SIZE);
        quad[0].vy = quad[1].vy = quad[1].vy + 6;
        quad[2].vy = quad[3].vy = quad[3].vy - 6;
        break;
    case MAP_SLOPE_EDGE_NORTH: /* edge up to north */
        ty = coords.vz % MAP_TILE_SIZE;
        rise = ty * 12;
        quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        quad[2].vy = quad[3].vy = quad[3].vy + 6;
        quad[0].vy = quad[1].vy = quad[1].vy - 6;
        break;
    case MAP_SLOPE_LOW_CORNER_NORTH_EAST: /* low corner, north-east up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = tx;
        rhs = ty;
        if (lhs < rhs) {
            rise = lhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        } else {
            rise = rhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        }
        break;
    case MAP_SLOPE_LOW_CORNER_SOUTH_EAST: /* low corner, south-east up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = tx;
        rhs = MAP_TILE_SIZE - ty;
        if (lhs >= rhs) {
            rise = ty * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * 12 - rise * slope / MAP_TILE_SIZE);
        } else {
            rise = lhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        }
        break;
    case MAP_SLOPE_LOW_CORNER_SOUTH_WEST: /* low corner, south-west up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = MAP_TILE_SIZE - tx;
        rhs = MAP_TILE_SIZE - ty;
        if (lhs >= rhs) {
            rise = ty * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * 12 - rise * slope / MAP_TILE_SIZE);
        } else {
            rise = tx * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * 12 - rise * slope / MAP_TILE_SIZE);
        }
        break;
    case MAP_SLOPE_LOW_CORNER_NORTH_WEST: /* low corner, north-west up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = MAP_TILE_SIZE - tx;
        rhs = ty;
        if (lhs >= rhs) {
            rise = rhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        } else {
            rise = tx * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * 12 - rise * slope / MAP_TILE_SIZE);
        }
        break;
    case MAP_SLOPE_HIGH_CORNER_NORTH_EAST: /* high corner, north-east up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = tx;
        rhs = ty;
        if (lhs >= rhs) {
            rise = lhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        } else {
            rise = rhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        }
        break;
    case MAP_SLOPE_HIGH_CORNER_SOUTH_EAST: /* high corner, south-east up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = tx;
        rhs = MAP_TILE_SIZE - ty;
        if (lhs >= rhs) {
            rise = lhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        } else {
            rise = ty * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * 12 - rise * slope / MAP_TILE_SIZE);
        }
        break;
    case MAP_SLOPE_HIGH_CORNER_SOUTH_WEST: /* high corner, south-west up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = MAP_TILE_SIZE - tx;
        rhs = MAP_TILE_SIZE - ty;
        if (lhs < rhs) {
            rise = ty * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * 12 - rise * slope / MAP_TILE_SIZE);
        } else {
            rise = tx * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * 12 - rise * slope / MAP_TILE_SIZE);
        }
        break;
    case MAP_SLOPE_HIGH_CORNER_NORTH_WEST: /* high corner, north-west up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = MAP_TILE_SIZE - tx;
        rhs = ty;
        if (lhs >= rhs) {
            rise = tx * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * 12 - rise * slope / MAP_TILE_SIZE);
        } else {
            rise = rhs * 12;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        }
        break;
    }
}

/* Build the four corner vertices of the map tile under a cursor position.
 *
 * The corners sit 14 units either side of coords on X and Y. Their height is
 * the tile's surface (height plus depth, 12 units per step) raised by the slope
 * half-height, then each slope type raises or lowers individual corners by that
 * half-height. Returns how the quad is triangulated: 0 planar, 1 split along
 * corners 0-3, 2 split along corners 1-2.
 *
 * split is cleared before the tile lookup: living across the call keeps it in
 * a saved register, as in the target, although the scheduler emits the clear
 * after the call. Testing depth & MAP_TILE_HALF_HEIGHT_MASK directly (rather
 * than masking depth in place) keeps the height load ahead of the depth load.
 */
s32 target_calculate_cursor_tile_polygon(const SVECTOR* coords, u8 layer, SVECTOR* quad) {
    map_tile_t* tile;
    /* Byte access retains the original packed load and field-extraction order. */
    s32 split;
    s32 rise;
    u8 height;
    u8 depth;
    u8 slope;

    split = 0;
    tile = map_get_tile_data_pointer((coords->vx / MAP_TILE_SIZE), (coords->vz / MAP_TILE_SIZE), layer);
    height = tile->height;
    depth = (*(u8*)&tile->depth_half_height);
    slope = tile->slope_type;
    quad[0].vx = coords->vx - 14;
    quad[0].vz = coords->vz + 14;
    quad[1].vx = coords->vx + 14;
    quad[1].vz = coords->vz + 14;
    quad[2].vx = coords->vx - 14;
    quad[2].vz = coords->vz - 14;
    quad[3].vx = coords->vx + 14;
    quad[3].vz = coords->vz - 14;
    quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = -((height + (depth >> MAP_TILE_DEPTH_SHIFT)) * 12);
    if ((depth & MAP_TILE_HALF_HEIGHT_MASK) != 0) {
        rise = (depth & MAP_TILE_HALF_HEIGHT_MASK) * 6;
        quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise;
        switch (slope) {
        case MAP_SLOPE_EDGE_EAST: /* edge up to east */
            quad[0].vy = quad[2].vy = quad[2].vy + rise;
            quad[1].vy = quad[3].vy = quad[3].vy - rise;
            break;
        case MAP_SLOPE_EDGE_WEST: /* edge up to west */
            quad[1].vy = quad[3].vy = quad[3].vy + rise;
            quad[0].vy = quad[2].vy = quad[2].vy - rise;
            break;
        case MAP_SLOPE_EDGE_SOUTH: /* edge up to south */
            quad[0].vy = quad[1].vy = quad[1].vy + rise;
            quad[2].vy = quad[3].vy = quad[3].vy - rise;
            break;
        case MAP_SLOPE_EDGE_NORTH: /* edge up to north */
            quad[2].vy = quad[3].vy = quad[3].vy + rise;
            quad[0].vy = quad[1].vy = quad[1].vy - rise;
            break;
        case MAP_SLOPE_LOW_CORNER_NORTH_EAST: /* low corner, north-east up */
            split = 1;
            quad[0].vy = quad[2].vy = quad[3].vy = quad[3].vy + rise;
            quad[1].vy = quad[1].vy - rise;
            break;
        case MAP_SLOPE_LOW_CORNER_SOUTH_EAST: /* low corner, south-east up */
            split = 2;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[2].vy + rise;
            quad[3].vy = quad[3].vy - rise;
            break;
        case MAP_SLOPE_LOW_CORNER_SOUTH_WEST: /* low corner, south-west up */
            split = 1;
            quad[0].vy = quad[1].vy = quad[3].vy = quad[3].vy + rise;
            quad[2].vy = quad[2].vy - rise;
            break;
        case MAP_SLOPE_LOW_CORNER_NORTH_WEST: /* low corner, north-west up */
            split = 2;
            quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy + rise;
            quad[0].vy = quad[0].vy - rise;
            break;
        case MAP_SLOPE_HIGH_CORNER_NORTH_EAST: /* high corner, north-east up */
            split = 1;
            quad[0].vy = quad[1].vy = quad[3].vy = quad[3].vy - rise;
            quad[2].vy = quad[2].vy + rise;
            break;
        case MAP_SLOPE_HIGH_CORNER_SOUTH_EAST: /* high corner, south-east up */
            split = 2;
            quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise;
            quad[0].vy = quad[0].vy + rise;
            break;
        case MAP_SLOPE_HIGH_CORNER_SOUTH_WEST: /* high corner, south-west up */
            split = 1;
            quad[0].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise;
            quad[1].vy = quad[1].vy + rise;
            break;
        case MAP_SLOPE_HIGH_CORNER_NORTH_WEST: /* high corner, north-west up */
            split = 2;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[2].vy - rise;
            quad[3].vy = quad[3].vy + rise;
            break;
        }
    }
    return split;
}

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
