/* Advance units along tile-edge paths, from step velocities and landing effects through knockback. */

#include "fft/ai_decision.h"
#include "fft/animation.h"
#include "fft/battle_map.h"
#include "fft/battle_state.h"
#include "fft/cursor.h"
#include "fft/effect.h"
#include "fft/effect_secondary.h"
#include "fft/gfx.h"
#include "fft/move.h"
#include "fft/move_step.h"
#include "fft/sound.h"
#include "fft/sprite.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/libgte.h"
#include "psx/types.h"

extern move_step_t g_move_step_value;
extern u8 g_move_step_current_edge_height;
extern u8 g_move_step_destination_edge_height;
extern s16 g_move_step_target_world_y;

/* move_step_phase_e by step direction: the phase each kind of step enters. */
extern u8 g_move_step_float_to_center_phases[];

/* The two map tiles of the step in progress, refreshed by move_step_get_current_and_destination_tiles
 * from the unit's movement value. Step starters receive the tile pointers; the geometry readers use the
 * tile fields (height at 0x02, depth_half_height at 0x03, slope_type, flags). */
extern battle_map_tile_t* g_move_step_current_tile;
extern battle_map_tile_t* g_move_step_destination_tile;

extern unit_runtime_t* g_move_step_tile_occupant;

/* Height scratch record that unit_action_height_calculate fills in place. */
extern unit_height_data_t g_move_step_tile_occupant_height;
static s32 g_move_displacement_direction;

static u8 g_walk_to_center_phases[4] = { MOVE_STEP_PHASE_WALK_TO_CENTER_SOUTH, MOVE_STEP_PHASE_WALK_TO_CENTER_EAST,
    MOVE_STEP_PHASE_WALK_TO_CENTER_NORTH, MOVE_STEP_PHASE_WALK_TO_CENTER_WEST };

static u8 g_climb_to_center_phases[4] = { MOVE_STEP_PHASE_CLIMB_TO_CENTER_SOUTH, MOVE_STEP_PHASE_CLIMB_TO_CENTER_EAST,
    MOVE_STEP_PHASE_CLIMB_TO_CENTER_NORTH, MOVE_STEP_PHASE_CLIMB_TO_CENTER_WEST };

static u8 g_jump_descent_phases[4] = { MOVE_STEP_PHASE_JUMP_DESCENT_SOUTH, MOVE_STEP_PHASE_JUMP_DESCENT_EAST,
    MOVE_STEP_PHASE_JUMP_DESCENT_NORTH, MOVE_STEP_PHASE_JUMP_DESCENT_WEST };

static u8 g_jump_landing_phases[4] = { MOVE_STEP_PHASE_JUMP_LANDING_SOUTH, MOVE_STEP_PHASE_JUMP_LANDING_EAST,
    MOVE_STEP_PHASE_JUMP_LANDING_NORTH, MOVE_STEP_PHASE_JUMP_LANDING_WEST };

/* Clearance above a tile for height_to_headroom_clamp, in half-height
 * units.
 *
 * When the position's other layer is not blocked and its surface is higher, the
 * result is twice the gap between that layer's underside (its height less its
 * ceiling depth) and a point 3 levels above this tile's surface. Otherwise it is
 * 0x40. */
static s32 tile_headroom_get(s32 x, s32 y, u32 layer) {
    battle_map_tile_t* from = battle_map_tile_get_pointer(x, y, layer);
    battle_map_tile_t* to = battle_map_tile_get_pointer(x, y, layer ^ 1);
    s32 from_height;
    s32 to_height;
    s32 limit;

    if ((to->flags.blocked) == 0) {
        from_height = from->height + (from->depth_half_height.depth);
        to_height = to->height + (to->depth_half_height.depth);
        if (from_height < to_height) {
            limit = from_height + 3;
            return (to->height - (to->ceiling_depth_and_marks.ceiling_depth) - limit) * 2;
        }
    }
    return 0x40;
}

enum {
    MOVE_STEP_HEIGHT_SCALE = 6 /* world units per half height unit; world y grows downward */
};

/*
 * Clamp a world height (world_coords.vy) to a band above one tile. World y
 * grows downward, so the smaller bound is the higher one: `lo` is the tile
 * surface raised by the clearance of tile_headroom_get,
 * and `hi` is the surface raised by 0x30 world units (8 half heights). When the
 * band is empty the result is `lo`.
 */
static s32 height_to_headroom_clamp(s32 value, s32 x, s32 y, u32 layer) {
    battle_map_tile_t* tile = battle_map_tile_get_pointer(x, y, layer);
    s32 clearance = tile_headroom_get(x, y, layer);
    s32 height;
    s32 lo;
    s32 hi;
    battle_map_tile_depth_t depth = tile->depth_half_height;

    height = ((tile->height + depth.depth) * 2) + depth.half_height;
    lo = -(height + clearance) * MOVE_STEP_HEIGHT_SCALE;
    if (value < lo) {
        return lo;
    }
    hi = (-height * MOVE_STEP_HEIGHT_SCALE) - 0x30;
    if (value >= hi) {
        if (hi < lo) {
            return lo;
        }
        return hi;
    }
    return value;
}

enum { MOVE_STEP_HALF_WIDTH = BATTLE_MAP_TILE_SIZE / 2, MOVE_STEP_NEAR_EDGE = -7, MOVE_STEP_FAR_EDGE = 35 };

/*
 * Build the per-frame velocity for a run of identical movement steps: walk
 * the remaining path while the steps keep the same direction and the clamped
 * ground height stays at the unit's current height, then normalize the delta
 * from the unit to the far edge of the run and scale it by the vsync interval.
 * move_step_set_velocity_with_final_height is the
 * same walk with the target tile's own surface height on the path's last step.
 *
 * The end point stops 7 world units short of the near edge of the run's last
 * tile (MOVE_STEP_NEAR_EDGE when x or y grows, MOVE_STEP_FAR_EDGE, a tile plus 7,
 * when it shrinks), on the center line of the other axis.
 *
 * The step code is copied to a 16-bit value before the switch: read through the
 * byte again, cse reuses the direction test's load and the reload is lost.
 */
static void move_step_set_velocity_clamped(unit_runtime_t* unit, const move_path_t* path, const move_step_t* step) {
    VECTOR direction;
    const move_step_t* current;
    s32 start_x;
    s32 start_world_z;
    s32 start_height;
    s32 index;
    s32 x;
    s32 y;
    s32 end_x;
    s32 end_world_z;
    s32 end_height;
    s16 code;

    battle_map_tile_get_pointer(unit->map_x, unit->map_y, unit->map_layer); /* result unused */
    x = unit->map_x;
    y = unit->map_y;
    index = unit->movement_path_offset - 1;
    start_x = unit->world_coords.vx;
    start_world_z = unit->world_coords.vz;
    start_height = unit->world_coords.vy;

    while (index < path->count) {
        current = &path->data.steps[index];
        if (current->direction != step->direction) {
            break;
        }
        /* Bits 6-7 are the step's direction, bits 0-1 its length less one. */
        code = *(u8*)current;
        switch (code >> 6) {
        case MOVE_STEP_DIRECTION_EAST:
            x += (code & 3) + 1;
            end_x = (x * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_NEAR_EDGE;
            end_world_z = (y * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            break;
        case MOVE_STEP_DIRECTION_WEST:
            x -= (code & 3) + 1;
            end_x = (x * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_FAR_EDGE;
            end_world_z = (y * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            break;
        case MOVE_STEP_DIRECTION_SOUTH:
            y -= (code & 3) + 1;
            end_x = (x * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            end_world_z = (y * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_FAR_EDGE;
            break;
        case MOVE_STEP_DIRECTION_NORTH:
            y += (code & 3) + 1;
            end_x = (x * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            end_world_z = (y * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_NEAR_EDGE;
            break;
        }
        end_height = height_to_headroom_clamp(start_height, x, y, current->higher);
        if (end_height != start_height) {
            break;
        }
        index++;
    }

    direction.vx = end_x - start_x;
    direction.vz = end_world_z - start_world_z;
    direction.vy = end_height - start_height;
    VectorNormal(&direction, &unit->velocity);
    unit->velocity.vx = unit->velocity.vx * g_system_vsync_interval;
    unit->velocity.vy = unit->velocity.vy * g_system_vsync_interval;
    unit->velocity.vz = unit->velocity.vz * g_system_vsync_interval;
}

/*
 * Build the per-frame velocity for a run of identical movement steps, as
 * move_step_set_velocity_clamped does, but aiming at the
 * far edge of the run's last tile, and on the path's last step at that tile's
 * own surface height instead of the clamped one.
 *
 * The step code is copied to a 16-bit value before the switch, as in the clamped
 * twin: read through the byte again, cse reuses the direction test's load.
 */
static void move_step_set_velocity_with_final_height(
    unit_runtime_t* unit, const move_path_t* path, const move_step_t* step) {
    VECTOR direction;
    battle_map_tile_t* tile;
    const move_step_t* current;
    s32 start_x;
    s32 start_world_z;
    s32 start_height;
    s32 index;
    s32 x;
    s32 y;
    s32 end_x;
    s32 end_world_z;
    s32 end_height;
    u32 layer;
    battle_map_tile_depth_t depth;
    s16 code;

    battle_map_tile_get_pointer(unit->map_x, unit->map_y, unit->map_layer); /* result unused */
    x = unit->map_x;
    y = unit->map_y;
    index = unit->movement_path_offset - 1;
    start_x = unit->world_coords.vx;
    start_world_z = unit->world_coords.vz;
    start_height = unit->world_coords.vy;

    while (index < path->count) {
        current = &path->data.steps[index];
        if (current->direction != step->direction) {
            break;
        }
        code = *(u8*)current;
        switch (code >> 6) {
        case MOVE_STEP_DIRECTION_EAST:
            x += (code & 3) + 1;
            end_x = (x * BATTLE_MAP_TILE_SIZE) + BATTLE_MAP_TILE_SIZE;
            end_world_z = (y * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            break;
        case MOVE_STEP_DIRECTION_WEST:
            x -= (code & 3) + 1;
            end_x = x * BATTLE_MAP_TILE_SIZE;
            end_world_z = (y * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            break;
        case MOVE_STEP_DIRECTION_SOUTH:
            y -= (code & 3) + 1;
            end_x = (x * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            end_world_z = y * BATTLE_MAP_TILE_SIZE;
            break;
        case MOVE_STEP_DIRECTION_NORTH:
            y += (code & 3) + 1;
            end_x = (x * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            end_world_z = (y * BATTLE_MAP_TILE_SIZE) + BATTLE_MAP_TILE_SIZE;
            break;
        }
        layer = current->higher;
        if (unit->movement_path_offset == path->count) {
            tile = battle_map_tile_get_pointer(x, y, layer);
            depth = tile->depth_half_height;
            end_height = -(((tile->height + depth.depth) * 2) + depth.half_height) * MOVE_STEP_HEIGHT_SCALE;
        } else {
            end_height = height_to_headroom_clamp(start_height, x, y, layer);
        }
        if (end_height != start_height) {
            break;
        }
        index++;
    }

    direction.vx = end_x - start_x;
    direction.vz = end_world_z - start_world_z;
    direction.vy = end_height - start_height;
    VectorNormal(&direction, &unit->velocity);
    unit->velocity.vx = unit->velocity.vx * g_system_vsync_interval;
    unit->velocity.vy = unit->velocity.vy * g_system_vsync_interval;
    unit->velocity.vz = unit->velocity.vz * g_system_vsync_interval;
}

/* Set the unit's per-frame velocity for the first half of a step, from the tile
 * center to its exit edge.
 *
 * The direction vector is 28 along the axis the step byte's top two bits select
 * and (edge_height - 1) * the tile's slope half-height * 6 in height, negated
 * because height is negative upward. edge_height is the slope_type height
 * of the side crossed (0 low, 1 sloping, 2 high), so a low side
 * lies below the center and a high side above it. VectorNormal reduces the vector
 * to unit length (ONE) and g_system_vsync_interval scales it; the caller
 * applies the walk speed with move_step_scale_velocity. */
static void center_edge_delta_set(
    unit_runtime_t* unit, const move_step_t* step, const battle_map_tile_t* tile, s32 edge_height) {
    VECTOR direction;

    direction.vy = -((edge_height - 1) * tile->depth_half_height.half_height * (BATTLE_MAP_HEIGHT_STEP / 2));
    switch (step->direction) {
    case MOVE_STEP_DIRECTION_EAST:
        direction.vx = BATTLE_MAP_TILE_SIZE;
        direction.vz = 0;
        break;
    case MOVE_STEP_DIRECTION_WEST:
        direction.vx = -BATTLE_MAP_TILE_SIZE;
        direction.vz = 0;
        break;
    case MOVE_STEP_DIRECTION_SOUTH:
        direction.vx = 0;
        direction.vz = -BATTLE_MAP_TILE_SIZE;
        break;
    case MOVE_STEP_DIRECTION_NORTH:
        direction.vx = 0;
        direction.vz = BATTLE_MAP_TILE_SIZE;
        break;
    }
    VectorNormal(&direction, &unit->velocity);
    unit->velocity.vx = unit->velocity.vx * g_system_vsync_interval;
    unit->velocity.vy = unit->velocity.vy * g_system_vsync_interval;
    unit->velocity.vz = unit->velocity.vz * g_system_vsync_interval;
}

/* Set the unit's per-frame velocity for the second half of a step, from the entry
 * edge of a tile to its center.
 *
 * The mirror of center_edge_delta_set: the height part
 * of the direction vector, (edge_height - 1) * the tile's slope half-height * 6,
 * is not negated, so entering over a high side (slope height 2) moves the unit
 * down to the center. */
static void edge_center_delta_set(
    unit_runtime_t* unit, const move_step_t* step, const battle_map_tile_t* tile, s32 edge_height) {
    VECTOR direction;

    direction.vy = (edge_height - 1) * tile->depth_half_height.half_height * (BATTLE_MAP_HEIGHT_STEP / 2);
    switch (step->direction) {
    case MOVE_STEP_DIRECTION_EAST:
        direction.vx = BATTLE_MAP_TILE_SIZE;
        direction.vz = 0;
        break;
    case MOVE_STEP_DIRECTION_WEST:
        direction.vx = -BATTLE_MAP_TILE_SIZE;
        direction.vz = 0;
        break;
    case MOVE_STEP_DIRECTION_SOUTH:
        direction.vx = 0;
        direction.vz = -BATTLE_MAP_TILE_SIZE;
        break;
    case MOVE_STEP_DIRECTION_NORTH:
        direction.vx = 0;
        direction.vz = BATTLE_MAP_TILE_SIZE;
        break;
    }
    VectorNormal(&direction, &unit->velocity);
    unit->velocity.vx = unit->velocity.vx * g_system_vsync_interval;
    unit->velocity.vy = unit->velocity.vy * g_system_vsync_interval;
    unit->velocity.vz = unit->velocity.vz * g_system_vsync_interval;
}

/* The float steps' copy of center_edge_delta_set: the
 * original wrote the same body a second time. */
static void float_center_edge_delta_set(
    unit_runtime_t* unit, const move_step_t* step, const battle_map_tile_t* tile, s32 edge_height) {
    VECTOR direction;

    direction.vy = -((edge_height - 1) * tile->depth_half_height.half_height * (BATTLE_MAP_HEIGHT_STEP / 2));
    switch (step->direction) {
    case MOVE_STEP_DIRECTION_EAST:
        direction.vx = BATTLE_MAP_TILE_SIZE;
        direction.vz = 0;
        break;
    case MOVE_STEP_DIRECTION_WEST:
        direction.vx = -BATTLE_MAP_TILE_SIZE;
        direction.vz = 0;
        break;
    case MOVE_STEP_DIRECTION_SOUTH:
        direction.vx = 0;
        direction.vz = -BATTLE_MAP_TILE_SIZE;
        break;
    case MOVE_STEP_DIRECTION_NORTH:
        direction.vx = 0;
        direction.vz = BATTLE_MAP_TILE_SIZE;
        break;
    }
    VectorNormal(&direction, &unit->velocity);
    unit->velocity.vx = unit->velocity.vx * g_system_vsync_interval;
    unit->velocity.vy = unit->velocity.vy * g_system_vsync_interval;
    unit->velocity.vz = unit->velocity.vz * g_system_vsync_interval;
}

/* The float steps' copy of edge_center_delta_set: the
 * original wrote the same body a second time. */
static void float_edge_center_delta_set(
    unit_runtime_t* unit, const move_step_t* step, const battle_map_tile_t* tile, s32 edge_height) {
    VECTOR direction;

    direction.vy = (edge_height - 1) * tile->depth_half_height.half_height * (BATTLE_MAP_HEIGHT_STEP / 2);
    switch (step->direction) {
    case MOVE_STEP_DIRECTION_EAST:
        direction.vx = BATTLE_MAP_TILE_SIZE;
        direction.vz = 0;
        break;
    case MOVE_STEP_DIRECTION_WEST:
        direction.vx = -BATTLE_MAP_TILE_SIZE;
        direction.vz = 0;
        break;
    case MOVE_STEP_DIRECTION_SOUTH:
        direction.vx = 0;
        direction.vz = -BATTLE_MAP_TILE_SIZE;
        break;
    case MOVE_STEP_DIRECTION_NORTH:
        direction.vx = 0;
        direction.vz = BATTLE_MAP_TILE_SIZE;
        break;
    }
    VectorNormal(&direction, &unit->velocity);
    unit->velocity.vx = unit->velocity.vx * g_system_vsync_interval;
    unit->velocity.vy = unit->velocity.vy * g_system_vsync_interval;
    unit->velocity.vz = unit->velocity.vz * g_system_vsync_interval;
}

/* Scale the unit's per-frame velocity by a 4.12 factor: callers pass the walk speed, or the step
 * speed for climb-speed steps. */
static void move_step_scale_velocity(unit_runtime_t* unit, s32 scale) {
    unit->velocity.vx = (scale * unit->velocity.vx) / ONE;
    unit->velocity.vy = (scale * unit->velocity.vy) / ONE;
    unit->velocity.vz = (scale * unit->velocity.vz) / ONE;
}

/* Starts the teleport animation and sets the step destination: the tile in the path bytes,
 * or the unit's own tile when the teleport failed. */
static void move_step_start_teleport_animation(unit_runtime_t* unit) {
    unit->step_phase = MOVE_STEP_PHASE_TELEPORT_ANIMATION;
    animation_facing_store(ANIMATION_TELEPORT, unit->facing, unit);
    if (unit->path.count == MOVE_PATH_TELEPORT) {
        unit->movement.destination_x = unit->path.data.teleport.x;
        unit->movement.destination_y = unit->path.data.teleport.y;
        unit->movement.destination_layer = unit->path.data.teleport.layer;
    } else {
        unit->movement.destination_x = unit->map_x;
        unit->movement.destination_y = unit->map_y;
        unit->movement.destination_layer = unit->map_layer;
    }
}

/* Puts the unit in the TELEPORT_START step phase. The init record it builds is not used. */
static void move_step_start_teleport(unit_runtime_t* unit) {
    effect_context_t data;

    effect_secondary_init_build(unit, &data);
    unit->step_phase = MOVE_STEP_PHASE_TELEPORT_START;
}

/* Start a floating unit's next step (the FLOAT_TO_EDGE phases) from the current
 * tile center, first unlinking a unit that rides from its mount.
 *
 * The step byte's top two bits pick the direction, which sets the facing, the
 * phase and the velocity toward the exit edge; its low two bits extend the
 * destination tile coordinate. The second tile argument is unused. */
static void move_step_start_float(unit_runtime_t* unit, const battle_map_tile_t* from, const battle_map_tile_t* to) {
    move_step_t current_step;
    move_step_t* step;
    s32 edge_height;
    s16 facing;
    u8 next;

    unit_rider_dismount_display_update(unit);
    edge_height = unit->current_edge_scale;
    step = &unit->current_path_step;
    /* The byte copy prevents a wider direction load. */
    current_step = unit->current_path_step;
    switch (current_step.direction) {
    case MOVE_STEP_DIRECTION_EAST:
        facing = UNIT_FACING_EAST;
        unit->step_phase = MOVE_STEP_PHASE_FLOAT_TO_EDGE_EAST;
        next = unit->map_x + 1;
        unit->movement.destination_x = next + unit->current_path_step.length;
        break;
    case MOVE_STEP_DIRECTION_WEST:
        facing = UNIT_FACING_WEST;
        unit->step_phase = MOVE_STEP_PHASE_FLOAT_TO_EDGE_WEST;
        next = unit->map_x - 1;
        unit->movement.destination_x = next - unit->current_path_step.length;
        break;
    case MOVE_STEP_DIRECTION_SOUTH:
        facing = UNIT_FACING_SOUTH;
        unit->step_phase = MOVE_STEP_PHASE_FLOAT_TO_EDGE_SOUTH;
        next = unit->map_y - 1;
        unit->movement.destination_y = next - unit->current_path_step.length;
        break;
    case MOVE_STEP_DIRECTION_NORTH:
        facing = UNIT_FACING_NORTH;
        unit->step_phase = MOVE_STEP_PHASE_FLOAT_TO_EDGE_NORTH;
        next = unit->map_y + 1;
        unit->movement.destination_y = next + unit->current_path_step.length;
        break;
    }
    unit->facing = facing;
    animation_walk_set(unit);
    float_center_edge_delta_set(unit, step, from, edge_height);
    move_step_scale_velocity(unit, unit->walk_speed);
    unit->movement.destination_layer = step->higher;
}

/* Start a flying unit's next step (the FLY_TO_EDGE phases) with
 * ANIMATION_FLY, first unlinking a unit that rides from its mount.
 *
 * The step byte's top two bits pick the direction, which sets the facing and
 * the phase; its low two bits extend the destination tile coordinate. A carried
 * rider (mount state MOUNT) takes ANIMATION_RIDING with the mount's
 * facing. The tile arguments are not read. */
static void move_step_start_fly(unit_runtime_t* unit, const battle_map_tile_t* unused_current_tile,
    const battle_map_tile_t* unused_destination_tile) {
    move_step_t current_step;
    s32 facing;
    move_step_t* step;
    unit_runtime_t* rider;
    u8 next;

    unit_rider_dismount(unit);
    step = &unit->current_path_step;
    /* The byte copy prevents a wider direction load. */
    current_step = unit->current_path_step;
    switch (current_step.direction) {
    case MOVE_STEP_DIRECTION_EAST:
        facing = UNIT_FACING_EAST;
        unit->step_phase = MOVE_STEP_PHASE_FLY_TO_EDGE_EAST;
        next = unit->map_x + 1;
        unit->movement.destination_x = next + unit->current_path_step.length;
        break;
    case MOVE_STEP_DIRECTION_WEST:
        facing = UNIT_FACING_WEST;
        unit->step_phase = MOVE_STEP_PHASE_FLY_TO_EDGE_WEST;
        next = unit->map_x - 1;
        unit->movement.destination_x = next - unit->current_path_step.length;
        break;
    case MOVE_STEP_DIRECTION_SOUTH:
        facing = UNIT_FACING_SOUTH;
        unit->step_phase = MOVE_STEP_PHASE_FLY_TO_EDGE_SOUTH;
        next = unit->map_y - 1;
        unit->movement.destination_y = next - unit->current_path_step.length;
        break;
    case MOVE_STEP_DIRECTION_NORTH:
        facing = UNIT_FACING_NORTH;
        unit->step_phase = MOVE_STEP_PHASE_FLY_TO_EDGE_NORTH;
        next = unit->map_y + 1;
        unit->movement.destination_y = next + unit->current_path_step.length;
        break;
    }

    animation_facing_store(ANIMATION_FLY, facing, unit);
    if (unit->mount_state == UNIT_MOUNT_STATE_MOUNT) {
        rider = unit_runtime_get(unit->mount_partner_runtime_id);
        if (rider != 0) {
            animation_facing_store(ANIMATION_RIDING, unit->facing, rider);
        }
    }
    move_step_set_velocity_clamped(unit, &unit->path, step);
    move_step_scale_velocity(unit, unit->walk_speed);
    unit->movement.destination_layer = step->higher;
}

/* Start a walking unit's next step (the WALK_TO_EDGE phases) from the current
 * tile center.
 *
 * The step byte's top two bits pick the direction, which sets the facing, the
 * phase and the velocity toward the exit edge; its low two bits extend the
 * destination tile coordinate. In BATTLE_STATE_ACTION_EXECUTE, as during a
 * knockback, the unit keeps its facing and animation. The switch has no
 * default, so `facing` is unset for a direction outside 0-3 (none exists). */
static void move_step_start_walk(
    unit_runtime_t* unit, const battle_map_tile_t* from, const battle_map_tile_t* unused_to) {
    move_step_t* step;
    move_step_t current_step;
    s32 edge_height = unit->current_edge_scale;
    s16 facing;
    u8 next;

    step = &unit->current_path_step;
    /* The byte copy prevents a wider direction load. */
    current_step = unit->current_path_step;
    switch (current_step.direction) {
    case MOVE_STEP_DIRECTION_EAST:
        facing = UNIT_FACING_EAST;
        unit->step_phase = MOVE_STEP_PHASE_WALK_TO_EDGE_EAST;
        next = unit->map_x + 1;
        unit->movement.destination_x = next + unit->current_path_step.length;
        break;
    case MOVE_STEP_DIRECTION_WEST:
        facing = UNIT_FACING_WEST;
        unit->step_phase = MOVE_STEP_PHASE_WALK_TO_EDGE_WEST;
        next = unit->map_x - 1;
        unit->movement.destination_x = next - unit->current_path_step.length;
        break;
    case MOVE_STEP_DIRECTION_SOUTH:
        facing = UNIT_FACING_SOUTH;
        unit->step_phase = MOVE_STEP_PHASE_WALK_TO_EDGE_SOUTH;
        next = unit->map_y - 1;
        unit->movement.destination_y = next - unit->current_path_step.length;
        break;
    case MOVE_STEP_DIRECTION_NORTH:
        facing = UNIT_FACING_NORTH;
        unit->step_phase = MOVE_STEP_PHASE_WALK_TO_EDGE_NORTH;
        next = unit->map_y + 1;
        unit->movement.destination_y = next + unit->current_path_step.length;
        break;
    }
    if (g_battle_state != BATTLE_STATE_ACTION_EXECUTE) {
        unit->facing = facing;
        animation_walk_set(unit);
    }
    center_edge_delta_set(unit, step, from, edge_height);
    move_step_scale_velocity(unit, unit->walk_speed);
    unit->movement.destination_layer = step->higher;
}

/* Start the unit's next step at climb speed (the CLIMB_TO_EDGE phases) with
 * ANIMATION_CLIMB.
 *
 * As in move_step_start_walk, the step byte's top two bits pick the
 * direction, which sets the facing, the phase and the velocity toward the exit
 * edge, and its low two bits extend the destination tile coordinate. A carried
 * rider (mount state MOUNT) takes ANIMATION_RIDING with the mount's
 * facing. The interpolation scale is step_speed rather than the walk speed. */
static void move_step_start_climb(
    unit_runtime_t* unit, const battle_map_tile_t* from, const battle_map_tile_t* unused_to) {
    move_step_t current_step;
    move_step_t* step;
    s32 edge_height;
    unit_runtime_t* rider;
    u8 next;

    edge_height = unit->current_edge_scale;
    step = &unit->current_path_step;
    /* The byte copy prevents a wider direction load. */
    current_step = unit->current_path_step;
    switch (current_step.direction) {
    case MOVE_STEP_DIRECTION_EAST:
        unit->step_phase = MOVE_STEP_PHASE_CLIMB_TO_EDGE_EAST;
        animation_facing_store(ANIMATION_CLIMB, UNIT_FACING_EAST, unit);
        next = unit->map_x + 1;
        unit->movement.destination_x = next + unit->current_path_step.length;
        break;
    case MOVE_STEP_DIRECTION_WEST:
        unit->step_phase = MOVE_STEP_PHASE_CLIMB_TO_EDGE_WEST;
        animation_facing_store(ANIMATION_CLIMB, UNIT_FACING_WEST, unit);
        next = unit->map_x - 1;
        unit->movement.destination_x = next - unit->current_path_step.length;
        break;
    case MOVE_STEP_DIRECTION_SOUTH:
        unit->step_phase = MOVE_STEP_PHASE_CLIMB_TO_EDGE_SOUTH;
        animation_facing_store(ANIMATION_CLIMB, UNIT_FACING_SOUTH, unit);
        next = unit->map_y - 1;
        unit->movement.destination_y = next - unit->current_path_step.length;
        break;
    case MOVE_STEP_DIRECTION_NORTH:
        unit->step_phase = MOVE_STEP_PHASE_CLIMB_TO_EDGE_NORTH;
        animation_facing_store(ANIMATION_CLIMB, UNIT_FACING_NORTH, unit);
        next = unit->map_y + 1;
        unit->movement.destination_y = next + unit->current_path_step.length;
        break;
    }
    if (unit->mount_state == UNIT_MOUNT_STATE_MOUNT) {
        rider = unit_runtime_get(unit->mount_partner_runtime_id);
        if (rider != 0) {
            animation_facing_store(ANIMATION_RIDING, unit->facing, rider);
        }
    }
    center_edge_delta_set(unit, step, from, edge_height);
    move_step_scale_velocity(unit, unit->step_speed);
    unit->movement.destination_layer = step->higher;
}

/* Start a unit's jump step toward the next path tile.
 *
 * The step byte's low two bits give the extra tiles the jump spans and its top
 * two bits the direction. The launch speed is the jump gravity times the
 * distance to the destination center (length tiles plus half a tile) in 6-unit
 * half-steps, and the horizontal speed is a fixed 3 world units per frame. A
 * jump of two or more extra tiles takes the JUMP_START phase and the jump
 * animation facing the travel direction; a shorter one goes straight to
 * JUMP_ASCENT, dismounts any rider and alternates between the two hop
 * animations (movement.alternate_short_hop). */
static void jump_start(unit_runtime_t* unit, const battle_map_tile_t* unused_from, const battle_map_tile_t* unused_to) {
    u16 facings[4] = { UNIT_FACING_EAST, UNIT_FACING_WEST, UNIT_FACING_SOUTH, UNIT_FACING_NORTH };
    u8 next;
    u8 jump_phases[4] = { MOVE_STEP_PHASE_JUMP_START_EAST, MOVE_STEP_PHASE_JUMP_START_WEST,
        MOVE_STEP_PHASE_JUMP_START_SOUTH, MOVE_STEP_PHASE_JUMP_START_NORTH };
    u8 hop_phases[4] = { MOVE_STEP_PHASE_JUMP_ASCENT_EAST, MOVE_STEP_PHASE_JUMP_ASCENT_WEST,
        MOVE_STEP_PHASE_JUMP_ASCENT_SOUTH, MOVE_STEP_PHASE_JUMP_ASCENT_NORTH };
    move_step_t* step;
    s32 animation;
    move_step_t current_step;

    /* Copy the byte to retain narrow loads of the packed step fields. */
    current_step = unit->current_path_step;
    unit->velocity.vy = -g_move_jump_gravity
        * (((current_step.length * (BATTLE_MAP_TILE_SIZE * ONE)) + (BATTLE_MAP_TILE_SIZE * ONE / 2)) / (6 * ONE));
    step = &unit->current_path_step;
    current_step = unit->current_path_step;
    if (current_step.length >= 2) {
        unit->step_phase = jump_phases[current_step.direction];
        current_step = unit->current_path_step;
        animation_facing_store(ANIMATION_JUMP, facings[current_step.direction], unit);
    } else {
        unit->step_phase = hop_phases[current_step.direction];
        unit_rider_dismount(unit);
        animation = ANIMATION_HOP;
        if (unit->movement.alternate_short_hop) {
            animation = ANIMATION_HOP_ALTERNATE;
        }
        animation_facing_store(animation, unit->facing, unit);
        unit->movement.alternate_short_hop ^= 1;
    }
    switch (step->direction) {
    case MOVE_STEP_DIRECTION_EAST:
        next = unit->map_x + 1;
        unit->movement.destination_x = next + step->length;
        unit->velocity.vx = 3 * ONE;
        unit->velocity.vz = 0;
        break;
    case MOVE_STEP_DIRECTION_WEST:
        next = unit->map_x - 1;
        unit->movement.destination_x = next - step->length;
        unit->velocity.vx = -3 * ONE;
        unit->velocity.vz = 0;
        break;
    case MOVE_STEP_DIRECTION_SOUTH:
        next = unit->map_y - 1;
        unit->movement.destination_y = next - step->length;
        unit->velocity.vx = 0;
        unit->velocity.vz = -3 * ONE;
        break;
    case MOVE_STEP_DIRECTION_NORTH:
        next = unit->map_y + 1;
        unit->movement.destination_y = next + step->length;
        unit->velocity.vx = 0;
        unit->velocity.vz = 3 * ONE;
        break;
    }
    unit->movement.destination_layer = step->higher;
}

/* Start a unit's jump across one or more tiles onto the destination tile.
 *
 * Heights are in half-steps of 6 world units. The landing height is the top of
 * the unit it lands on (standing_on_battle_id_plus_one, set for a step onto a
 * unit; per unit_action_height_calculate) or, when there is none or its
 * height is UNIT_HEIGHT_UNAVAILABLE, the destination tile's height plus
 * its slope scaled by destination_edge_scale, plus 2. The launch speed comes
 * from the rise over the unit's current height plus two half-steps of
 * clearance per tile crossed (length + 1); the frame count adds a fall from
 * the apex, and the horizontal speed covers half a tile over those frames.
 * The step byte's top two bits give the direction, as in
 * jump_start. `unused_10` reproduces an unreferenced
 * 16-byte frame slot below the height data. */
static void climb_jump_start(unit_runtime_t* unit, const battle_map_tile_t* from, const battle_map_tile_t* to) {
    VECTOR unused_10;
    unit_height_data_t height_data;
    move_step_t* step = &unit->current_path_step;
    s32 height;
    s32 speed;
    s32 time;
    u8 next;

    if (unit->standing_on_battle_id_plus_one != 0
        && (unit_action_height_calculate(&height_data, unit->standing_on_battle_id_plus_one - 1),
            (height_data.unit_flags & UNIT_HEIGHT_UNAVAILABLE) == 0)) {
        height = height_data.total_height;
    } else {
        height = to->height * 2 + to->depth_half_height.half_height * unit->destination_edge_scale + 2;
    }
    /* Grouping each constant with the gravity term keeps the target's order
     * (scale, then multiply by gravity); the flat spelling scales gravity first. */
    speed
        = SquareRoot12((height - -unit->world_coords.vy / 6 + (step->length + 1) * 2) * (6 * g_move_jump_gravity) * 2);
    time = speed / g_move_jump_gravity;
    unit->velocity.vy = -speed;
    time += SquareRoot12((step->length + 1) * (12 * g_move_jump_gravity) * 2) / g_move_jump_gravity;
    switch (step->direction) {
    case MOVE_STEP_DIRECTION_EAST:
        unit->step_phase = MOVE_STEP_PHASE_JUMP_START_EAST;
        animation_facing_store(ANIMATION_JUMP, UNIT_FACING_EAST, unit);
        next = unit->map_x + 1;
        unit->movement.destination_x = next + step->length;
        unit->velocity.vx = (BATTLE_MAP_TILE_SIZE * ONE / 2) / time;
        unit->velocity.vz = 0;
        break;
    case MOVE_STEP_DIRECTION_WEST:
        unit->step_phase = MOVE_STEP_PHASE_JUMP_START_WEST;
        animation_facing_store(ANIMATION_JUMP, UNIT_FACING_WEST, unit);
        next = unit->map_x - 1;
        unit->movement.destination_x = next - step->length;
        unit->velocity.vx = -(BATTLE_MAP_TILE_SIZE * ONE / 2) / time;
        unit->velocity.vz = 0;
        break;
    case MOVE_STEP_DIRECTION_SOUTH:
        unit->step_phase = MOVE_STEP_PHASE_JUMP_START_SOUTH;
        animation_facing_store(ANIMATION_JUMP, UNIT_FACING_SOUTH, unit);
        next = unit->map_y - 1;
        unit->movement.destination_y = next - step->length;
        unit->velocity.vz = -(BATTLE_MAP_TILE_SIZE * ONE / 2) / time;
        unit->velocity.vx = 0;
        break;
    case MOVE_STEP_DIRECTION_NORTH:
        unit->step_phase = MOVE_STEP_PHASE_JUMP_START_NORTH;
        animation_facing_store(ANIMATION_JUMP, UNIT_FACING_NORTH, unit);
        next = unit->map_y + 1;
        unit->movement.destination_y = next + step->length;
        unit->velocity.vz = (BATTLE_MAP_TILE_SIZE * ONE / 2) / time;
        unit->velocity.vx = 0;
        break;
    }
    unit->movement.destination_layer = step->higher;
}

/* Starts a unit's hop up to a tile at most seven half-steps higher (called from
 * move_step_update_walk_to_edge): derives the launch speed and
 * flight frames from the rise to the landing height, then sets the facing,
 * destination tile and horizontal velocity (half a tile over the flight) for
 * the direction in bits 6-7 of `current_path_step`, and plays the next of the
 * two alternating hop animations.
 *
 * Case bodies store the destination before clearing the other velocity axis;
 * that order gives the target's map_x/step load order. */
static void climb_hop_start(unit_runtime_t* unit, const battle_map_tile_t* from, const battle_map_tile_t* to) {
    s32 unused[4];
    unit_height_data_t height_data;
    s32 rise;
    s32 frames;
    move_step_t* step;
    s32 rise_height;
    s32 height;
    u32 animation;
    u8 next;

    step = &unit->current_path_step;
    if (unit->standing_on_battle_id_plus_one != 0
        && (unit_action_height_calculate(&height_data, unit->standing_on_battle_id_plus_one - 1),
            (height_data.unit_flags & UNIT_HEIGHT_UNAVAILABLE) == 0)) {
        height = height_data.total_height;
    } else {
        height = to->height * 2 + to->depth_half_height.half_height * unit->destination_edge_scale;
    }
    rise_height = (height - -unit->world_coords.vy / 6) * 6 + 1;
    rise = SquareRoot12(rise_height * g_move_jump_gravity * 2);
    unit->velocity.vy = -rise;
    frames = rise / g_move_jump_gravity;
    switch (step->direction) {
    case MOVE_STEP_DIRECTION_EAST:
        unit->step_phase = MOVE_STEP_PHASE_JUMP_ASCENT_EAST;
        unit->facing = UNIT_FACING_EAST;
        next = unit->map_x + 1;
        unit->movement.destination_x = next + step->length;
        unit->velocity.vz = 0;
        unit->velocity.vx = (BATTLE_MAP_TILE_SIZE * ONE / 2) / frames;
        break;
    case MOVE_STEP_DIRECTION_WEST:
        unit->step_phase = MOVE_STEP_PHASE_JUMP_ASCENT_WEST;
        unit->facing = UNIT_FACING_WEST;
        next = unit->map_x - 1;
        unit->movement.destination_x = next - step->length;
        unit->velocity.vz = 0;
        unit->velocity.vx = -(BATTLE_MAP_TILE_SIZE * ONE / 2) / frames;
        break;
    case MOVE_STEP_DIRECTION_SOUTH:
        unit->step_phase = MOVE_STEP_PHASE_JUMP_ASCENT_SOUTH;
        unit->facing = UNIT_FACING_SOUTH;
        next = unit->map_y - 1;
        unit->movement.destination_y = next - step->length;
        unit->velocity.vx = 0;
        unit->velocity.vz = -(BATTLE_MAP_TILE_SIZE * ONE / 2) / frames;
        break;
    case MOVE_STEP_DIRECTION_NORTH:
        /* Without this reorg moves this case's `li a0,0xe000` into the
         * `beq a1,3` delay slot, where the target keeps a nop; an asm insn
         * stops fill_slots_from_thread's scan (stop_search_p). */
        __asm__("");
        unit->step_phase = MOVE_STEP_PHASE_JUMP_ASCENT_NORTH;
        unit->facing = UNIT_FACING_NORTH;
        next = unit->map_y + 1;
        unit->movement.destination_y = next + step->length;
        unit->velocity.vx = 0;
        unit->velocity.vz = (BATTLE_MAP_TILE_SIZE * ONE / 2) / frames;
        break;
    }
    animation = ANIMATION_HOP;
    if (unit->movement.alternate_short_hop) {
        animation = ANIMATION_HOP_ALTERNATE;
    }
    animation_facing_store(animation, unit->facing, unit);
    unit->movement.alternate_short_hop ^= 1;
    unit->movement.destination_layer = step->higher;
}

/* Start a unit's climb jump from its current tile onto the higher tile `to`.
 *
 * The rise velocity is sqrt(2 * gravity * height) for the height difference in
 * half-height steps (6 world units each) plus two half-steps per tile of jump
 * length; a second apex term extends the frame count. The step byte's top two
 * bits pick the direction, which sets the phase, facing, the destination
 * coordinate, and a half-tile horizontal displacement spread over the frame
 * count. `from` is unused. `unused` reproduces an unreferenced 24-byte
 * frame slot. The separate `height -=`/`height +=` statements keep the target's
 * in-place accumulation register; `rise_height` and `apex` keep the multiplication
 * order (term * 6 or 12 before gravity) that one folded expression reassociates.
 */
static void float_jump_start(unit_runtime_t* unit, const battle_map_tile_t* from, const battle_map_tile_t* to) {
    move_step_t current_step;
    s32 rise;
    s32 frames;
    move_step_t* step;
    s32 unused[6];
    s32 rise_height;
    s32 height;
    s32 apex;
    u8 next;

    step = &unit->current_path_step;
    height = to->height * 2 + to->depth_half_height.half_height * unit->destination_edge_scale + 2;
    height -= -unit->world_coords.vy / 6;
    current_step = unit->current_path_step;
    height += (current_step.length + 1) * 2;
    rise_height = height * 6;
    rise = SquareRoot12(rise_height * g_move_jump_gravity * 2);
    frames = rise / g_move_jump_gravity;
    unit->velocity.vy = -rise;
    current_step = unit->current_path_step;
    apex = (current_step.length + 1) * 12;
    frames += SquareRoot12(apex * g_move_jump_gravity * 2) / g_move_jump_gravity;
    /* The byte copy prevents a wider direction load. */
    current_step = unit->current_path_step;
    switch (current_step.direction) {
    case MOVE_STEP_DIRECTION_EAST:
        unit->step_phase = MOVE_STEP_PHASE_FLOAT_JUMP_EAST;
        unit->facing = UNIT_FACING_EAST;
        unit->velocity.vz = 0;
        next = unit->map_x + 1;
        unit->movement.destination_x = next + unit->current_path_step.length;
        unit->velocity.vx = (BATTLE_MAP_TILE_SIZE * ONE / 2) / frames;
        break;
    case MOVE_STEP_DIRECTION_WEST:
        unit->step_phase = MOVE_STEP_PHASE_FLOAT_JUMP_WEST;
        unit->facing = UNIT_FACING_WEST;
        unit->velocity.vz = 0;
        next = unit->map_x - 1;
        unit->movement.destination_x = next - unit->current_path_step.length;
        unit->velocity.vx = -(BATTLE_MAP_TILE_SIZE * ONE / 2) / frames;
        break;
    case MOVE_STEP_DIRECTION_SOUTH:
        unit->step_phase = MOVE_STEP_PHASE_FLOAT_JUMP_SOUTH;
        unit->facing = UNIT_FACING_SOUTH;
        unit->velocity.vx = 0;
        next = unit->map_y - 1;
        unit->movement.destination_y = next - unit->current_path_step.length;
        unit->velocity.vz = -(BATTLE_MAP_TILE_SIZE * ONE / 2) / frames;
        break;
    case MOVE_STEP_DIRECTION_NORTH:
        unit->step_phase = MOVE_STEP_PHASE_FLOAT_JUMP_NORTH;
        unit->facing = UNIT_FACING_NORTH;
        unit->velocity.vx = 0;
        next = unit->map_y + 1;
        unit->movement.destination_y = next + unit->current_path_step.length;
        unit->velocity.vz = (BATTLE_MAP_TILE_SIZE * ONE / 2) / frames;
        break;
    }
    unit->movement.destination_layer = step->higher;
}

/* Advance a moving unit by one frame of its step: add the per-frame velocity to
 * its real (Q12) position, keep the height consistent with the ground for the
 * stage in step_phase, and refresh the integer world coordinates. A mount also places
 * its rider, 10 world units above it.
 *
 * The tile under the new position is real / 0x1c000 (28 world units per tile);
 * on entering the destination tile the unit takes the destination's layer. Height
 * is negative upward. ground_height and destination_height are the integer world
 * Y heights under the unit and at its destination, including rider adjustments. Per stage:
 * - walk and climb, to the edge or to the center: at or below the ground the unit
 *   snaps onto it and the horizontal delta is rebuilt for the slope (the climb
 *   stages first raise step_speed by g_move_jump_gravity, up to 0x7fff).
 *   Above the ground it snaps when less than 6 units up, otherwise it gains
 *   gravity; 0x2a units or more above the ground (the destination's ground in
 *   the to-center stages) turns the step into a jump, with the fall animation
 *   outside action execution.
 * - jump ascent and descent, float jump: gravity, stopping at the ground while
 *   the unit moves down.
 * - float: at the ground, snap and rebuild the float delta; above it the fall
 *   speed gains gravity while it is under 0x2000.
 * - fly: at the ground, rebuild the velocity for the run of contiguous steps.
 *
 * The first switch repeats the layer test that the statement before it has just
 * made false, so its bodies never run. */
static void move_step_apply_velocity(unit_runtime_t* unit) {
    point16_t tile;
    u8 layer;
    s16 ground_height;
    s16 destination_height;
    battle_map_tile_t* map_tile;
    unit_runtime_t* rider;
    s32 depth;

    tile.x = (unit->real.vx + unit->velocity.vx) / (BATTLE_MAP_TILE_SIZE * ONE);
    tile.y = (unit->real.vz + unit->velocity.vz) / (BATTLE_MAP_TILE_SIZE * ONE);
    /* Entering the destination tile switches the unit to the destination's layer. */
    layer = unit->map_layer;
    if (tile.x == unit->movement.destination_x && tile.y == unit->movement.destination_y
        && unit->map_layer != unit->movement.destination_layer) {
        layer = unit->map_layer = unit->movement.destination_layer;
    }

    /* Never runs: the statement above has already made map_layer equal destination_layer here. */
    switch (unit->step_phase) {
    case MOVE_STEP_PHASE_WALK_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_WEST:
        if (tile.x == unit->movement.destination_x && tile.y == unit->movement.destination_y
            && unit->map_layer != unit->movement.destination_layer) {
            layer = unit->map_layer = unit->movement.destination_layer;
            map_tile = battle_map_tile_get_pointer(tile.x, tile.y, layer);
            edge_center_delta_set(unit, &unit->current_path_step, map_tile, unit->destination_edge_scale);
            move_step_scale_velocity(unit, unit->walk_speed);
        }
        break;
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_WEST:
        if (tile.x == unit->movement.destination_x && tile.y == unit->movement.destination_y
            && unit->map_layer != unit->movement.destination_layer) {
            layer = unit->map_layer = unit->movement.destination_layer;
            map_tile = battle_map_tile_get_pointer(tile.x, tile.y, layer);
            edge_center_delta_set(unit, &unit->current_path_step, map_tile, unit->destination_edge_scale);
            move_step_scale_velocity(unit, unit->step_speed);
        }
        break;
    }

    /* Move horizontally, then sample the ground under the unit and at its destination. */
    unit->real.vx += unit->velocity.vx;
    unit->real.vz += unit->velocity.vz;
    unit->world_coords.vx = unit->real.vx / ONE;
    unit->world_coords.vz = unit->real.vz / ONE;
    ground_height = unit_height_calculate_from_world_coords(unit);
    destination_height = unit_height_calculate_from_move_destination(unit);

    /* Vertical motion for the current stage; heights are negative upward. */
    switch (unit->step_phase) {
    case MOVE_STEP_PHASE_WALK_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_WALK_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_WALK_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_WALK_TO_EDGE_WEST:
        unit->real.vy += unit->velocity.vy;
        map_tile = battle_map_tile_get_pointer(tile.x, tile.y, layer);
        if ((-unit->real.vy / ONE) <= -ground_height) {
            unit->real.vy = ground_height * ONE;
            center_edge_delta_set(unit, &unit->current_path_step, map_tile, unit->current_edge_scale);
            move_step_scale_velocity(unit, unit->walk_speed);
        } else {
            if ((-unit->real.vy / ONE) < -ground_height + 6) {
                unit->real.vy = ground_height * ONE;
            } else if (unit->velocity.vy <= 0x7FFF) {
                unit->velocity.vy += g_move_jump_gravity;
            }
            if (-unit->real.vy / ONE >= -ground_height + 0x2A) {
                unit->step_phase = MOVE_STEP_PHASE_JUMP_ASCENT_NORTH;
                if (g_battle_state != BATTLE_STATE_ACTION_EXECUTE) {
                    animation_facing_store(ANIMATION_FALL, unit->facing, unit);
                }
            }
        }
        unit->shadow_dirty |= 1;
        break;
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_WEST:
        unit->real.vy += unit->velocity.vy;
        map_tile = battle_map_tile_get_pointer(tile.x, tile.y, layer);
        if ((-unit->real.vy / ONE) <= -ground_height) {
            unit->real.vy = ground_height * ONE;
            if (unit->step_speed <= 0x7FFF) {
                unit->step_speed += g_move_jump_gravity;
            }
            center_edge_delta_set(unit, &unit->current_path_step, map_tile, unit->current_edge_scale);
            move_step_scale_velocity(unit, unit->step_speed);
        } else {
            if ((-unit->real.vy / ONE) < -ground_height + 6) {
                unit->real.vy = ground_height * ONE;
            } else if (unit->velocity.vy <= 0x7FFF) {
                unit->velocity.vy += g_move_jump_gravity;
            }
            if (-unit->real.vy / ONE >= -ground_height + 0x2A) {
                unit->step_phase = MOVE_STEP_PHASE_JUMP_ASCENT_NORTH;
                if (g_battle_state != BATTLE_STATE_ACTION_EXECUTE) {
                    animation_facing_store(ANIMATION_FALL, unit->facing, unit);
                }
            }
        }
        unit->shadow_dirty |= 1;
        break;
    case MOVE_STEP_PHASE_WALK_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_WEST:
        unit->real.vy += unit->velocity.vy;
        map_tile = battle_map_tile_get_pointer(tile.x, tile.y, layer);
        if (-unit->real.vy / ONE <= -ground_height) {
            unit->real.vy = ground_height * ONE;
            edge_center_delta_set(unit, &unit->current_path_step, map_tile, unit->destination_edge_scale);
            move_step_scale_velocity(unit, unit->walk_speed);
        }
        if (-ground_height < (-unit->real.vy / ONE)) {
            if ((-unit->real.vy / ONE) < -ground_height + 6) {
                unit->real.vy = ground_height * ONE;
            } else if (unit->velocity.vy <= 0x7FFF) {
                unit->velocity.vy += g_move_jump_gravity;
            }
            if (-unit->real.vy / ONE >= -destination_height + 0x2A) {
                unit->step_phase = MOVE_STEP_PHASE_JUMP_DESCENT_NORTH;
                if (g_battle_state != BATTLE_STATE_ACTION_EXECUTE) {
                    animation_facing_store(ANIMATION_FALL, unit->facing, unit);
                }
            }
        }
        unit->shadow_dirty |= 1;
        break;
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_WEST:
        unit->real.vy += unit->velocity.vy;
        map_tile = battle_map_tile_get_pointer(tile.x, tile.y, layer);
        if (-unit->real.vy / ONE <= -ground_height) {
            unit->real.vy = ground_height * ONE;
            if (unit->step_speed <= 0x7FFF) {
                unit->step_speed += g_move_jump_gravity;
            }
            edge_center_delta_set(unit, &unit->current_path_step, map_tile, unit->destination_edge_scale);
            move_step_scale_velocity(unit, unit->step_speed);
        }
        if ((-unit->real.vy / ONE) > -ground_height) {
            if ((-unit->real.vy / ONE) < -ground_height + 6) {
                unit->real.vy = ground_height * ONE;
            } else if (unit->velocity.vy <= 0x7FFF) {
                unit->velocity.vy += g_move_jump_gravity;
            }
            if (-unit->real.vy / ONE >= -destination_height + 0x2A) {
                unit->step_phase = MOVE_STEP_PHASE_JUMP_DESCENT_NORTH;
                if (g_battle_state != BATTLE_STATE_ACTION_EXECUTE) {
                    animation_facing_store(ANIMATION_FALL, unit->facing, unit);
                }
            }
        }
        unit->shadow_dirty |= 1;
        break;
    case MOVE_STEP_PHASE_JUMP_ASCENT_SOUTH:
    case MOVE_STEP_PHASE_JUMP_DESCENT_SOUTH:
    case MOVE_STEP_PHASE_JUMP_ASCENT_EAST:
    case MOVE_STEP_PHASE_JUMP_DESCENT_EAST:
    case MOVE_STEP_PHASE_JUMP_ASCENT_NORTH:
    case MOVE_STEP_PHASE_JUMP_DESCENT_NORTH:
    case MOVE_STEP_PHASE_JUMP_ASCENT_WEST:
    case MOVE_STEP_PHASE_JUMP_DESCENT_WEST:
    case MOVE_STEP_PHASE_FLOAT_JUMP_SOUTH:
    case MOVE_STEP_PHASE_FLOAT_JUMP_EAST:
    case MOVE_STEP_PHASE_FLOAT_JUMP_NORTH:
    case MOVE_STEP_PHASE_FLOAT_JUMP_WEST:
        if (unit->velocity.vy <= 0x7FFF) {
            unit->velocity.vy += g_move_jump_gravity;
        }
        unit->real.vy += unit->velocity.vy;
        if (unit->velocity.vy > 0) {
            if (-unit->real.vy / ONE < -ground_height) {
                unit->real.vy = ground_height * ONE;
            }
        }
        unit->shadow_dirty |= 1;
        break;
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_WEST:
        unit->real.vy += unit->velocity.vy;
        map_tile = battle_map_tile_get_pointer(tile.x, tile.y, layer);
        depth = -unit->real.vy;
        if (depth / ONE <= -ground_height) {
            unit->real.vy = ground_height * ONE;
            float_center_edge_delta_set(unit, &unit->current_path_step, map_tile, unit->destination_edge_scale);
            move_step_scale_velocity(unit, unit->walk_speed);
        } else {
            /* Shared with the next case: a duplicated branch does not merge. */
            goto accelerate;
        }
        unit->shadow_dirty |= 1;
        break;
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_WEST:
        unit->real.vy += unit->velocity.vy;
        map_tile = battle_map_tile_get_pointer(tile.x, tile.y, layer);
        depth = -unit->real.vy;
        if (depth / ONE <= -ground_height) {
            unit->real.vy = ground_height * ONE;
            float_edge_center_delta_set(unit, &unit->current_path_step, map_tile, unit->destination_edge_scale);
            move_step_scale_velocity(unit, unit->walk_speed);
        } else {
        accelerate:
            /* Keeps depth live here, so it takes a0 and reorg leaves `move a0,s0` out of both landing tests. */
            __asm__("" : : "r"(depth));
            if (unit->velocity.vy < 0x2000) {
                unit->velocity.vy += g_move_jump_gravity;
            }
        }
        unit->shadow_dirty |= 1;
        break;
    case MOVE_STEP_PHASE_FLY_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_FLY_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_FLY_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_FLY_TO_EDGE_WEST:
        unit->real.vy += unit->velocity.vy;
        battle_map_tile_get_pointer(tile.x, tile.y, layer);
        if (-unit->real.vy / ONE <= -ground_height) {
            unit->real.vy = ground_height * ONE;
            move_step_set_velocity_clamped(unit, &unit->path, &unit->current_path_step);
            move_step_scale_velocity(unit, unit->walk_speed);
        }
        unit->shadow_dirty |= 1;
        break;
    case MOVE_STEP_PHASE_FLY_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_FLY_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_FLY_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_FLY_TO_CENTER_WEST:
        unit->real.vy += unit->velocity.vy;
        battle_map_tile_get_pointer(tile.x, tile.y, layer);
        depth = -unit->real.vy;
        if (depth / ONE <= -ground_height) {
            unit->real.vy = ground_height * ONE;
            move_step_set_velocity_with_final_height(unit, &unit->path, &unit->current_path_step);
            move_step_scale_velocity(unit, unit->walk_speed);
        }
        unit->shadow_dirty |= 1;
        break;
    }

    unit_set_world_coords_from_real_coords(unit);
    /* A mount carries its rider 10 world units higher (0xA000 in Q12). */
    if (unit->mount_state == UNIT_MOUNT_STATE_MOUNT) {
        rider = unit_runtime_get(unit->mount_partner_runtime_id);
        if (rider != 0) {
            rider->world_coords = unit->world_coords;
            rider->world_coords.vy -= 10;
            rider->real = unit->real;
            rider->real.vy -= 0xA000;
        }
    }
}

/* Play a sound on a free SFX channel pair unless the unit's motion flags suppress
 * its sounds. The unit sequence scripts, the jump and landing steps and the death
 * animations play their sounds through it. */
void unit_sfx_play(unit_runtime_t* unit, s32 sound_id) {
    if ((unit->movement.suppress_sfx_and_landing_effects) == 0) {
        sound_free_channel_sfx_play(sound_id);
    }
}

/* Play the landing sound for the surface of the tile a unit's step ends on:
 * SOUND_SFX_LAND_SOFT for grassland, thicket and rug, SOUND_SFX_LAND_WATER for the
 * water and marsh surfaces, SOUND_SFX_LAND_WOOD for wooden floor, tree and box,
 * and SOUND_SFX_LAND_DEFAULT otherwise. Nothing plays for a unit whose motion
 * flags suppress landing effects, or when the destination tile does not exist. */
static void landing_sfx_play(unit_runtime_t* unit) {
    battle_map_tile_t* tile;
    s32 surface;
    s32 sfx;

    if (unit->movement.suppress_sfx_and_landing_effects)
        return;
    tile = battle_map_tile_get_pointer(
        unit->movement.destination_x, unit->movement.destination_y, unit->movement.destination_layer);
    if (tile == 0)
        return;
    surface = tile->surface.type;
    switch (surface) {
    case BATTLE_MAP_SURFACE_GRASSLAND:
    case BATTLE_MAP_SURFACE_THICKET:
    case BATTLE_MAP_SURFACE_RUG:
        sfx = SOUND_SFX_LAND_SOFT;
        break;
    case BATTLE_MAP_SURFACE_SWAMP:
    case BATTLE_MAP_SURFACE_MARSH:
    case BATTLE_MAP_SURFACE_POISONED_MARSH:
    case BATTLE_MAP_SURFACE_WATERWAY:
    case BATTLE_MAP_SURFACE_RIVER:
    case BATTLE_MAP_SURFACE_LAKE:
    case BATTLE_MAP_SURFACE_SEA:
    case BATTLE_MAP_SURFACE_WATERFALL:
        sfx = SOUND_SFX_LAND_WATER;
        break;
    case BATTLE_MAP_SURFACE_WOODEN_FLOOR:
    case BATTLE_MAP_SURFACE_TREE:
    case BATTLE_MAP_SURFACE_BOX:
        sfx = SOUND_SFX_LAND_WOOD;
        break;
    default:
        sfx = SOUND_SFX_LAND_DEFAULT;
        break;
    }
    sound_free_channel_sfx_play(sfx);
}

/* Start the landing effect that suits the surface of the unit's destination tile:
 * the fall dust on hard ground, floors and structures, the splash on swamp, marsh
 * and open water. Other surfaces (grassland, thicket, snow, lava, rugs, furniture
 * and the like) get none. Nothing starts for a unit whose motion flags suppress
 * landing effects, or when the destination tile does not exist. */
static void landing_secondary_set(unit_runtime_t* unit) {
    battle_map_tile_t* tile;
    s32 surface;

    if (unit->movement.suppress_sfx_and_landing_effects)
        return;
    tile = battle_map_tile_get_pointer(
        unit->movement.destination_x, unit->movement.destination_y, unit->movement.destination_layer);
    if (tile == 0)
        return;
    surface = tile->surface.type;
    switch (surface) {
    case BATTLE_MAP_SURFACE_SAND:
    case BATTLE_MAP_SURFACE_STALACTITE:
    case BATTLE_MAP_SURFACE_ROCKY_CLIFF:
    case BATTLE_MAP_SURFACE_GRAVEL:
    case BATTLE_MAP_SURFACE_WASTELAND:
    case BATTLE_MAP_SURFACE_LAVA_ROCKS:
    case BATTLE_MAP_SURFACE_ICE:
    case BATTLE_MAP_SURFACE_ROAD:
    case BATTLE_MAP_SURFACE_WOODEN_FLOOR:
    case BATTLE_MAP_SURFACE_STONE_FLOOR:
    case BATTLE_MAP_SURFACE_ROOF:
    case BATTLE_MAP_SURFACE_STONE_WALL:
    case BATTLE_MAP_SURFACE_SALT:
    case BATTLE_MAP_SURFACE_BOOK:
    case BATTLE_MAP_SURFACE_TREE:
    case BATTLE_MAP_SURFACE_BOX:
    case BATTLE_MAP_SURFACE_BRICK:
    case BATTLE_MAP_SURFACE_CHIMNEY:
    case BATTLE_MAP_SURFACE_MUD_WALL:
    case BATTLE_MAP_SURFACE_BRIDGE:
    case BATTLE_MAP_SURFACE_STAIRS:
    case BATTLE_MAP_SURFACE_DECK:
    case BATTLE_MAP_SURFACE_MACHINE:
    case BATTLE_MAP_SURFACE_IRON_PLATE:
    case BATTLE_MAP_SURFACE_TOMBSTONE:
    case BATTLE_MAP_SURFACE_COFFIN:
        effect_secondary_fall_dust_start(unit);
        break;
    case BATTLE_MAP_SURFACE_SWAMP:
    case BATTLE_MAP_SURFACE_MARSH:
    case BATTLE_MAP_SURFACE_POISONED_MARSH:
    case BATTLE_MAP_SURFACE_WATERWAY:
    case BATTLE_MAP_SURFACE_RIVER:
    case BATTLE_MAP_SURFACE_LAKE:
    case BATTLE_MAP_SURFACE_SEA:
    case BATTLE_MAP_SURFACE_WATERFALL:
        effect_secondary_splash_start(unit);
        break;
    }
}

/*
 * Resolves the terrain records for a moving unit's current tile and the tile
 * one step away, as encoded by current_path_step: bits 6-7 select the axis and
 * sign, bit 5 selects the map layer and bits 0-1 add extra distance.
 */
static void move_step_get_current_and_destination_tiles(
    unit_runtime_t* unit, battle_map_tile_t** out_current, battle_map_tile_t** out_target) {
    u8 movement;
    u32 flags;
    s32 dir;
    s32 x;
    s32 y;
    s32 layer;

    x = unit->map_x;
    y = unit->map_y;
    layer = unit->map_layer;
    /* The scalar snapshot preserves the direction switch and layer extraction registers. */
    movement = *(u8*)&unit->current_path_step;
    *out_current = battle_map_tile_get_pointer(x, y, layer);
    flags = movement;
    dir = flags >> 6;
    switch (dir) {
    case MOVE_STEP_DIRECTION_EAST:
        layer = flags >> 5;
        layer &= 1;
        x = unit->map_x;
        x += 1;
        y = unit->map_y;
        x += flags & 3;
        break;
    case MOVE_STEP_DIRECTION_WEST:
        layer = flags >> 5;
        layer &= 1;
        x = unit->map_x;
        x -= 1;
        y = unit->map_y;
        x -= flags & 3;
        break;
    case MOVE_STEP_DIRECTION_SOUTH:
        layer = flags >> 5;
        layer &= 1;
        y = unit->map_y;
        y -= 1;
        x = unit->map_x;
        y -= flags & 3;
        break;
    case MOVE_STEP_DIRECTION_NORTH:
        layer = flags >> 5;
        layer &= 1;
        y = unit->map_y;
        y += 1;
        x = unit->map_x;
        y += flags & 3;
        break;
    default:
        return;
    }
    *out_target = battle_map_tile_get_pointer(x, y, layer);
}

/* Settles a unit on its map tile when its knockback or path move ends, or once it
 * has mounted: the battle stats take the tile and facing, the record is snapped
 * to the tile, the cursor moves to it, the movement path is emptied and the
 * status animation restarts.
 *
 * For a mounted pair the partner is meant to take the same tile, but retail
 * looks it up by the unit's own runtime_id (not mount_partner_runtime_id) and
 * tests the unit's stats rather than the result's, so the second
 * unit_set_tile_position call repeats the first for the same unit. */
static void move_settle_on_tile(unit_runtime_t* unit) {
    unit_stats_t* stats = unit->stats;
    unit_runtime_t* partner;

    if (stats != 0) {
        /* facing is read signed (lh) here. */
        unit_set_tile_position(
            stats->battle_id, unit->map_x, unit->map_y, unit->map_layer, unit->facing / UNIT_FACING_QUARTER_TURN);
        if (unit->mount_state != UNIT_MOUNT_STATE_NONE) {
            partner = unit_runtime_get(unit->runtime_id);
            if (unit->stats != 0) {
                unit_set_tile_position(partner->stats->battle_id, unit->map_x, unit->map_y, unit->map_layer,
                    unit->facing / UNIT_FACING_QUARTER_TURN);
            }
        }
    }
    unit_set_move_and_world_coords(unit);
    unit_height_calculate_from_world_coords(unit);
    cursor_move_to_unit(unit);
    unit->path.count = 0;
    animation_status_set(unit);
}

/*
 * Mount the unit onto its pending mount.  Bit 0x80 of pending_mount selects an
 * explicit battle id in the low seven bits; otherwise the mount is whatever
 * chocobo already stands on the unit's tile, and failing that the unit is
 * itself the mount for a rider recorded at mount_partner_runtime_id.
 */
void unit_mount_onto_pending(unit_runtime_t* unit) {
    unit_runtime_t* other;

    move_settle_on_tile(unit);
    if (unit->path.pending_mount & MOVE_PENDING_MOUNT_SET) {
        other = unit_battle_get(unit->path.pending_mount & MOVE_PENDING_MOUNT_ID_MASK);
        if (other != 0) {
            unit_rider_seat(unit, other);
            unit_sfx_play(unit, SOUND_SFX_LAND_DEFAULT);
        }
        return;
    }

    other = unit_tile_other_chocobo_find(unit->map_x, unit->map_y, unit->map_layer);
    if (other != 0) {
        if (other != unit) {
            unit_rider_seat(unit, other);
        }
        return;
    }

    if (unit->mount_state == UNIT_MOUNT_STATE_MOUNT) {
        other = unit_runtime_get(unit->mount_partner_runtime_id);
        if (other != 0) {
            unit_rider_seat(other, unit);
        }
    }
}

/* Place a unit that shares a tile with a mover delta world units from the point
 * (x, y) along g_move_displacement_direction, at the ground height there,
 * and mark its shadow for redrawing.
 *
 * y is the map depth axis (world_coords.vz). Dead, crystal, treasure and jumping units
 * are left where they are. */
static void directional_unit_displace(unit_runtime_t* unit, s32 x, s32 y, s32 delta) {
    s16 axis;
    s16 standing_world_y;

    if ((unit->status_flags_1_4 & UNIT_STATUS_DEAD) != 0) {
        return;
    }
    if ((unit->status_flags_5_6 & (UNIT_STATUS_CRYSTAL | UNIT_STATUS_TREASURE | UNIT_STATUS_JUMP)) != 0) {
        return;
    }
    switch (g_move_displacement_direction) {
    case MOVE_DIRECTION_SOUTH:
        axis = y - delta;
        unit->world_coords.vz = axis;
        unit->real.vz = axis * ONE;
        break;
    case MOVE_DIRECTION_NORTH:
        axis = y + delta;
        unit->world_coords.vz = axis;
        unit->real.vz = axis * ONE;
        break;
    case MOVE_DIRECTION_EAST:
        axis = x + delta;
        unit->world_coords.vx = axis;
        unit->real.vx = axis * ONE;
        break;
    case MOVE_DIRECTION_WEST:
        axis = x - delta;
        unit->world_coords.vx = axis;
        unit->real.vx = axis * ONE;
        break;
    }
    standing_world_y = unit_height_calculate_from_world_coords(unit);
    unit->world_coords.vy = standing_world_y;
    unit->real.vy = standing_world_y * ONE;
    unit->shadow_dirty |= 1;
}

/* Pushes a unit sharing the mover's tile aside as the mover steps off-center.
 *
 * The other unit is set (7 - d) * 3 / 2 world units off its own tile center,
 * where d is how far the mover is past its tile center in `direction` (at most
 * 7), so the push fades out as the mover leaves. The push direction is
 * whatever move_step_displace_unit_at_destination_tile last left in
 * g_move_displacement_direction. Nothing is pushed while the mover's
 * step before last was onto a unit (the step's mount bit). The case order and
 * per-case center temporaries reproduce the target's cross-jumped tails. */
static void overlapping_unit_displace(unit_runtime_t* unit, s32 direction) {
    unit_runtime_t* other = unit_find_overlapping_at_tile(unit, unit->map_x, unit->map_y, unit->map_layer);
    s32 dist;
    s32 x;
    s32 y;
    s32 center;

    if (unit->movement_path_offset < 2) {
        return;
    }
    if (unit->path.data.steps[unit->movement_path_offset - 2].mount) {
        return;
    }
    if (other == 0) {
        return;
    }
    x = other->map_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
    y = other->map_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
    switch (direction) {
    case MOVE_DIRECTION_NORTH:
        center = unit->map_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        dist = unit->world_coords.vz - center;
        break;
    case MOVE_DIRECTION_SOUTH:
        center = unit->map_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        dist = center - unit->world_coords.vz;
        break;
    case MOVE_DIRECTION_WEST:
        center = unit->map_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        dist = center - unit->world_coords.vx;
        break;
    case MOVE_DIRECTION_EAST:
        center = unit->map_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        dist = unit->world_coords.vx - center;
        break;
    }
    if (dist >= 8) {
        dist = 7;
    }
    dist -= 7;
    directional_unit_displace(other, x, y, -dist * 3 / 2);
}

/* Pushes a unit sharing the mover's destination tile aside, mirroring
 * overlapping_unit_displace but keyed on the pending step's
 * destination tile instead of the unit's current map square. The trailing
 * loop picks the push direction in g_move_displacement_direction: the
 * first direction off the path's axis on a straight step, or on a turn the
 * first one that reverses neither the current nor the next step.
 *
 * The case order and the reused `dist` temporary reproduce the target's
 * cross-jumped tails and its s1 allocation; spelling the second path index as
 * offset + base reproduces the offset-before-base address add. */
static void move_step_displace_unit_at_destination_tile(unit_runtime_t* unit, s32 direction) {
    unit_runtime_t* other;
    s32 dist;
    s32 x;
    s32 y;
    s32 center;
    s32 next;

    if (unit->movement_path_offset == unit->path.count) {
        return;
    }
    other = unit_find_overlapping_at_tile(
        unit, unit->movement.destination_x, unit->movement.destination_y, unit->movement.destination_layer);
    if (unit->movement_path_offset == 0) {
        return;
    }
    if (unit->path.data.steps[unit->movement_path_offset - 1].mount) {
        return;
    }
    if (other == 0) {
        return;
    }
    x = other->movement.destination_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
    y = other->movement.destination_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
    switch (direction) {
    case MOVE_DIRECTION_NORTH:
        center = unit->movement.destination_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        dist = center - unit->world_coords.vz;
        break;
    case MOVE_DIRECTION_SOUTH:
        center = unit->movement.destination_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        dist = unit->world_coords.vz - center;
        break;
    case MOVE_DIRECTION_WEST:
        center = unit->movement.destination_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        dist = unit->world_coords.vx - center;
        break;
    case MOVE_DIRECTION_EAST:
        center = unit->movement.destination_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        dist = center - unit->world_coords.vx;
        break;
    }
    if (dist >= 8) {
        dist = 7;
    }
    dist -= 7;
    dist = -dist * 3 / 2;
    switch ((unit->movement_path_offset + unit->path.data.steps)->direction) {
    case MOVE_STEP_DIRECTION_EAST:
        next = MOVE_DIRECTION_EAST;
        break;
    case MOVE_STEP_DIRECTION_WEST:
        next = MOVE_DIRECTION_WEST;
        break;
    case MOVE_STEP_DIRECTION_SOUTH:
        next = MOVE_DIRECTION_SOUTH;
        break;
    case MOVE_STEP_DIRECTION_NORTH:
        next = MOVE_DIRECTION_NORTH;
        break;
    }
    for (g_move_displacement_direction = 0; g_move_displacement_direction < 4; g_move_displacement_direction++) {
        if (direction == next) {
            if ((g_move_displacement_direction != next) && (g_move_displacement_direction != (next ^ 2))) {
                break;
            }
        } else {
            if ((g_move_displacement_direction != (direction ^ 2)) && (g_move_displacement_direction != (next ^ 2))) {
                break;
            }
        }
    }
    directional_unit_displace(other, x, y, dist);
}

/* Travel direction of the step in progress (move_direction_e), from the
 * unit's tile to its destination tile. A difference in y decides before one in x,
 * and the result is NORTH when the two tiles are the same. */
static move_direction_e move_step_get_direction(const unit_runtime_t* unit) {
    move_direction_e direction;

    if (unit->map_y < unit->movement.destination_y) {
        direction = MOVE_DIRECTION_NORTH;
    } else if (unit->movement.destination_y < unit->map_y) {
        direction = MOVE_DIRECTION_SOUTH;
    } else if (unit->map_x > unit->movement.destination_x) {
        direction = MOVE_DIRECTION_WEST;
    } else if (unit->map_x < unit->movement.destination_x) {
        direction = MOVE_DIRECTION_EAST;
    } else {
        direction = MOVE_DIRECTION_NORTH;
    }
    return direction;
}

/* Continue a jump step on the ground once the landing animation is over: restart
 * the step (at climb speed when the step byte's climb bit is set) and enter its
 * to-center stage for the direction of travel. */
static void center_motion_resume(unit_runtime_t* unit) {
    s32 direction = move_step_get_direction(unit);

    switch (g_move_step_value.climb) {
    case 0:
        move_step_start_walk(unit, g_move_step_current_tile, g_move_step_destination_tile);
        unit->step_phase = g_walk_to_center_phases[direction];
        break;
    case 1:
        move_step_start_climb(unit, g_move_step_current_tile, g_move_step_destination_tile);
        unit->step_phase = g_climb_to_center_phases[direction];
        break;
    }
}

/* Step handler of the JUMP_LANDING phases: once ANIMATION_LAND has
 * run out (or another animation plays), continue the step toward the tile center. */
static void move_step_update_jump_landing(unit_runtime_t* unit) {
    if (((unit->animation.sequence_id >> 1) != ANIMATION_LAND) || (unit->animation.wait == 0)) {
        center_motion_resume(unit);
    }
}

/* Whether a walking unit has reached the near edge of its movement destination
 * tile in the given direction.  Tiles are 28 world units wide and the test
 * keeps a 7-unit margin. */
static s32 move_step_has_reached_entry_edge(s32 direction, const unit_runtime_t* unit) {
    switch (direction) {
    case MOVE_DIRECTION_NORTH:
        return unit->world_coords.vz >= ((unit->movement.destination_y * BATTLE_MAP_TILE_SIZE) + 7);
    case MOVE_DIRECTION_SOUTH:
        return (((unit->movement.destination_y + 1) * BATTLE_MAP_TILE_SIZE) - 7) >= unit->world_coords.vz;
    case MOVE_DIRECTION_WEST:
        return (((unit->movement.destination_x + 1) * BATTLE_MAP_TILE_SIZE) - 7) >= unit->world_coords.vx;
    case MOVE_DIRECTION_EAST:
        return unit->world_coords.vx >= ((unit->movement.destination_x * BATTLE_MAP_TILE_SIZE) + 7);
    }
}

/*
 * Step handler of the JUMP_ASCENT and JUMP_DESCENT phases: carry a jumping unit
 * to the entry edge of its destination tile, where its horizontal motion stops,
 * and land it once it is down at the ground height there.
 *
 * A hop (either hop animation) landing slower than 0x6000 runs straight on into
 * the step's to-center stage, with the landing sound and effect only when it
 * comes down below the surface of a tile with depth (water). Any other landing
 * enters the JUMP_LANDING phase for two frames with the land animation (kept
 * off during action execution), plus the soft landing sound on a unit or the
 * surface's landing sound and effect.
 */
static void move_step_update_jump_flight(unit_runtime_t* unit) {
    s32 direction;
    s16 target_world_y;
    battle_map_tile_t* tile;

    sprite_unit_frame_load(unit, &unit->animation, unit->animation.frame, unit->animation.sequence_id);
    direction = move_step_get_direction(unit);
    move_step_apply_velocity(unit);
    if (g_battle_state != BATTLE_STATE_EVENT) {
        move_step_displace_unit_at_destination_tile(unit, direction);
    }
    if (move_step_has_reached_entry_edge(direction, unit) == 0) {
        return;
    }
    unit->velocity.vz = 0;
    unit->velocity.vx = 0;
    target_world_y = unit_height_calculate_from_world_coords(unit);
    g_move_step_target_world_y = target_world_y;
    if (unit->world_coords.vy < target_world_y) {
        return;
    }
    /* ANIMATION_HOP or ANIMATION_HOP_ALTERNATE. */
    if (((unit->animation.sequence_id >> 1) - ANIMATION_HOP) < 2U) {
        if (unit->velocity.vy < 0x6000) {
            unit->velocity.vy = 0;
            switch (g_move_step_value.climb) {
            case 0:
                move_step_start_walk(unit, g_move_step_current_tile, g_move_step_destination_tile);
                unit->step_phase = g_walk_to_center_phases[direction];
                break;
            case 1:
                move_step_start_climb(unit, g_move_step_current_tile, g_move_step_destination_tile);
                unit->step_phase = g_climb_to_center_phases[direction];
                break;
            }
            tile = battle_map_tile_get_pointer(
                unit->movement.destination_x, unit->movement.destination_y, unit->movement.destination_layer);
            if (tile->depth_half_height.depth == 0) {
                return;
            }
            if (-unit->world_coords.vy >= (tile->height + tile->depth_half_height.depth) * 12) {
                return;
            }
            landing_sfx_play(unit);
            landing_secondary_set(unit);
            return;
        }
        unit->velocity.vy = 0;
        unit->state_frame_counter = 2;
        unit->step_phase = g_jump_landing_phases[direction];
        if (g_battle_state != BATTLE_STATE_ACTION_EXECUTE) {
            animation_facing_store(ANIMATION_LAND, unit->facing, unit);
        }
        if (g_move_step_value.mount) {
            unit_sfx_play(unit, SOUND_SFX_LAND_SOFT);
            return;
        }
        landing_sfx_play(unit);
        landing_secondary_set(unit);
        return;
    }
    unit->velocity.vy = 0;
    unit->state_frame_counter = 2;
    unit->step_phase = g_jump_landing_phases[direction];
    if (g_battle_state != BATTLE_STATE_ACTION_EXECUTE) {
        animation_facing_store(ANIMATION_LAND, unit->facing, unit);
    }
    if (g_move_step_value.mount) {
        unit_sfx_play(unit, SOUND_SFX_LAND_SOFT);
        return;
    }
    landing_sfx_play(unit);
    landing_secondary_set(unit);
}

/* Step handler of the JUMP_ASCENT phases. Once the unit's vertical velocity is
 * no longer negative it enters the JUMP_DESCENT phase on the destination
 * level and, for a jump onto a unit (the step's mount bit), records the unit
 * on the destination tile as the one it stands on (battle id plus one). */
static void move_step_update_jump_ascent(unit_runtime_t* unit) {
    unit_runtime_t* occupant;
    s32 direction;

    sprite_unit_frame_load(unit, &unit->animation, unit->animation.frame, unit->animation.sequence_id);
    direction = move_step_get_direction(unit);
    if (g_battle_state != BATTLE_STATE_EVENT) {
        overlapping_unit_displace(unit, direction);
    }
    if (unit->velocity.vy >= 0) {
        unit->step_phase = g_jump_descent_phases[direction];
        unit->map_layer = unit->movement.destination_layer;
        if (g_move_step_value.mount) {
            occupant = unit_find_runtime_at_tile(
                unit->movement.destination_x, unit->movement.destination_y, unit->movement.destination_layer);
            g_move_step_tile_occupant = occupant;
            if (occupant != 0) {
                unit->standing_on_battle_id_plus_one = occupant->stats->battle_id + 1;
                return;
            }
        }
        unit->standing_on_battle_id_plus_one = 0;
    }
}

/*
 * Report whether a walking unit has reached the center of its destination tile
 * along the given facing.  Tile centers are 28 world units apart with a
 * half-tile offset of 14.
 */
static s32 move_step_has_reached_center(s32 direction, const unit_runtime_t* unit) {
    switch (direction) {
    case MOVE_DIRECTION_NORTH:
        return unit->world_coords.vz
            >= unit->movement.destination_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
    case MOVE_DIRECTION_SOUTH:
        return unit->movement.destination_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2)
            >= unit->world_coords.vz;
    case MOVE_DIRECTION_WEST:
        return unit->movement.destination_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2)
            >= unit->world_coords.vx;
    case MOVE_DIRECTION_EAST:
        return unit->world_coords.vx
            >= unit->movement.destination_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
    }
}

enum { TILE_EDGE_MARGIN = 7, REAL_COORD_SHIFT = 12 };

/*
 * Snaps the unit's world and real coordinate on the axis of travel to
 * TILE_EDGE_MARGIN inside the exit edge of its current tile, the point where
 * move_step_has_reached_exit_edge turns true; the counterpart of
 * move_step_snap_axis_to_center for map_x/map_y.
 */
static void move_step_snap_axis_to_exit_edge(s32 direction, unit_runtime_t* unit) {
    s16 coordinate;

    switch (direction) {
    case MOVE_DIRECTION_NORTH:
        coordinate = (unit->map_y + 1) * BATTLE_MAP_TILE_SIZE - TILE_EDGE_MARGIN;
        unit->world_coords.vz = coordinate;
        unit->real.vz = coordinate << REAL_COORD_SHIFT;
        return;
    case MOVE_DIRECTION_SOUTH:
        coordinate = unit->map_y * BATTLE_MAP_TILE_SIZE + TILE_EDGE_MARGIN;
        unit->world_coords.vz = coordinate;
        unit->real.vz = coordinate << REAL_COORD_SHIFT;
        return;
    case MOVE_DIRECTION_WEST:
        coordinate = unit->map_x * BATTLE_MAP_TILE_SIZE + TILE_EDGE_MARGIN;
        unit->world_coords.vx = coordinate;
        unit->real.vx = coordinate << REAL_COORD_SHIFT;
        return;
    case MOVE_DIRECTION_EAST:
        coordinate = (unit->map_x + 1) * BATTLE_MAP_TILE_SIZE - TILE_EDGE_MARGIN;
        unit->world_coords.vx = coordinate;
        unit->real.vx = coordinate << REAL_COORD_SHIFT;
        return;
    }
}

enum { TILE_CENTER_OFFSET = BATTLE_MAP_TILE_SIZE / 2 };

/*
 * Snaps the unit's world and real coordinate on the axis of travel to the
 * center of its movement destination tile: north and south set the map-depth
 * axis (vz), east and west the x axis.
 */
static void move_step_snap_axis_to_center(s32 direction, unit_runtime_t* unit) {
    s16 coordinate;

    switch (direction) {
    case MOVE_DIRECTION_NORTH:
    case MOVE_DIRECTION_SOUTH:
        coordinate = unit->movement.destination_y * BATTLE_MAP_TILE_SIZE + TILE_CENTER_OFFSET;
        unit->world_coords.vz = coordinate;
        unit->real.vz = coordinate << REAL_COORD_SHIFT;
        return;
    case MOVE_DIRECTION_EAST:
    case MOVE_DIRECTION_WEST:
        coordinate = unit->movement.destination_x * BATTLE_MAP_TILE_SIZE + TILE_CENTER_OFFSET;
        unit->world_coords.vx = coordinate;
        unit->real.vx = coordinate << REAL_COORD_SHIFT;
        return;
    }
}

/* Step handler of the WALK_TO_CENTER and CLIMB_TO_CENTER stages (and of the
 * knockback step): advance the unit one frame toward the center of its
 * destination tile.
 *
 * Outside events it pushes aside a unit on the destination tile. At the center
 * the horizontal motion stops; once the unit is also down at the ground height
 * there, it takes the destination tile as its map position and the step ends. */
static void move_step_update_walk_to_center(unit_runtime_t* unit) {
    s32 direction = move_step_get_direction(unit);
    s16 standing_world_y;

    move_step_apply_velocity(unit);
    if (g_battle_state != BATTLE_STATE_EVENT) {
        move_step_displace_unit_at_destination_tile(unit, direction);
    }
    if (move_step_has_reached_center(direction, unit) != 0) {
        move_step_snap_axis_to_center(direction, unit);
        unit->velocity.vz = 0;
        unit->velocity.vx = 0;
        standing_world_y = unit_height_calculate_from_world_coords(unit);
        g_move_step_target_world_y = standing_world_y;
        if (unit->world_coords.vy >= standing_world_y) {
            unit->map_x = unit->movement.destination_x;
            unit->map_y = unit->movement.destination_y;
            unit->velocity.vy = 0;
            unit->real.vy = g_move_step_target_world_y * ONE;
            unit->world_coords.vy = unit->real.vy / ONE;
            unit->step_phase = MOVE_STEP_PHASE_NONE;
            unit->shadow_dirty |= 1;
        }
    }
}

/* Whether a moving unit has come within 7 units of the far edge of the tile it occupies in
 * direction (move_direction_e). */
static s32 move_step_has_reached_exit_edge(s32 direction, const unit_runtime_t* unit) {
    switch (direction) {
    case MOVE_DIRECTION_NORTH:
        return unit->world_coords.vz >= (((unit->map_y + 1) * BATTLE_MAP_TILE_SIZE) - 7);
    case MOVE_DIRECTION_SOUTH:
        return ((unit->map_y * BATTLE_MAP_TILE_SIZE) + 7) >= unit->world_coords.vz;
    case MOVE_DIRECTION_WEST:
        return ((unit->map_x * BATTLE_MAP_TILE_SIZE) + 7) >= unit->world_coords.vx;
    case MOVE_DIRECTION_EAST:
        return unit->world_coords.vx >= (((unit->map_x + 1) * BATTLE_MAP_TILE_SIZE) - 7);
    }
}

/* Step handler of the WALK_TO_EDGE and CLIMB_TO_EDGE stages: advance a walking
 * unit one frame toward the exit edge of its tile, pushing aside a unit that
 * shares the tile outside events.
 *
 * At the edge the rest of the step is planned. A step that spans extra tiles
 * (a nonzero length) starts a jump step. Otherwise the heights of the two
 * edges are compared in half-steps of 6 world units: the current one is the
 * top of the unit the mover stands on, or the tile edge height plus any water
 * lift; the destination one is the top of the unit on the destination tile for
 * a step onto a unit, or the tile edge height. A rise of at most one half-step
 * continues as a walk (WALK_TO_CENTER) or climb-speed step (CLIMB_TO_CENTER),
 * up to seven starts climb_hop_start, and anything higher
 * climb_jump_start. An occupant whose height is
 * UNIT_HEIGHT_UNAVAILABLE counts as the bare tile. */
static void move_step_update_walk_to_edge(unit_runtime_t* unit) {
    s32 direction = move_step_get_direction(unit);
    u8 standing_on;

    move_step_apply_velocity(unit);
    if (g_battle_state != BATTLE_STATE_EVENT) {
        overlapping_unit_displace(unit, direction);
    }
    if (move_step_has_reached_exit_edge(direction, unit) == 0) {
        return;
    }
    move_step_snap_axis_to_exit_edge(direction, unit);
    if (g_move_step_value.length == 0) {
        standing_on = unit->standing_on_battle_id_plus_one;
        if (standing_on != 0) {
            unit_action_height_calculate(&g_move_step_tile_occupant_height, standing_on - 1);
            if (g_move_step_tile_occupant_height.unit_flags & UNIT_HEIGHT_UNAVAILABLE) {
                g_move_step_current_edge_height = g_move_step_current_tile->height * 2
                    + g_move_step_current_tile->depth_half_height.half_height * unit->current_edge_scale;
            } else {
                g_move_step_current_edge_height = g_move_step_tile_occupant_height.total_height;
            }
        } else {
            g_move_step_current_edge_height = g_move_step_current_tile->height * 2
                + g_move_step_current_tile->depth_half_height.half_height * unit->current_edge_scale;
            g_move_step_current_edge_height += move_get_water_standing_offset(unit, g_move_step_current_tile);
        }

        if (g_move_step_value.mount) {
            g_move_step_tile_occupant = unit_find_runtime_at_tile(
                unit->movement.destination_x, unit->movement.destination_y, unit->movement.destination_layer);
            unit->standing_on_battle_id_plus_one = g_move_step_tile_occupant->stats->battle_id + 1;
            unit_action_height_calculate(
                &g_move_step_tile_occupant_height, g_move_step_tile_occupant->stats->battle_id);
            if (g_move_step_tile_occupant_height.unit_flags & UNIT_HEIGHT_UNAVAILABLE) {
                g_move_step_destination_edge_height = g_move_step_destination_tile->height * 2
                    + g_move_step_destination_tile->depth_half_height.half_height * unit->destination_edge_scale;
            } else {
                g_move_step_destination_edge_height = g_move_step_tile_occupant_height.total_height;
            }
        } else {
            unit->standing_on_battle_id_plus_one = 0;
            g_move_step_destination_edge_height = g_move_step_destination_tile->height * 2
                + g_move_step_destination_tile->depth_half_height.half_height * unit->destination_edge_scale;
            /* Retail adds the destination's water lift to the current height, not to the
             * destination height, so a step up onto deep water can pass as a plain step. */
            g_move_step_current_edge_height += move_get_water_standing_offset(unit, g_move_step_destination_tile);
        }

        if (g_move_step_current_edge_height + 1 >= g_move_step_destination_edge_height) {
            unit_rider_dismount_display_update(unit);
            switch (g_move_step_value.climb) {
            case 0:
                move_step_start_walk(unit, g_move_step_current_tile, g_move_step_destination_tile);
                unit->step_phase = MOVE_STEP_PHASE_WALK_TO_CENTER_NORTH;
                return;
            case 1:
                move_step_start_climb(unit, g_move_step_current_tile, g_move_step_destination_tile);
                unit->step_phase = MOVE_STEP_PHASE_CLIMB_TO_CENTER_NORTH;
                return;
            }
            return;
        }

        if (g_move_step_current_edge_height + 7 >= g_move_step_destination_edge_height) {
            unit_rider_dismount(unit);
            unit->step_speed = 0x2000;
            climb_hop_start(unit, g_move_step_current_tile, g_move_step_destination_tile);
            unit->standing_on_battle_id_plus_one = 0;
            return;
        }
        unit->step_speed = 0x2000;
        climb_jump_start(unit, g_move_step_current_tile, g_move_step_destination_tile);
        unit->standing_on_battle_id_plus_one = 0;
    } else {
        unit->step_speed = 0x2000;
        jump_start(unit, g_move_step_current_tile, g_move_step_destination_tile);
        unit->standing_on_battle_id_plus_one = 0;
    }
}

/*
 * Step handler of the FLOAT_TO_EDGE stages: advance a floating unit one frame
 * toward its current tile's exit edge. At the edge the two tiles' edge heights
 * decide: a rise of at most one half-step continues as a float step into the
 * FLOAT_TO_CENTER stage, anything higher starts a float jump
 * (float_jump_start) at step speed 0x2000.
 */
static void move_step_update_float_to_edge(unit_runtime_t* unit) {
    s32 direction = move_step_get_direction(unit);

    move_step_apply_velocity(unit);
    if (g_battle_state != BATTLE_STATE_EVENT) {
        overlapping_unit_displace(unit, direction);
    }
    if (move_step_has_reached_exit_edge(direction, unit) != 0) {
        move_step_snap_axis_to_exit_edge(direction, unit);
        g_move_step_current_edge_height = g_move_step_current_tile->height * 2
            + g_move_step_current_tile->depth_half_height.half_height * unit->current_edge_scale;
        unit->standing_on_battle_id_plus_one = 0;
        g_move_step_destination_edge_height = g_move_step_destination_tile->height * 2
            + g_move_step_destination_tile->depth_half_height.half_height * unit->destination_edge_scale;
        if (g_move_step_current_edge_height + 1 >= g_move_step_destination_edge_height) {
            move_step_start_float(unit, g_move_step_current_tile, g_move_step_destination_tile);
            unit->step_phase = g_move_step_float_to_center_phases[direction];
        } else {
            unit->step_speed = 0x2000;
            float_jump_start(unit, g_move_step_current_tile, g_move_step_destination_tile);
        }
    }
}

/* Step handler of the FLOAT_TO_CENTER stages: advance a floating unit one frame
 * toward the center of its destination tile. The original wrote the body of
 * move_step_update_walk_to_center a second time for it,
 * differing only in the push.
 *
 * Outside events it pushes aside a unit sharing its current tile, where the
 * walking twin pushes one on the destination tile. At the center the horizontal
 * motion stops; once the unit is also down at the ground height there, it takes
 * the destination tile as its map position and the step ends. */
static void move_step_update_float_to_center(unit_runtime_t* unit) {
    s32 direction = move_step_get_direction(unit);
    s16 standing_world_y;

    move_step_apply_velocity(unit);
    if (g_battle_state != BATTLE_STATE_EVENT) {
        overlapping_unit_displace(unit, direction);
    }
    if (move_step_has_reached_center(direction, unit) != 0) {
        move_step_snap_axis_to_center(direction, unit);
        unit->velocity.vz = 0;
        unit->velocity.vx = 0;
        standing_world_y = unit_height_calculate_from_world_coords(unit);
        g_move_step_target_world_y = standing_world_y;
        if (unit->world_coords.vy >= standing_world_y) {
            unit->map_x = unit->movement.destination_x;
            unit->map_y = unit->movement.destination_y;
            unit->velocity.vy = 0;
            unit->real.vy = g_move_step_target_world_y * ONE;
            unit->world_coords.vy = unit->real.vy / ONE;
            unit->step_phase = MOVE_STEP_PHASE_NONE;
            unit->shadow_dirty |= 1;
        }
    }
}

/* Step handler of the FLOAT_JUMP phases: carry a floating unit to the entry edge of its
 * destination tile, where its horizontal motion stops; once it has come down to the tile's height,
 * start the float step into the matching FLOAT_TO_CENTER phase. */
static void float_jump_update(unit_runtime_t* unit) {
    s32 direction = move_step_get_direction(unit);

    move_step_apply_velocity(unit);
    if (g_battle_state != BATTLE_STATE_EVENT) {
        move_step_displace_unit_at_destination_tile(unit, direction);
    }
    if (move_step_has_reached_entry_edge(direction, unit) != 0) {
        unit->velocity.vz = 0;
        unit->velocity.vx = 0;
        if (unit->world_coords.vy >= (s16)unit_height_calculate_from_world_coords(unit)) {
            unit->velocity.vy = 0;
            move_step_start_float(unit, g_move_step_current_tile, g_move_step_destination_tile);
            unit->step_phase = g_move_step_float_to_center_phases[direction];
        }
    }
}

/* Step handler of the FLY_TO_EDGE stages: advance a flying unit one frame toward
 * the exit edge of its tile.
 *
 * At the edge it stores the edge heights of the current and destination tiles
 * (height * 2 + slope half-height * edge value), clears the unit it stood on,
 * builds the velocity for the run of contiguous steps and enters the
 * FLY_TO_CENTER stage. */
static void fly_to_edge_update(unit_runtime_t* unit) {
    s32 direction = move_step_get_direction(unit);

    move_step_apply_velocity(unit);
    if (move_step_has_reached_exit_edge(direction, unit) != 0) {
        move_step_snap_axis_to_exit_edge(direction, unit);
        g_move_step_current_edge_height = g_move_step_current_tile->height * 2
            + g_move_step_current_tile->depth_half_height.half_height * unit->current_edge_scale;
        unit->standing_on_battle_id_plus_one = 0;
        g_move_step_destination_edge_height = g_move_step_destination_tile->height * 2
            + g_move_step_destination_tile->depth_half_height.half_height * unit->destination_edge_scale;
        move_step_set_velocity_with_final_height(unit, &unit->path, &unit->current_path_step);
        move_step_scale_velocity(unit, unit->walk_speed);
        unit->step_phase = MOVE_STEP_PHASE_FLY_TO_CENTER_NORTH;
    }
}

/*
 * Step handler of the FLY_TO_CENTER stages: advance a flying unit onto its destination tile center.
 *
 * Once the tile center is reached, the target height is the tile's world Y,
 * or 10 above the mount's when the path is complete and pending_mount bit 0x80
 * names a mount. The step ends when the unit reaches that height in its
 * direction of vertical travel (velocity.vy) or when path steps remain; it
 * then snaps to the center, clears velocity, adopts the destination map
 * coordinates and sets shadow_dirty bit 0.
 */
static void fly_to_center_update(unit_runtime_t* unit) {
    s32 done = 0;
    s32 direction = move_step_get_direction(unit);
    unit_runtime_t* mount;

    move_step_apply_velocity(unit);
    if (move_step_has_reached_center(direction, unit) != 0) {
        unit->velocity.vz = 0;
        unit->velocity.vx = 0;
        g_move_step_target_world_y = unit_height_calculate_from_world_coords(unit);
        if (unit->movement_path_offset == unit->path.count && (unit->path.pending_mount & MOVE_PENDING_MOUNT_SET)) {
            mount = unit_battle_get(unit->path.pending_mount & MOVE_PENDING_MOUNT_ID_MASK);
            if (mount != 0) {
                g_move_step_target_world_y = unit_height_calculate_from_world_coords(mount) - 10;
            }
        }
        if (unit->velocity.vy > 0) {
            if (unit->world_coords.vy >= g_move_step_target_world_y) {
                done = 1;
            }
        } else if (unit->world_coords.vy <= g_move_step_target_world_y) {
            done = 1;
        }
        if (unit->movement_path_offset != unit->path.count) {
            done = 1;
        }
        if (done) {
            unit->velocity.vy = 0;
            move_step_snap_axis_to_center(direction, unit);
            unit->velocity.vz = 0;
            unit->velocity.vx = 0;
            g_move_step_target_world_y = unit_height_calculate_from_world_coords(unit);
            unit->step_phase = MOVE_STEP_PHASE_NONE;
            unit->map_x = unit->movement.destination_x;
            unit->map_y = unit->movement.destination_y;
            unit->shadow_dirty |= 1;
        }
    }
}

/* Store the edge heights the unit's next step crosses: the side of its current
 * tile it leaves and the side of the destination tile it enters, each a 2-bit
 * field of slope_type (bits 0-1 east, 2-3 west, 4-5 south, 6-7 north; 0 low,
 * 1 sloping, 2 high). Both go to the unit's per-step scales and to
 * g_move_step_current_edge_height and g_move_step_destination_edge_height.
 * A unit standing on another unit leaves at edge height 0, and a step onto a
 * unit (the step's mount bit) enters at 0. The step tiles are reloaded first
 * and the unit's palette follows the destination tile's shading. */
static void slope_scales_set(unit_runtime_t* unit) {
    move_step_get_current_and_destination_tiles(unit, &g_move_step_current_tile, &g_move_step_destination_tile);
    sprite_palette_check_tile_status_mod(unit, g_move_step_destination_tile->flags.shading, 1);
    switch (g_move_step_value.direction) {
    case MOVE_STEP_DIRECTION_EAST:
        unit->destination_edge_scale = g_move_step_destination_edge_height
            = (g_move_step_destination_tile->slope_type & 0xC) >> 2;
        unit->current_edge_scale = g_move_step_current_edge_height = g_move_step_current_tile->slope_type & 3;
        break;
    case MOVE_STEP_DIRECTION_WEST:
        unit->destination_edge_scale = g_move_step_destination_edge_height
            = g_move_step_destination_tile->slope_type & 3;
        unit->current_edge_scale = g_move_step_current_edge_height = (g_move_step_current_tile->slope_type & 0xC) >> 2;
        break;
    case MOVE_STEP_DIRECTION_SOUTH:
        unit->destination_edge_scale = g_move_step_destination_edge_height
            = g_move_step_destination_tile->slope_type >> 6;
        unit->current_edge_scale = g_move_step_current_edge_height = (g_move_step_current_tile->slope_type & 0x30) >> 4;
        break;
    case MOVE_STEP_DIRECTION_NORTH:
        unit->destination_edge_scale = g_move_step_destination_edge_height
            = (g_move_step_destination_tile->slope_type & 0x30) >> 4;
        unit->current_edge_scale = g_move_step_current_edge_height = g_move_step_current_tile->slope_type >> 6;
        break;
    }
    if (unit->standing_on_battle_id_plus_one != 0) {
        unit->current_edge_scale = g_move_step_current_edge_height = 0;
    }
    if (g_move_step_value.mount) {
        unit->destination_edge_scale = g_move_step_destination_edge_height = 0;
    }
}

/* Step handler of the TELEPORT_ANIMATION phase: once the animation countdown
 * reaches 0, end the path and keep its count in last_path_count. */
static void teleport_animation_update(unit_runtime_t* unit) {
    if (unit->animation.wait == 0) {
        unit->step_phase = MOVE_STEP_PHASE_NONE;
        unit->last_path_count = unit->path.count;
        unit->path.count = 0;
    }
}

/* Step handler of the TELEPORT_START phase. */
static void teleport_start_update(unit_runtime_t* unit) {
    move_step_start_teleport_animation(unit);
}

/* Called for the last step of a path: when the unit has a pending mount, set the
 * step's mount bit (0x10), which marks a step that ends on another unit. */
static void mount_flag_set(unit_runtime_t* unit) {
    if (unit->path.pending_mount == 0) {
        return;
    }
    unit->path.data.steps[unit->movement_path_offset].mount = 1;
}

/*
 * Advance a moving unit along its path by one frame: the full counterpart of
 * move_knockback_update, dispatching every step phase (walk,
 * climb, jump, fly, float and teleport) rather than the knockback subset.
 *
 * The jump-start phases dismount any rider once the jump animation has run
 * out; the two teleport phases play the teleport. When no step is active the
 * path count selects what follows: MOVE_PATH_TELEPORT starts the
 * teleport, MOVE_PATH_TELEPORT_FAILED also clears the pending mount and
 * restarts the AI action, 0 or an exhausted path mounts the pending mount,
 * and otherwise the next path byte starts a step (the path's
 * MOVE_EFFECTIVE_FLY and _FLOAT flags pick the fly and float step
 * starters, and the step's slow bit picks the climb speed).
 */
void move_step_update(unit_runtime_t* unit) {
    u32 offset;
    u8 count;
    u8 flags;

    move_step_get_current_and_destination_tiles(unit, &g_move_step_current_tile, &g_move_step_destination_tile);
    g_move_step_value = unit->current_path_step;
    switch (unit->step_phase) {
    case MOVE_STEP_PHASE_JUMP_START_SOUTH:
    case MOVE_STEP_PHASE_JUMP_START_EAST:
    case MOVE_STEP_PHASE_JUMP_START_NORTH:
    case MOVE_STEP_PHASE_JUMP_START_WEST:
        if (unit->animation.wait == 0) {
            unit->step_phase++;
            animation_facing_store(ANIMATION_FALL, unit->facing, unit);
            unit_sfx_play(unit, SOUND_SFX_JUMP);
            unit_rider_dismount(unit);
        }
        break;
    case MOVE_STEP_PHASE_JUMP_LANDING_SOUTH:
    case MOVE_STEP_PHASE_JUMP_LANDING_EAST:
    case MOVE_STEP_PHASE_JUMP_LANDING_NORTH:
    case MOVE_STEP_PHASE_JUMP_LANDING_WEST:
        move_step_update_jump_landing(unit);
        break;
    case MOVE_STEP_PHASE_JUMP_ASCENT_SOUTH:
    case MOVE_STEP_PHASE_JUMP_ASCENT_EAST:
    case MOVE_STEP_PHASE_JUMP_ASCENT_NORTH:
    case MOVE_STEP_PHASE_JUMP_ASCENT_WEST:
        move_step_update_jump_ascent(unit);
        /* fallthrough */
    case MOVE_STEP_PHASE_JUMP_DESCENT_SOUTH:
    case MOVE_STEP_PHASE_JUMP_DESCENT_EAST:
    case MOVE_STEP_PHASE_JUMP_DESCENT_NORTH:
    case MOVE_STEP_PHASE_JUMP_DESCENT_WEST:
        move_step_update_jump_flight(unit);
        break;
    case MOVE_STEP_PHASE_FLOAT_JUMP_SOUTH:
    case MOVE_STEP_PHASE_FLOAT_JUMP_EAST:
    case MOVE_STEP_PHASE_FLOAT_JUMP_NORTH:
    case MOVE_STEP_PHASE_FLOAT_JUMP_WEST:
        float_jump_update(unit);
        break;
    case MOVE_STEP_PHASE_WALK_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_WEST:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_WEST:
        move_step_update_walk_to_center(unit);
        break;
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_WEST:
        move_step_update_float_to_center(unit);
        break;
    case MOVE_STEP_PHASE_FLY_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_FLY_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_FLY_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_FLY_TO_CENTER_WEST:
        fly_to_center_update(unit);
        break;
    case MOVE_STEP_PHASE_WALK_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_WALK_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_WALK_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_WALK_TO_EDGE_WEST:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_WEST:
        move_step_update_walk_to_edge(unit);
        break;
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_WEST:
        move_step_update_float_to_edge(unit);
        break;
    case MOVE_STEP_PHASE_FLY_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_FLY_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_FLY_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_FLY_TO_EDGE_WEST:
        fly_to_edge_update(unit);
        break;
    case MOVE_STEP_PHASE_TELEPORT_START:
        teleport_start_update(unit);
        break;
    case MOVE_STEP_PHASE_TELEPORT_ANIMATION:
        teleport_animation_update(unit);
        break;
    }

    /* With no step in progress, the path count decides what follows. */
    if (unit->step_phase == MOVE_STEP_PHASE_NONE) {
        count = unit->path.count;
        if (count == MOVE_PATH_TELEPORT) {
            unit_rider_dismount_display_update(unit);
            move_step_start_teleport(unit);
        } else if (count == MOVE_PATH_TELEPORT_FAILED) {
            unit_rider_dismount_display_update(unit);
            move_step_start_teleport(unit);
            unit->path.pending_mount = 0;
            ai_selected_action_init();
        } else if (count == 0) {
            unit_mount_onto_pending(unit);
        } else if (unit->movement_path_offset < count) {
            if (unit->movement_path_offset == count - 1) {
                mount_flag_set(unit);
            }

            /* Take the next step and start it with the starter for the movement type. */
            offset = unit->movement_path_offset;
            unit->movement_path_offset = offset + 1;
            unit->current_path_step = g_move_step_value = unit->path.data.steps[offset];
            slope_scales_set(unit);
            flags = unit->path.flags;
            if (flags & MOVE_EFFECTIVE_FLY) {
                move_step_start_fly(unit, g_move_step_current_tile, g_move_step_destination_tile);
            } else if (flags & MOVE_EFFECTIVE_FLOAT) {
                move_step_start_float(unit, g_move_step_current_tile, g_move_step_destination_tile);
            } else {
                switch (g_move_step_value.slow) {
                case 0:
                    unit->step_speed = 0x2000;
                    move_step_start_walk(unit, g_move_step_current_tile, g_move_step_destination_tile);
                    break;
                case 1:
                    move_step_start_climb(unit, g_move_step_current_tile, g_move_step_destination_tile);
                    break;
                }
            }
        } else {
            unit_mount_onto_pending(unit);
        }
    }
}

/*
 * Advance a knocked-back or path-moving unit by one frame.
 *
 * While a step is in progress (step_phase non-zero) the walk speed
 * decays by a quarter per frame down to ONE and the step phase handler runs.
 * Once no step is active, the next path byte starts a new step, or
 * move_settle_on_tile ends the move when the path is exhausted.
 */
void move_knockback_update(unit_runtime_t* unit) {
    u32 offset;
    s32 speed;

    move_step_get_current_and_destination_tiles(unit, &g_move_step_current_tile, &g_move_step_destination_tile);
    g_move_step_value = unit->current_path_step;
    if (unit->step_phase != MOVE_STEP_PHASE_NONE) {
        speed = unit->walk_speed;
        unit->walk_speed = (speed > ONE) ? speed / 4 * 3 : ONE;
        switch (unit->step_phase) {
        case MOVE_STEP_PHASE_WALK_TO_CENTER_SOUTH:
        case MOVE_STEP_PHASE_WALK_TO_CENTER_EAST:
        case MOVE_STEP_PHASE_WALK_TO_CENTER_NORTH:
        case MOVE_STEP_PHASE_WALK_TO_CENTER_WEST:
            move_step_update_walk_to_center(unit);
            break;
        case MOVE_STEP_PHASE_WALK_TO_EDGE_SOUTH:
        case MOVE_STEP_PHASE_WALK_TO_EDGE_EAST:
        case MOVE_STEP_PHASE_WALK_TO_EDGE_NORTH:
        case MOVE_STEP_PHASE_WALK_TO_EDGE_WEST:
            move_step_update_walk_to_edge(unit);
            break;
        case MOVE_STEP_PHASE_JUMP_LANDING_SOUTH:
        case MOVE_STEP_PHASE_JUMP_LANDING_EAST:
        case MOVE_STEP_PHASE_JUMP_LANDING_NORTH:
        case MOVE_STEP_PHASE_JUMP_LANDING_WEST:
            move_step_update_jump_landing(unit);
            break;
        case MOVE_STEP_PHASE_JUMP_ASCENT_SOUTH:
        case MOVE_STEP_PHASE_JUMP_ASCENT_EAST:
        case MOVE_STEP_PHASE_JUMP_ASCENT_NORTH:
        case MOVE_STEP_PHASE_JUMP_ASCENT_WEST:
            move_step_update_jump_ascent(unit);
            /* fallthrough */
        case MOVE_STEP_PHASE_JUMP_DESCENT_SOUTH:
        case MOVE_STEP_PHASE_JUMP_DESCENT_EAST:
        case MOVE_STEP_PHASE_JUMP_DESCENT_NORTH:
        case MOVE_STEP_PHASE_JUMP_DESCENT_WEST:
            move_step_update_jump_flight(unit);
            break;
        }
        if (unit->step_phase != MOVE_STEP_PHASE_NONE) {
            return;
        }
    }
    if (unit->path.count != 0) {
        offset = unit->movement_path_offset;
        if (offset < unit->path.count) {
            unit->movement_path_offset = offset + 1;
            unit->current_path_step = g_move_step_value = unit->path.data.steps[offset];
            slope_scales_set(unit);
            unit->step_speed = 0x2000;
            move_step_start_walk(unit, g_move_step_current_tile, g_move_step_destination_tile);
        } else {
            move_settle_on_tile(unit);
        }
    }
}

/* Give a knocked-back unit a one-step path to its destination: a plain step in the travel
 * direction with the destination level as its elevation bit, at walk speed 0x8000 (four times the
 * usual 0x2000), which move_knockback_update then slows. */
static void knockback_init(unit_runtime_t* unit) {
    s32 direction;

    unit->movement_path_offset = 0;
    unit->path.count = 1;
    unit->path.flags = move_get_effective_flags(unit->stats);

    unit->path.data.steps[0].length = 0;
    unit->path.data.steps[0].slow = 0;
    unit->path.data.steps[0].climb = 0;
    unit->path.data.steps[0].mount = 0;
    unit->path.data.steps[0].higher = unit->movement.destination_layer & 1;
    direction = move_step_get_direction(unit);

    switch (direction) {
    case MOVE_DIRECTION_NORTH:
        unit->path.data.steps[0].direction = MOVE_STEP_DIRECTION_NORTH;
        break;
    case MOVE_DIRECTION_SOUTH:
        unit->path.data.steps[0].direction = MOVE_STEP_DIRECTION_SOUTH;
        break;
    case MOVE_DIRECTION_WEST:
        unit->path.data.steps[0].direction = MOVE_STEP_DIRECTION_WEST;
        break;
    case MOVE_DIRECTION_EAST:
        unit->path.data.steps[0].direction = MOVE_STEP_DIRECTION_EAST;
        break;
    }

    unit->walk_speed = 0x8000;
}

/* Knockback: copy the destination tile that the attacker's record (src) holds for
 * its target into the target's (dst) movement destination and start the target's
 * one-step knockback path. */
void move_knockback_start(const unit_runtime_t* src, unit_runtime_t* dst) {
    dst->movement.destination_x = src->strike.target_tile.x;
    dst->movement.destination_y = src->strike.target_tile.y;
    dst->movement.destination_layer = src->strike.target_tile.layer;
    knockback_init(dst);
}
