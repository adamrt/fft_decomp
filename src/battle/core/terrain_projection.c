/* Place unit sprites, shadows and cursor geometry on the battle terrain. */

#include "fft/battle/camera.h"
#include "fft/battle/cursor.h"
#include "fft/battle/move.h"
#include "fft/game/battle_unit.h"
#include "fft/gfx/animation.h"
#include "fft/gfx/sprite.h"
#include "fft/gfx/sprite_sheet.h"
#include "fft/map/map.h"
#include "fft/map/map_unit.h"
#include "fft/sys/math.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Move a point half a tile (14 units) on x and z, with signs set by the camera yaw quadrant, back
 * by `offset` on both, and 12 units up: the depth point of a stepping unit. */
static void get_step_depth_point(const SVECTOR* input, SVECTOR* output, s32 offset) {
    u16 x;
    u16 z;

    output->vy = input->vy - 0xc;
    switch (g_camera_render_state.vy & 0xc00) {
    case 0:
        x = input->vx + 0xe;
        output->vx = x - offset;
        z = input->vz - 0xe;
        output->vz = z + offset;
        break;
    case 0x400:
        x = input->vx + 0xe;
        output->vx = x - offset;
        z = input->vz + 0xe;
        output->vz = z - offset;
        break;
    case 0x800:
        x = input->vx - 0xe;
        output->vx = x + offset;
        z = input->vz + 0xe;
        output->vz = z - offset;
        break;
    case 0xc00:
        x = input->vx - 0xe;
        output->vx = x + offset;
        z = input->vz - 0xe;
        output->vz = z + offset;
        break;
    }
}

/* get_step_depth_point with offset 1: the depth point of a stepping unit on a
 * tile without MAP_TILE_SURFACE_FLAG_DEPTH_BIAS. */
void camera_get_unbiased_step_depth_point(const SVECTOR* input, SVECTOR* output) {
    get_step_depth_point(input, output, 1);
}

/* get_step_depth_point with offset 5: the depth point of a stepping unit on a
 * tile with MAP_TILE_SURFACE_FLAG_DEPTH_BIAS. */
void camera_get_biased_step_depth_point(const SVECTOR* input, SVECTOR* output) {
    get_step_depth_point(input, output, 5);
}

/*
 * Snap an integer world-space point to the 28-unit tile grid for the current camera
 * yaw, writing the result to output.
 *
 * Same shape as get_step_depth_point: each of the x and z
 * components picks up the caller's offset with a sign that depends on the yaw
 * quadrant, and the y component is a plain bias. Here each component is first
 * snapped down to a whole tile, and on the side the camera faces away from it
 * is moved on by one further tile. Callers pass an offset of 1 or 5. The
 * far_edge temporaries keep the target's add-then-subtract order; written
 * inline, GCC re-associates them into a longer sequence.
 */
static void get_tile_depth_point(const SVECTOR* input, SVECTOR* output, s32 offset) {
    s32 yaw;

    output->vy = input->vy - 0xb;
    yaw = g_camera_render_state.vy & 0xc00;

    switch (yaw) {
    case 0: {
        s32 far_edge = (input->vx / MAP_TILE_SIZE) * MAP_TILE_SIZE + MAP_TILE_SIZE;
        output->vx = far_edge - offset;
    }
        output->vz = (input->vz / MAP_TILE_SIZE) * MAP_TILE_SIZE + offset;
        break;
    case 0x400: {
        s32 far_edge = (input->vx / MAP_TILE_SIZE) * MAP_TILE_SIZE + MAP_TILE_SIZE;
        output->vx = far_edge - offset;
    }
        {
            s32 far_edge = (input->vz / MAP_TILE_SIZE) * MAP_TILE_SIZE + MAP_TILE_SIZE;
            output->vz = far_edge - offset;
        }
        break;
    case 0x800:
        output->vx = (input->vx / MAP_TILE_SIZE) * MAP_TILE_SIZE + offset;
        {
            s32 far_edge = (input->vz / MAP_TILE_SIZE) * MAP_TILE_SIZE + MAP_TILE_SIZE;
            output->vz = far_edge - offset;
        }
        break;
    case 0xc00:
        output->vx = (input->vx / MAP_TILE_SIZE) * MAP_TILE_SIZE + offset;
        output->vz = (input->vz / MAP_TILE_SIZE) * MAP_TILE_SIZE + offset;
        break;
    }
}

/* get_tile_depth_point with offset 1: the depth point of a standing unit,
 * a cursor or a bubble on a tile without MAP_TILE_SURFACE_FLAG_DEPTH_BIAS. */
void camera_get_unbiased_tile_depth_point(const SVECTOR* world, SVECTOR* destination) {
    get_tile_depth_point(world, destination, 1);
}

/* get_tile_depth_point with offset 5: the depth point on a tile with
 * MAP_TILE_SURFACE_FLAG_DEPTH_BIAS. */
void camera_get_biased_tile_depth_point(const SVECTOR* world, SVECTOR* destination) {
    get_tile_depth_point(world, destination, 5);
}

/* The unit's draw position: its world coordinates plus its event and SEQ offsets, with the float
 * bob added to y. */
void map_unit_get_draw_coords(SVECTOR* destination, const map_unit_t* unit) {
    /* float_bob_offset is read signed (lh) here. */
    vector_set_short(destination, unit->world_coords.vx + unit->event_offset.vx + unit->seq_offset.vx,
        unit->world_coords.vy + unit->event_offset.vy + unit->seq_offset.vy + (s16)unit->float_bob_offset,
        unit->world_coords.vz + unit->event_offset.vz + unit->seq_offset.vz);
}

/* Lays the unit's shadow quad on the terrain surface under the unit.
 *
 * The quad is a 20 x 20 square of world units centered on the unit's position
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
 * sprite_draw_shadow calls it before projecting the quad. */
void sprite_build_shadow_quad(map_unit_t* unit) {
    SVECTOR coords;
    SVECTOR unused; /* unreferenced; keeps the 0x30-byte target frame */
    map_tile_t* tile;
    SVECTOR* quad;
    u8 layer = unit->map_layer;
    u8 height;
    map_tile_depth_t depth;
    u8 slope_type;
    s32 slope;
    s16 tx;
    s16 ty;
    s32 lhs;
    s32 rhs;
    s32 rise;

    map_unit_get_draw_coords(&coords, unit);
    tile = map_get_tile(coords.vx / MAP_TILE_SIZE, coords.vz / MAP_TILE_SIZE, layer);
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
    quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = -(height * MAP_HEIGHT_STEP);

    slope = depth.half_height;
    if (slope == 0) {
        return;
    }

    switch (slope_type) {
    case MAP_SLOPE_EDGE_EAST: /* edge up to east */
        tx = coords.vx % MAP_TILE_SIZE;
        rise = tx * MAP_HEIGHT_STEP;
        quad[0].vy = quad[1].vy = quad[3].vy = quad[2].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        quad[0].vy = quad[2].vy = quad[2].vy + 6;
        quad[1].vy = quad[3].vy = quad[3].vy - 6;
        break;
    case MAP_SLOPE_EDGE_WEST: /* edge up to west */
        tx = coords.vx % MAP_TILE_SIZE;
        rise = tx * MAP_HEIGHT_STEP;
        quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
            = quad[3].vy - (slope * MAP_HEIGHT_STEP - rise * slope / MAP_TILE_SIZE);
        quad[1].vy = quad[3].vy = quad[3].vy + 6;
        quad[0].vy = quad[2].vy = quad[2].vy - 6;
        break;
    case MAP_SLOPE_EDGE_SOUTH: /* edge up to south */
        ty = coords.vz % MAP_TILE_SIZE;
        rise = ty * MAP_HEIGHT_STEP;
        quad[0].vy = quad[2].vy = quad[3].vy = quad[1].vy
            = quad[3].vy - (slope * MAP_HEIGHT_STEP - rise * slope / MAP_TILE_SIZE);
        quad[0].vy = quad[1].vy = quad[1].vy + 6;
        quad[2].vy = quad[3].vy = quad[3].vy - 6;
        break;
    case MAP_SLOPE_EDGE_NORTH: /* edge up to north */
        ty = coords.vz % MAP_TILE_SIZE;
        rise = ty * MAP_HEIGHT_STEP;
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
            rise = lhs * MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        } else {
            rise = rhs * MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        }
        break;
    case MAP_SLOPE_LOW_CORNER_SOUTH_EAST: /* low corner, south-east up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = tx;
        rhs = MAP_TILE_SIZE - ty;
        if (lhs >= rhs) {
            rise = ty * MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * MAP_HEIGHT_STEP - rise * slope / MAP_TILE_SIZE);
        } else {
            rise = lhs * MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        }
        break;
    case MAP_SLOPE_LOW_CORNER_SOUTH_WEST: /* low corner, south-west up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = MAP_TILE_SIZE - tx;
        rhs = MAP_TILE_SIZE - ty;
        if (lhs >= rhs) {
            rise = ty * MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * MAP_HEIGHT_STEP - rise * slope / MAP_TILE_SIZE);
        } else {
            rise = tx * MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * MAP_HEIGHT_STEP - rise * slope / MAP_TILE_SIZE);
        }
        break;
    case MAP_SLOPE_LOW_CORNER_NORTH_WEST: /* low corner, north-west up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = MAP_TILE_SIZE - tx;
        rhs = ty;
        if (lhs >= rhs) {
            rise = rhs * MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        } else {
            rise = tx * MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * MAP_HEIGHT_STEP - rise * slope / MAP_TILE_SIZE);
        }
        break;
    case MAP_SLOPE_HIGH_CORNER_NORTH_EAST: /* high corner, north-east up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = tx;
        rhs = ty;
        if (lhs >= rhs) {
            rise = lhs * MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        } else {
            rise = rhs * MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        }
        break;
    case MAP_SLOPE_HIGH_CORNER_SOUTH_EAST: /* high corner, south-east up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = tx;
        rhs = MAP_TILE_SIZE - ty;
        if (lhs >= rhs) {
            rise = lhs * MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        } else {
            rise = ty * MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * MAP_HEIGHT_STEP - rise * slope / MAP_TILE_SIZE);
        }
        break;
    case MAP_SLOPE_HIGH_CORNER_SOUTH_WEST: /* high corner, south-west up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = MAP_TILE_SIZE - tx;
        rhs = MAP_TILE_SIZE - ty;
        if (lhs < rhs) {
            rise = ty * MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * MAP_HEIGHT_STEP - rise * slope / MAP_TILE_SIZE);
        } else {
            rise = tx * MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * MAP_HEIGHT_STEP - rise * slope / MAP_TILE_SIZE);
        }
        break;
    case MAP_SLOPE_HIGH_CORNER_NORTH_WEST: /* high corner, north-west up */
        tx = coords.vx % MAP_TILE_SIZE;
        ty = coords.vz % MAP_TILE_SIZE;
        lhs = MAP_TILE_SIZE - tx;
        rhs = ty;
        if (lhs >= rhs) {
            rise = tx * MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy
                = quad[3].vy - (slope * MAP_HEIGHT_STEP - rise * slope / MAP_TILE_SIZE);
        } else {
            rise = rhs * MAP_HEIGHT_STEP;
            quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = quad[3].vy - rise * slope / MAP_TILE_SIZE;
        }
        break;
    }
}

/* Build the four corners of the cursor quad over the tile under coords on map layer `layer`, at
 * the tile surface and following its slope. Returns 0 for a planar quad, 1 when a corner slope
 * creases it along the v0-v3 diagonal, 2 along v1-v2. */
s32 cursor_calculate_tile_polygon(const SVECTOR* coords, u8 layer, SVECTOR* quad) {
    map_tile_t* tile;
    s32 split;
    s32 rise;
    u8 height;
    u8 depth;
    u8 slope;

    split = 0;
    tile = map_get_tile(coords->vx / MAP_TILE_SIZE, coords->vz / MAP_TILE_SIZE, layer);
    height = tile->height;
    /* Byte access retains the original packed load and field-extraction order. */
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
    quad[0].vy = quad[1].vy = quad[2].vy = quad[3].vy = -((height + (depth >> MAP_TILE_DEPTH_SHIFT)) * MAP_HEIGHT_STEP);
    if (depth & MAP_TILE_HALF_HEIGHT_MASK) {
        /* The slope spans half_height levels: raise the quad to its middle, then
         * move the high corners up and the low corners down by half the span. */
        rise = (depth & MAP_TILE_HALF_HEIGHT_MASK) * (MAP_HEIGHT_STEP / 2);
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
s32 map_calculate_surface_height(const SVECTOR* coords, s32 layer) {
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
    tile = map_get_tile(tile_pos[0], tile_pos[1], (u8)layer);
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

/* How far a unit stands above a water tile's floor, in half levels: the full depth for Float and
 * on-water movement, one level less for in-water movement, else 0. */
s32 move_get_water_standing_offset(map_unit_t* unit, map_tile_t* tile) {
    s32 movement_flags;

    if (unit->battle_unit != 0) {
        movement_flags = move_get_effective_flags(unit->battle_unit);
    } else {
        movement_flags = 0;
    }
    if (tile->depth_half_height.depth) {
        if ((movement_flags & MOVE_EFFECTIVE_FLOAT) || (movement_flags & MOVE_EFFECTIVE_ON_WATER)) {
            return tile->depth_half_height.depth * 2;
        }
        if (movement_flags & MOVE_EFFECTIVE_IN_WATER) {
            return (tile->depth_half_height.depth - 1) * 2;
        }
    }
    return 0;
}

/* A rider's mount; any other unit is returned itself. */
static map_unit_t* get_mount(map_unit_t* unit) {
    if (unit->mount_state == UNIT_MOUNT_STATE_RIDER) {
        return map_unit_get(unit->mount_partner_map_unit_id);
    }
    return unit;
}

/*
 * The world y (negative is up) of a unit's feet at coords: vx and vz are world
 * x and z, vy holds the map layer.
 *
 * A unit standing on another unit (standing_on_battle_unit_id_plus_one) stands on
 * top of it, unless that unit's height is unavailable; any other unit stands
 * on the terrain surface under coords. On a water tile a unit with Float or
 * on-water movement, a treasure or a crystal stays at the water surface, a
 * chicken or frog 4 units below it, and an in-water mover or a dead unit one
 * level below it; others stand on the bottom. depth_height_offset (the rise
 * of a floating unit) then lifts every case.
 */
static s32 calculate_coords_height(map_unit_t* unit, const SVECTOR* coords) {
    unit_height_data_t height;
    map_tile_t* tile;
    s32 flags;
    s16 world_height;
    s32 sprite;
    s32 animation;
    s16 raised;

    if (unit->standing_on_battle_unit_id_plus_one != 0) {
        battle_unit_calculate_height_data(&height, unit->standing_on_battle_unit_id_plus_one - 1);
        if (height.unit_flags & UNIT_HEIGHT_UNAVAILABLE) {
            world_height = map_calculate_surface_height(coords, coords->vy & 0xff);
        } else {
            /* total_height counts half levels. */
            world_height = -height.total_height / 2 * MAP_HEIGHT_STEP;
        }
    } else {
        world_height = map_calculate_surface_height(coords, coords->vy & 0xff);
    }
    if (unit->battle_unit != 0) {
        tile = map_get_tile(coords->vx / MAP_TILE_SIZE, coords->vz / MAP_TILE_SIZE, coords->vy);
        if (tile->depth_half_height.depth) {
            flags = move_get_effective_flags(unit->battle_unit);
            sprite = unit->spritesheet_id;
            animation = unit->animation.sequence_id >> 1;
            if ((flags & MOVE_EFFECTIVE_FLOAT) || (flags & MOVE_EFFECTIVE_ON_WATER)) {
                world_height -= (tile->depth_half_height.depth) * MAP_HEIGHT_STEP + unit->depth_height_offset;
            } else if (sprite == SPRITE_SHEET_ID_CHICKEN || sprite == SPRITE_SHEET_ID_FROG) {
                raised = world_height + 4;
                world_height = raised - ((tile->depth_half_height.depth) * MAP_HEIGHT_STEP + unit->depth_height_offset);
            } else if ((flags & MOVE_EFFECTIVE_IN_WATER) || animation == ANIMATION_DEAD) {
                world_height -= ((tile->depth_half_height.depth) - 1) * MAP_HEIGHT_STEP + unit->depth_height_offset;
            } else if (sprite == SPRITE_SHEET_ID_TREASURE || sprite == SPRITE_SHEET_ID_CRYSTAL) {
                world_height -= (tile->depth_half_height.depth) * MAP_HEIGHT_STEP + unit->depth_height_offset;
            } else {
                world_height -= unit->depth_height_offset;
            }
        } else {
            world_height -= unit->depth_height_offset;
        }
    } else {
        world_height -= unit->depth_height_offset;
    }
    return world_height;
}

/* calculate_coords_height at the caller's coords (vy
 * holds the map layer), evaluated for the mount when the unit is a rider. A
 * rider's result is moved by -10 plus the mount's float bob offset.
 *
 * Here and in the three functions below, the separate adjusted_world_height
 * step keeps the target's order of operations. */
s32 map_unit_calculate_height_with_caller_coords(map_unit_t* unit, const SVECTOR* coords) {
    map_unit_t* mount = get_mount(unit);
    s32 adjusted_world_height;
    s32 world_height = calculate_coords_height(mount, coords);

    if (unit->mount_state == UNIT_MOUNT_STATE_RIDER) {
        adjusted_world_height = world_height - 10;
        world_height = mount->float_bob_offset + adjusted_world_height;
    }
    return (s16)world_height;
}

/* calculate_coords_height at the center of the movement
 * destination tile of the unit, or of its mount for a rider. A rider's result is
 * moved by -10 plus the mount's float bob offset. */
s32 map_unit_calculate_height_from_move_destination(map_unit_t* unit) {
    SVECTOR coord;
    map_unit_t* mount = get_mount(unit);
    s32 adjusted_world_height;
    s32 world_height;

    coord.vx = mount->movement.destination_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    coord.vz = mount->movement.destination_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    coord.vy = mount->movement.destination_layer;
    world_height = calculate_coords_height(mount, &coord);
    if (unit->mount_state == UNIT_MOUNT_STATE_RIDER) {
        adjusted_world_height = world_height - 10;
        world_height = mount->float_bob_offset + adjusted_world_height;
    }
    return (s16)world_height;
}

/* calculate_coords_height at the current x and map-depth
 * position of the unit, or of its mount for a rider, on the mount's map layer. A
 * rider's result is moved by -10 plus the mount's float bob offset. */
s32 map_unit_calculate_height_from_world_coords(map_unit_t* unit) {
    SVECTOR coord;
    map_unit_t* mount = get_mount(unit);
    s32 adjusted_world_height;
    s32 world_height;

    /* The target loads the s16 world coordinates unsigned (lhu). */
    coord.vx = mount->world_coords.vx;
    coord.vz = mount->world_coords.vz;
    coord.vy = mount->map_layer;
    world_height = calculate_coords_height(mount, &coord);
    if (unit->mount_state == UNIT_MOUNT_STATE_RIDER) {
        adjusted_world_height = world_height - 10;
        world_height = mount->float_bob_offset + adjusted_world_height;
    }
    return (s16)world_height;
}

/* calculate_coords_height at the center of the map tile
 * of the unit, or of its mount for a rider. A rider's result is moved by -10 plus
 * the mount's float bob offset. */
s32 map_unit_calculate_height_from_tile(map_unit_t* unit) {
    SVECTOR coord;
    map_unit_t* mount = get_mount(unit);
    s32 adjusted_world_height;
    s32 world_height;

    coord.vx = mount->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    coord.vz = mount->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    coord.vy = mount->map_layer;
    world_height = calculate_coords_height(mount, &coord);
    if (unit->mount_state == UNIT_MOUNT_STATE_RIDER) {
        adjusted_world_height = world_height - 10;
        world_height = mount->float_bob_offset + adjusted_world_height;
    }
    return (s16)world_height;
}

/* calculate_coords_height at the center of the tile
 * recorded in the battle stats of the unit, or of its mount for a rider, on the
 * mount's map layer. A rider's result is moved by -10 plus the mount's float bob
 * offset. */
s32 map_unit_calculate_height_from_battle_unit_tile(map_unit_t* unit) {
    SVECTOR coord;
    battle_unit_t* battle_unit;
    map_unit_t* mount = get_mount(unit);
    s32 adjusted_world_height;
    s32 world_height;

    battle_unit = mount->battle_unit;
    coord.vx = battle_unit->x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    battle_unit = mount->battle_unit;
    coord.vz = battle_unit->position.y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    coord.vy = mount->map_layer;
    world_height = calculate_coords_height(mount, &coord);
    if (unit->mount_state == UNIT_MOUNT_STATE_RIDER) {
        adjusted_world_height = world_height - 10;
        world_height = mount->float_bob_offset + adjusted_world_height;
    }
    return (s16)world_height;
}
