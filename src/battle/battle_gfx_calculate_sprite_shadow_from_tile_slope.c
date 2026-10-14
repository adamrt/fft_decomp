#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "psx/libgte.h"
#include "psx/types.h"

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
