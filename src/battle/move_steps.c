/* Movement steps, including the step-delta setters (each written twice in the original, for units and for floating
 * units), step starts, landing sounds and effects, displacement and tile edge updates. */

#include "fft/animation.h"
#include "fft/battle_map.h"
#include "fft/cursor.h"
#include "fft/effect.h"
#include "fft/effect_secondary.h"
#include "fft/move.h"
#include "fft/move_step.h"
#include "fft/sound.h"
#include "fft/sprite.h"
#include "fft/state.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/libgte.h"
#include "psx/types.h"

static u8 g_move_step_walk_to_center_phases[4] = { MOVE_STEP_PHASE_WALK_TO_CENTER_SOUTH,
    MOVE_STEP_PHASE_WALK_TO_CENTER_EAST, MOVE_STEP_PHASE_WALK_TO_CENTER_NORTH, MOVE_STEP_PHASE_WALK_TO_CENTER_WEST };
static u8 g_move_step_climb_to_center_phases[4] = { MOVE_STEP_PHASE_CLIMB_TO_CENTER_SOUTH,
    MOVE_STEP_PHASE_CLIMB_TO_CENTER_EAST, MOVE_STEP_PHASE_CLIMB_TO_CENTER_NORTH, MOVE_STEP_PHASE_CLIMB_TO_CENTER_WEST };
static u8 g_move_step_jump_descent_phases[4] = { MOVE_STEP_PHASE_JUMP_DESCENT_SOUTH, MOVE_STEP_PHASE_JUMP_DESCENT_EAST,
    MOVE_STEP_PHASE_JUMP_DESCENT_NORTH, MOVE_STEP_PHASE_JUMP_DESCENT_WEST };
static u8 g_move_step_jump_landing_phases[4] = { MOVE_STEP_PHASE_JUMP_LANDING_SOUTH, MOVE_STEP_PHASE_JUMP_LANDING_EAST,
    MOVE_STEP_PHASE_JUMP_LANDING_NORTH, MOVE_STEP_PHASE_JUMP_LANDING_WEST };

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
static void move_step_set_delta_center_to_edge(
    unit_t* unit, const move_step_t* step, const battle_map_tile_t* tile, s32 edge_height) {
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
 * The mirror of move_step_set_delta_center_to_edge: the height part
 * of the direction vector, (edge_height - 1) * the tile's slope half-height * 6,
 * is not negated, so entering over a high side (slope height 2) moves the unit
 * down to the center. */
static void move_step_set_delta_edge_to_center(
    unit_t* unit, const move_step_t* step, const battle_map_tile_t* tile, s32 edge_height) {
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

/* The float steps' copy of move_step_set_delta_center_to_edge: the
 * original wrote the same body a second time. */
static void move_step_set_float_delta_center_to_edge(
    unit_t* unit, const move_step_t* step, const battle_map_tile_t* tile, s32 edge_height) {
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

/* The float steps' copy of move_step_set_delta_edge_to_center: the
 * original wrote the same body a second time. */
static void move_step_set_float_delta_edge_to_center(
    unit_t* unit, const move_step_t* step, const battle_map_tile_t* tile, s32 edge_height) {
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
void move_step_scale_velocity(unit_t* unit, s32 scale) {
    unit->velocity.vx = (scale * unit->velocity.vx) / ONE;
    unit->velocity.vy = (scale * unit->velocity.vy) / ONE;
    unit->velocity.vz = (scale * unit->velocity.vz) / ONE;
}

/* Starts the teleport animation and sets the step destination: the tile in the path bytes,
 * or the unit's own tile when the teleport failed. */
void move_step_start_teleport_animation(unit_t* unit) {
    unit->step_phase = MOVE_STEP_PHASE_TELEPORT_ANIMATION;
    animation_store_facing(ANIMATION_TELEPORT, unit->facing, unit);
    if (unit->path.count == MOVE_PATH_TELEPORT) {
        unit->movement.destination_x = unit->path.data.teleport.x;
        unit->movement.destination_y = unit->path.data.teleport.y;
        unit->movement.destination_z = unit->path.data.teleport.elevation;
    } else {
        unit->movement.destination_x = unit->map_x;
        unit->movement.destination_y = unit->map_y;
        unit->movement.destination_z = unit->map_z;
    }
}

/* Puts the unit in the TELEPORT_START step phase. The init record it builds is not used. */
void move_step_start_teleport(unit_t* unit) {
    effect_context_t data;

    effect_secondary_build_init(unit, &data);
    unit->step_phase = MOVE_STEP_PHASE_TELEPORT_START;
}

/* Start a floating unit's next step (the FLOAT_TO_EDGE phases) from the current
 * tile center, first unlinking a unit that rides from its mount.
 *
 * The step byte's top two bits pick the direction, which sets the facing, the
 * phase and the velocity toward the exit edge; its low two bits extend the
 * destination tile coordinate. The second tile argument is unused. */
void move_step_start_float(unit_t* unit, const battle_map_tile_t* from, const battle_map_tile_t* to) {
    move_step_t current_step;
    move_step_t* step;
    s32 edge_height;
    s16 facing;
    u8 next;

    unit_mount_dismount_rider_and_update_display(unit);
    edge_height = unit->current_edge_height;
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
    animation_set_walk(unit);
    move_step_set_float_delta_center_to_edge(unit, step, from, edge_height);
    move_step_scale_velocity(unit, unit->walk_speed);
    unit->movement.destination_z = step->higher;
}

/* Start a flying unit's next step (the FLY_TO_EDGE phases) with
 * ANIMATION_FLY, first unlinking a unit that rides from its mount.
 *
 * The step byte's top two bits pick the direction, which sets the facing and
 * the phase; its low two bits extend the destination tile coordinate. A carried
 * rider (mount state MOUNT) takes ANIMATION_RIDING with the mount's
 * facing. The tile arguments are not read. */
void move_step_start_fly(
    unit_t* unit, const battle_map_tile_t* unused_current_tile, const battle_map_tile_t* unused_destination_tile) {
    move_step_t current_step;
    s32 facing;
    move_step_t* step;
    unit_t* rider;
    u8 next;

    unit_mount_dismount_rider(unit);
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

    animation_store_facing(ANIMATION_FLY, facing, unit);
    if (unit->mount_state == UNIT_MOUNT_STATE_MOUNT) {
        rider = unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        if (rider != 0) {
            animation_store_facing(ANIMATION_RIDING, unit->facing, rider);
        }
    }
    move_step_set_velocity_clamped(unit, &unit->path, step);
    move_step_scale_velocity(unit, unit->walk_speed);
    unit->movement.destination_z = step->higher;
}

/* Start a walking unit's next step (the WALK_TO_EDGE phases) from the current
 * tile center.
 *
 * The step byte's top two bits pick the direction, which sets the facing, the
 * phase and the velocity toward the exit edge; its low two bits extend the
 * destination tile coordinate. In GAME_STATE_ACTION_EXECUTE, as during a
 * knockback, the unit keeps its facing and animation. The switch has no
 * default, so `facing` is unset for a direction outside 0-3 (none exists). */
void move_step_start_walk(unit_t* unit, const battle_map_tile_t* from, const battle_map_tile_t* unused_to) {
    move_step_t* step;
    move_step_t current_step;
    s32 edge_height = unit->current_edge_height;
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
    if (g_game_state != GAME_STATE_ACTION_EXECUTE) {
        unit->facing = facing;
        animation_set_walk(unit);
    }
    move_step_set_delta_center_to_edge(unit, step, from, edge_height);
    move_step_scale_velocity(unit, unit->walk_speed);
    unit->movement.destination_z = step->higher;
}

/* Start the unit's next step at climb speed (the CLIMB_TO_EDGE phases) with
 * ANIMATION_CLIMB.
 *
 * As in move_step_start_walk, the step byte's top two bits pick the
 * direction, which sets the facing, the phase and the velocity toward the exit
 * edge, and its low two bits extend the destination tile coordinate. A carried
 * rider (mount state MOUNT) takes ANIMATION_RIDING with the mount's
 * facing. The interpolation scale is step_speed rather than the walk speed. */
void move_step_start_climb(unit_t* unit, const battle_map_tile_t* from, const battle_map_tile_t* unused_to) {
    move_step_t current_step;
    move_step_t* step;
    s32 edge_height;
    unit_t* rider;
    u8 next;

    edge_height = unit->current_edge_height;
    step = &unit->current_path_step;
    /* The byte copy prevents a wider direction load. */
    current_step = unit->current_path_step;
    switch (current_step.direction) {
    case MOVE_STEP_DIRECTION_EAST:
        unit->step_phase = MOVE_STEP_PHASE_CLIMB_TO_EDGE_EAST;
        animation_store_facing(ANIMATION_CLIMB, UNIT_FACING_EAST, unit);
        next = unit->map_x + 1;
        unit->movement.destination_x = next + unit->current_path_step.length;
        break;
    case MOVE_STEP_DIRECTION_WEST:
        unit->step_phase = MOVE_STEP_PHASE_CLIMB_TO_EDGE_WEST;
        animation_store_facing(ANIMATION_CLIMB, UNIT_FACING_WEST, unit);
        next = unit->map_x - 1;
        unit->movement.destination_x = next - unit->current_path_step.length;
        break;
    case MOVE_STEP_DIRECTION_SOUTH:
        unit->step_phase = MOVE_STEP_PHASE_CLIMB_TO_EDGE_SOUTH;
        animation_store_facing(ANIMATION_CLIMB, UNIT_FACING_SOUTH, unit);
        next = unit->map_y - 1;
        unit->movement.destination_y = next - unit->current_path_step.length;
        break;
    case MOVE_STEP_DIRECTION_NORTH:
        unit->step_phase = MOVE_STEP_PHASE_CLIMB_TO_EDGE_NORTH;
        animation_store_facing(ANIMATION_CLIMB, UNIT_FACING_NORTH, unit);
        next = unit->map_y + 1;
        unit->movement.destination_y = next + unit->current_path_step.length;
        break;
    }
    if (unit->mount_state == UNIT_MOUNT_STATE_MOUNT) {
        rider = unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        if (rider != 0) {
            animation_store_facing(ANIMATION_RIDING, unit->facing, rider);
        }
    }
    move_step_set_delta_center_to_edge(unit, step, from, edge_height);
    move_step_scale_velocity(unit, unit->step_speed);
    unit->movement.destination_z = step->higher;
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
static void move_step_start_jump(
    unit_t* unit, const battle_map_tile_t* unused_from, const battle_map_tile_t* unused_to) {
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
        animation_store_facing(ANIMATION_JUMP, facings[current_step.direction], unit);
    } else {
        unit->step_phase = hop_phases[current_step.direction];
        unit_mount_dismount_rider(unit);
        animation = ANIMATION_HOP;
        if (unit->movement.alternate_short_hop) {
            animation = ANIMATION_HOP_ALTERNATE;
        }
        animation_store_facing(animation, unit->facing, unit);
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
    unit->movement.destination_z = step->higher;
}

/* Start a unit's jump across one or more tiles onto the destination tile.
 *
 * Heights are in half-steps of 6 world units. The landing height is the top of
 * the unit it lands on (standing_on_battle_id_plus_one, set for a step onto a
 * unit; per unit_calculate_height_data) or, when there is none or its
 * height is UNIT_HEIGHT_UNAVAILABLE, the destination tile's height plus
 * its slope scaled by destination_edge_height, plus 2. The launch speed comes
 * from the rise over the unit's current height plus two half-steps of
 * clearance per tile crossed (length + 1); the frame count adds a fall from
 * the apex, and the horizontal speed covers half a tile over those frames.
 * The step byte's top two bits give the direction, as in
 * move_step_start_jump. `unused_10` reproduces an unreferenced
 * 16-byte frame slot below the height data. */
static void move_step_start_climb_jump(unit_t* unit, const battle_map_tile_t* from, const battle_map_tile_t* to) {
    VECTOR unused_10;
    unit_height_data_t height_data;
    move_step_t* step = &unit->current_path_step;
    s32 height;
    s32 speed;
    s32 time;
    u8 next;

    if (unit->standing_on_battle_id_plus_one != 0
        && (unit_calculate_height_data(&height_data, unit->standing_on_battle_id_plus_one - 1),
            (height_data.unit_flags & UNIT_HEIGHT_UNAVAILABLE) == 0)) {
        height = height_data.total_height;
    } else {
        height = to->height * 2 + to->depth_half_height.half_height * unit->destination_edge_height + 2;
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
        animation_store_facing(ANIMATION_JUMP, UNIT_FACING_EAST, unit);
        next = unit->map_x + 1;
        unit->movement.destination_x = next + step->length;
        unit->velocity.vx = (BATTLE_MAP_TILE_SIZE * ONE / 2) / time;
        unit->velocity.vz = 0;
        break;
    case MOVE_STEP_DIRECTION_WEST:
        unit->step_phase = MOVE_STEP_PHASE_JUMP_START_WEST;
        animation_store_facing(ANIMATION_JUMP, UNIT_FACING_WEST, unit);
        next = unit->map_x - 1;
        unit->movement.destination_x = next - step->length;
        unit->velocity.vx = -(BATTLE_MAP_TILE_SIZE * ONE / 2) / time;
        unit->velocity.vz = 0;
        break;
    case MOVE_STEP_DIRECTION_SOUTH:
        unit->step_phase = MOVE_STEP_PHASE_JUMP_START_SOUTH;
        animation_store_facing(ANIMATION_JUMP, UNIT_FACING_SOUTH, unit);
        next = unit->map_y - 1;
        unit->movement.destination_y = next - step->length;
        unit->velocity.vz = -(BATTLE_MAP_TILE_SIZE * ONE / 2) / time;
        unit->velocity.vx = 0;
        break;
    case MOVE_STEP_DIRECTION_NORTH:
        unit->step_phase = MOVE_STEP_PHASE_JUMP_START_NORTH;
        animation_store_facing(ANIMATION_JUMP, UNIT_FACING_NORTH, unit);
        next = unit->map_y + 1;
        unit->movement.destination_y = next + step->length;
        unit->velocity.vz = (BATTLE_MAP_TILE_SIZE * ONE / 2) / time;
        unit->velocity.vx = 0;
        break;
    }
    unit->movement.destination_z = step->higher;
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
static void move_step_start_climb_hop(unit_t* unit, const battle_map_tile_t* from, const battle_map_tile_t* to) {
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
        && (unit_calculate_height_data(&height_data, unit->standing_on_battle_id_plus_one - 1),
            (height_data.unit_flags & UNIT_HEIGHT_UNAVAILABLE) == 0)) {
        height = height_data.total_height;
    } else {
        height = to->height * 2 + to->depth_half_height.half_height * unit->destination_edge_height;
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
    animation_store_facing(animation, unit->facing, unit);
    unit->movement.alternate_short_hop ^= 1;
    unit->movement.destination_z = step->higher;
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
static void move_step_start_float_jump(unit_t* unit, const battle_map_tile_t* from, const battle_map_tile_t* to) {
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
    height = to->height * 2 + to->depth_half_height.half_height * unit->destination_edge_height + 2;
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
    unit->movement.destination_z = step->higher;
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
void move_step_apply_velocity(unit_t* unit) {
    s16 tile[2];
    u8 layer;
    s16 ground_height;
    s16 destination_height;
    battle_map_tile_t* map_tile;
    unit_t* rider;
    s32 depth;

    tile[0] = (unit->real.vx + unit->velocity.vx) / (BATTLE_MAP_TILE_SIZE * ONE);
    tile[1] = (unit->real.vz + unit->velocity.vz) / (BATTLE_MAP_TILE_SIZE * ONE);
    /* Entering the destination tile switches the unit to the destination's layer. */
    layer = unit->map_z;
    if (tile[0] == unit->movement.destination_x && tile[1] == unit->movement.destination_y
        && unit->map_z != unit->movement.destination_z) {
        layer = unit->map_z = unit->movement.destination_z;
    }

    /* Never runs: the statement above has already made map_z equal destination_z here. */
    switch (unit->step_phase) {
    case MOVE_STEP_PHASE_WALK_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_WEST:
        if (tile[0] == unit->movement.destination_x && tile[1] == unit->movement.destination_y
            && unit->map_z != unit->movement.destination_z) {
            layer = unit->map_z = unit->movement.destination_z;
            map_tile = battle_map_tile_get_pointer(tile[0], tile[1], layer);
            move_step_set_delta_edge_to_center(unit, &unit->current_path_step, map_tile, unit->destination_edge_height);
            move_step_scale_velocity(unit, unit->walk_speed);
        }
        break;
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_WEST:
        if (tile[0] == unit->movement.destination_x && tile[1] == unit->movement.destination_y
            && unit->map_z != unit->movement.destination_z) {
            layer = unit->map_z = unit->movement.destination_z;
            map_tile = battle_map_tile_get_pointer(tile[0], tile[1], layer);
            move_step_set_delta_edge_to_center(unit, &unit->current_path_step, map_tile, unit->destination_edge_height);
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
        map_tile = battle_map_tile_get_pointer(tile[0], tile[1], layer);
        if ((-unit->real.vy / ONE) <= -ground_height) {
            unit->real.vy = ground_height * ONE;
            move_step_set_delta_center_to_edge(unit, &unit->current_path_step, map_tile, unit->current_edge_height);
            move_step_scale_velocity(unit, unit->walk_speed);
        } else {
            if ((-unit->real.vy / ONE) < -ground_height + 6) {
                unit->real.vy = ground_height * ONE;
            } else if (unit->velocity.vy <= 0x7FFF) {
                unit->velocity.vy += g_move_jump_gravity;
            }
            if (-unit->real.vy / ONE >= -ground_height + 0x2A) {
                unit->step_phase = MOVE_STEP_PHASE_JUMP_ASCENT_NORTH;
                if (g_game_state != GAME_STATE_ACTION_EXECUTE) {
                    animation_store_facing(ANIMATION_FALL, unit->facing, unit);
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
        map_tile = battle_map_tile_get_pointer(tile[0], tile[1], layer);
        if ((-unit->real.vy / ONE) <= -ground_height) {
            unit->real.vy = ground_height * ONE;
            if (unit->step_speed <= 0x7FFF) {
                unit->step_speed += g_move_jump_gravity;
            }
            move_step_set_delta_center_to_edge(unit, &unit->current_path_step, map_tile, unit->current_edge_height);
            move_step_scale_velocity(unit, unit->step_speed);
        } else {
            if ((-unit->real.vy / ONE) < -ground_height + 6) {
                unit->real.vy = ground_height * ONE;
            } else if (unit->velocity.vy <= 0x7FFF) {
                unit->velocity.vy += g_move_jump_gravity;
            }
            if (-unit->real.vy / ONE >= -ground_height + 0x2A) {
                unit->step_phase = MOVE_STEP_PHASE_JUMP_ASCENT_NORTH;
                if (g_game_state != GAME_STATE_ACTION_EXECUTE) {
                    animation_store_facing(ANIMATION_FALL, unit->facing, unit);
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
        map_tile = battle_map_tile_get_pointer(tile[0], tile[1], layer);
        if (-unit->real.vy / ONE <= -ground_height) {
            unit->real.vy = ground_height * ONE;
            move_step_set_delta_edge_to_center(unit, &unit->current_path_step, map_tile, unit->destination_edge_height);
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
                if (g_game_state != GAME_STATE_ACTION_EXECUTE) {
                    animation_store_facing(ANIMATION_FALL, unit->facing, unit);
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
        map_tile = battle_map_tile_get_pointer(tile[0], tile[1], layer);
        if (-unit->real.vy / ONE <= -ground_height) {
            unit->real.vy = ground_height * ONE;
            if (unit->step_speed <= 0x7FFF) {
                unit->step_speed += g_move_jump_gravity;
            }
            move_step_set_delta_edge_to_center(unit, &unit->current_path_step, map_tile, unit->destination_edge_height);
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
                if (g_game_state != GAME_STATE_ACTION_EXECUTE) {
                    animation_store_facing(ANIMATION_FALL, unit->facing, unit);
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
        map_tile = battle_map_tile_get_pointer(tile[0], tile[1], layer);
        depth = -unit->real.vy;
        if (depth / ONE <= -ground_height) {
            unit->real.vy = ground_height * ONE;
            move_step_set_float_delta_center_to_edge(
                unit, &unit->current_path_step, map_tile, unit->destination_edge_height);
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
        map_tile = battle_map_tile_get_pointer(tile[0], tile[1], layer);
        depth = -unit->real.vy;
        if (depth / ONE <= -ground_height) {
            unit->real.vy = ground_height * ONE;
            move_step_set_float_delta_edge_to_center(
                unit, &unit->current_path_step, map_tile, unit->destination_edge_height);
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
        battle_map_tile_get_pointer(tile[0], tile[1], layer);
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
        battle_map_tile_get_pointer(tile[0], tile[1], layer);
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
        rider = unit_get_by_runtime_id(unit->mount_partner_runtime_id);
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
void unit_play_sfx(unit_t* unit, s32 sound_id) {
    if ((unit->movement.suppress_sfx_and_landing_effects) == 0) {
        sound_play_sfx_on_free_channel(sound_id);
    }
}

/* Play the landing sound for the surface of the tile a unit's step ends on:
 * SOUND_SFX_LAND_SOFT for grassland, thicket and rug, SOUND_SFX_LAND_WATER for the
 * water and marsh surfaces, SOUND_SFX_LAND_WOOD for wooden floor, tree and box,
 * and SOUND_SFX_LAND_DEFAULT otherwise. Nothing plays for a unit whose motion
 * flags suppress landing effects, or when the destination tile does not exist. */
static void move_step_play_landing_sfx_by_surface_type(unit_t* unit) {
    battle_map_tile_t* tile;
    s32 surface;
    s32 sfx;

    if (unit->movement.suppress_sfx_and_landing_effects)
        return;
    tile = battle_map_tile_get_pointer(
        unit->movement.destination_x, unit->movement.destination_y, unit->movement.destination_z);
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
    sound_play_sfx_on_free_channel(sfx);
}

/* Start the landing effect that suits the surface of the unit's destination tile:
 * the fall dust on hard ground, floors and structures, the splash on swamp, marsh
 * and open water. Other surfaces (grassland, thicket, snow, lava, rugs, furniture
 * and the like) get none. Nothing starts for a unit whose motion flags suppress
 * landing effects, or when the destination tile does not exist. */
static void move_step_set_landing_secondary_by_surface_type(unit_t* unit) {
    battle_map_tile_t* tile;
    s32 surface;

    if (unit->movement.suppress_sfx_and_landing_effects)
        return;
    tile = battle_map_tile_get_pointer(
        unit->movement.destination_x, unit->movement.destination_y, unit->movement.destination_z);
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
        effect_secondary_set_fall_dust(unit);
        break;
    case BATTLE_MAP_SURFACE_SWAMP:
    case BATTLE_MAP_SURFACE_MARSH:
    case BATTLE_MAP_SURFACE_POISONED_MARSH:
    case BATTLE_MAP_SURFACE_WATERWAY:
    case BATTLE_MAP_SURFACE_RIVER:
    case BATTLE_MAP_SURFACE_LAKE:
    case BATTLE_MAP_SURFACE_SEA:
    case BATTLE_MAP_SURFACE_WATERFALL:
        effect_secondary_set_splash(unit);
        break;
    }
}

/*
 * Resolves the terrain records for a moving unit's current tile and the tile
 * one step away, as encoded by current_path_step: bits 6-7 select the axis and
 * sign, bit 5 selects the map layer and bits 0-1 add extra distance.
 */
void move_step_get_current_and_destination_tiles(
    unit_t* unit, battle_map_tile_t** out_current, battle_map_tile_t** out_target) {
    u8 movement;
    u32 flags;
    s32 dir;
    s32 x;
    s32 y;
    s32 layer;

    x = unit->map_x;
    y = unit->map_y;
    layer = unit->map_z;
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
void move_settle_on_tile(unit_t* unit) {
    unit_stats_t* stats = unit->stats;
    unit_t* partner;

    if (stats != 0) {
        /* facing is read signed (lh) here. */
        unit_set_tile_position(
            stats->battle_id, unit->map_x, unit->map_y, unit->map_z, unit->facing / UNIT_FACING_QUARTER_TURN);
        if (unit->mount_state != UNIT_MOUNT_STATE_NONE) {
            partner = unit_get_by_runtime_id(unit->runtime_id);
            if (unit->stats != 0) {
                unit_set_tile_position(partner->stats->battle_id, unit->map_x, unit->map_y, unit->map_z,
                    unit->facing / UNIT_FACING_QUARTER_TURN);
            }
        }
    }
    unit_set_move_and_world_coords(unit);
    unit_height_calculate_from_world_coords(unit);
    cursor_move_to_unit(unit);
    unit->path.count = 0;
    animation_set_for_status(unit);
}

/*
 * Mount the unit onto its pending mount.  Bit 0x80 of pending_mount selects an
 * explicit battle id in the low seven bits; otherwise the mount is whatever
 * chocobo already stands on the unit's tile, and failing that the unit is
 * itself the mount for a rider recorded at mount_partner_runtime_id.
 */
void unit_mount_onto_pending(unit_t* unit) {
    unit_t* other;

    move_settle_on_tile(unit);
    if (unit->path.pending_mount & MOVE_PENDING_MOUNT_SET) {
        other = unit_get_by_battle_id(unit->path.pending_mount & MOVE_PENDING_MOUNT_ID_MASK);
        if (other != 0) {
            unit_mount_seat_rider(unit, other);
            unit_play_sfx(unit, SOUND_SFX_LAND_DEFAULT);
        }
        return;
    }

    other = unit_find_other_chocobo_at_map_coords(unit->map_x, unit->map_y, unit->map_z);
    if (other != 0) {
        if (other != unit) {
            unit_mount_seat_rider(unit, other);
        }
        return;
    }

    if (unit->mount_state == UNIT_MOUNT_STATE_MOUNT) {
        other = unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        if (other != 0) {
            unit_mount_seat_rider(other, unit);
        }
    }
}

/* Place a unit that shares a tile with a mover delta world units from the point
 * (x, y) along g_move_displacement_direction, at the ground height there,
 * and mark its shadow for redrawing.
 *
 * y is the map depth axis (world_coords.vz). Dead, crystal, treasure and jumping units
 * are left where they are. */
static void move_step_displace_unit_along_direction(unit_t* unit, s32 x, s32 y, s32 delta) {
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
static void move_step_displace_overlapping_unit(unit_t* unit, s32 direction) {
    unit_t* other = unit_find_overlapping_at_tile(unit, unit->map_x, unit->map_y, unit->map_z);
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
    move_step_displace_unit_along_direction(other, x, y, -dist * 3 / 2);
}

/* Pushes a unit sharing the mover's destination tile aside, mirroring
 * move_step_displace_overlapping_unit but keyed on the pending step's
 * destination tile instead of the unit's current map square. The trailing
 * loop picks the push direction in g_move_displacement_direction: the
 * first direction off the path's axis on a straight step, or on a turn the
 * first one that reverses neither the current nor the next step.
 *
 * The case order and the reused `dist` temporary reproduce the target's
 * cross-jumped tails and its s1 allocation; spelling the second path index as
 * offset + base reproduces the offset-before-base address add. */
void move_step_displace_unit_at_destination_tile(unit_t* unit, s32 direction) {
    unit_t* other;
    s32 dist;
    s32 x;
    s32 y;
    s32 center;
    s32 next;

    if (unit->movement_path_offset == unit->path.count) {
        return;
    }
    other = unit_find_overlapping_at_tile(
        unit, unit->movement.destination_x, unit->movement.destination_y, unit->movement.destination_z);
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
    move_step_displace_unit_along_direction(other, x, y, dist);
}

/* Travel direction of the step in progress (move_direction_e), from the
 * unit's tile to its destination tile. A difference in y decides before one in x,
 * and the result is NORTH when the two tiles are the same. */
move_direction_e move_step_get_direction(const unit_t* unit) {
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
static void move_step_resume_to_center(unit_t* unit) {
    s32 direction = move_step_get_direction(unit);

    switch (g_move_step_value.climb) {
    case 0:
        move_step_start_walk(unit, g_move_step_current_tile, g_move_step_destination_tile);
        unit->step_phase = g_move_step_walk_to_center_phases[direction];
        break;
    case 1:
        move_step_start_climb(unit, g_move_step_current_tile, g_move_step_destination_tile);
        unit->step_phase = g_move_step_climb_to_center_phases[direction];
        break;
    }
}

/* Step handler of the JUMP_LANDING phases: once ANIMATION_LAND has
 * run out (or another animation plays), continue the step toward the tile center. */
void move_step_update_jump_landing(unit_t* unit) {
    if (((unit->animation.sequence_id >> 1) != ANIMATION_LAND) || (unit->animation.wait == 0)) {
        move_step_resume_to_center(unit);
    }
}

/* Whether a walking unit has reached the near edge of its movement destination
 * tile in the given direction.  Tiles are 28 world units wide and the test
 * keeps a 7-unit margin. */
s32 move_step_has_reached_entry_edge(s32 direction, const unit_t* unit) {
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
void move_step_update_jump_flight(unit_t* unit) {
    s32 direction;
    s16 target_world_y;
    battle_map_tile_t* tile;

    sprite_frame_load_unit(unit, &unit->animation, unit->animation.frame, unit->animation.sequence_id);
    direction = move_step_get_direction(unit);
    move_step_apply_velocity(unit);
    if (g_game_state != GAME_STATE_EVENT) {
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
                unit->step_phase = g_move_step_walk_to_center_phases[direction];
                break;
            case 1:
                move_step_start_climb(unit, g_move_step_current_tile, g_move_step_destination_tile);
                unit->step_phase = g_move_step_climb_to_center_phases[direction];
                break;
            }
            tile = battle_map_tile_get_pointer(
                unit->movement.destination_x, unit->movement.destination_y, unit->movement.destination_z);
            if (tile->depth_half_height.depth == 0) {
                return;
            }
            if (-unit->world_coords.vy >= (tile->height + tile->depth_half_height.depth) * 12) {
                return;
            }
            move_step_play_landing_sfx_by_surface_type(unit);
            move_step_set_landing_secondary_by_surface_type(unit);
            return;
        }
        unit->velocity.vy = 0;
        unit->state_frame_counter = 2;
        unit->step_phase = g_move_step_jump_landing_phases[direction];
        if (g_game_state != GAME_STATE_ACTION_EXECUTE) {
            animation_store_facing(ANIMATION_LAND, unit->facing, unit);
        }
        if (g_move_step_value.mount) {
            unit_play_sfx(unit, SOUND_SFX_LAND_SOFT);
            return;
        }
        move_step_play_landing_sfx_by_surface_type(unit);
        move_step_set_landing_secondary_by_surface_type(unit);
        return;
    }
    unit->velocity.vy = 0;
    unit->state_frame_counter = 2;
    unit->step_phase = g_move_step_jump_landing_phases[direction];
    if (g_game_state != GAME_STATE_ACTION_EXECUTE) {
        animation_store_facing(ANIMATION_LAND, unit->facing, unit);
    }
    if (g_move_step_value.mount) {
        unit_play_sfx(unit, SOUND_SFX_LAND_SOFT);
        return;
    }
    move_step_play_landing_sfx_by_surface_type(unit);
    move_step_set_landing_secondary_by_surface_type(unit);
}

/* Step handler of the JUMP_ASCENT phases. Once the unit's vertical velocity is
 * no longer negative it enters the JUMP_DESCENT phase on the destination
 * level and, for a jump onto a unit (the step's mount bit), records the unit
 * on the destination tile as the one it stands on (battle id plus one). */
void move_step_update_jump_ascent(unit_t* unit) {
    unit_t* occupant;
    s32 direction;

    sprite_frame_load_unit(unit, &unit->animation, unit->animation.frame, unit->animation.sequence_id);
    direction = move_step_get_direction(unit);
    if (g_game_state != GAME_STATE_EVENT) {
        move_step_displace_overlapping_unit(unit, direction);
    }
    if (unit->velocity.vy >= 0) {
        unit->step_phase = g_move_step_jump_descent_phases[direction];
        unit->map_z = unit->movement.destination_z;
        if (g_move_step_value.mount) {
            occupant = unit_find_runtime_at_tile(
                unit->movement.destination_x, unit->movement.destination_y, unit->movement.destination_z);
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
s32 move_step_has_reached_center(s32 direction, const unit_t* unit) {
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
void move_step_snap_axis_to_exit_edge(s32 direction, unit_t* unit) {
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
void move_step_snap_axis_to_center(s32 direction, unit_t* unit) {
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
void move_step_update_walk_to_center(unit_t* unit) {
    s32 direction = move_step_get_direction(unit);
    s16 standing_world_y;

    move_step_apply_velocity(unit);
    if (g_game_state != GAME_STATE_EVENT) {
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
s32 move_step_has_reached_exit_edge(s32 direction, const unit_t* unit) {
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
 * up to seven starts move_step_start_climb_hop, and anything higher
 * move_step_start_climb_jump. An occupant whose height is
 * UNIT_HEIGHT_UNAVAILABLE counts as the bare tile. */
void move_step_update_walk_to_edge(unit_t* unit) {
    s32 direction = move_step_get_direction(unit);
    u8 standing_on;

    move_step_apply_velocity(unit);
    if (g_game_state != GAME_STATE_EVENT) {
        move_step_displace_overlapping_unit(unit, direction);
    }
    if (move_step_has_reached_exit_edge(direction, unit) == 0) {
        return;
    }
    move_step_snap_axis_to_exit_edge(direction, unit);
    if (g_move_step_value.length == 0) {
        standing_on = unit->standing_on_battle_id_plus_one;
        if (standing_on != 0) {
            unit_calculate_height_data(&g_move_step_tile_occupant_height, standing_on - 1);
            if (g_move_step_tile_occupant_height.unit_flags & UNIT_HEIGHT_UNAVAILABLE) {
                g_move_step_current_edge_height = g_move_step_current_tile->height * 2
                    + g_move_step_current_tile->depth_half_height.half_height * unit->current_edge_height;
            } else {
                g_move_step_current_edge_height = g_move_step_tile_occupant_height.total_height;
            }
        } else {
            g_move_step_current_edge_height = g_move_step_current_tile->height * 2
                + g_move_step_current_tile->depth_half_height.half_height * unit->current_edge_height;
            g_move_step_current_edge_height += move_get_water_standing_offset(unit, g_move_step_current_tile);
        }

        if (g_move_step_value.mount) {
            g_move_step_tile_occupant = unit_find_runtime_at_tile(
                unit->movement.destination_x, unit->movement.destination_y, unit->movement.destination_z);
            unit->standing_on_battle_id_plus_one = g_move_step_tile_occupant->stats->battle_id + 1;
            unit_calculate_height_data(&g_move_step_tile_occupant_height, g_move_step_tile_occupant->stats->battle_id);
            if (g_move_step_tile_occupant_height.unit_flags & UNIT_HEIGHT_UNAVAILABLE) {
                g_move_step_destination_edge_height = g_move_step_destination_tile->height * 2
                    + g_move_step_destination_tile->depth_half_height.half_height * unit->destination_edge_height;
            } else {
                g_move_step_destination_edge_height = g_move_step_tile_occupant_height.total_height;
            }
        } else {
            unit->standing_on_battle_id_plus_one = 0;
            g_move_step_destination_edge_height = g_move_step_destination_tile->height * 2
                + g_move_step_destination_tile->depth_half_height.half_height * unit->destination_edge_height;
            /* Retail adds the destination's water lift to the current height, not to the
             * destination height, so a step up onto deep water can pass as a plain step. */
            g_move_step_current_edge_height += move_get_water_standing_offset(unit, g_move_step_destination_tile);
        }

        if (g_move_step_current_edge_height + 1 >= g_move_step_destination_edge_height) {
            unit_mount_dismount_rider_and_update_display(unit);
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
            unit_mount_dismount_rider(unit);
            unit->step_speed = 0x2000;
            move_step_start_climb_hop(unit, g_move_step_current_tile, g_move_step_destination_tile);
            unit->standing_on_battle_id_plus_one = 0;
            return;
        }
        unit->step_speed = 0x2000;
        move_step_start_climb_jump(unit, g_move_step_current_tile, g_move_step_destination_tile);
        unit->standing_on_battle_id_plus_one = 0;
    } else {
        unit->step_speed = 0x2000;
        move_step_start_jump(unit, g_move_step_current_tile, g_move_step_destination_tile);
        unit->standing_on_battle_id_plus_one = 0;
    }
}

/*
 * Step handler of the FLOAT_TO_EDGE stages: advance a floating unit one frame
 * toward its current tile's exit edge. At the edge the two tiles' edge heights
 * decide: a rise of at most one half-step continues as a float step into the
 * FLOAT_TO_CENTER stage, anything higher starts a float jump
 * (move_step_start_float_jump) at step speed 0x2000.
 */
void move_step_update_float_to_edge(unit_t* unit) {
    s32 direction = move_step_get_direction(unit);

    move_step_apply_velocity(unit);
    if (g_game_state != GAME_STATE_EVENT) {
        move_step_displace_overlapping_unit(unit, direction);
    }
    if (move_step_has_reached_exit_edge(direction, unit) != 0) {
        move_step_snap_axis_to_exit_edge(direction, unit);
        g_move_step_current_edge_height = g_move_step_current_tile->height * 2
            + g_move_step_current_tile->depth_half_height.half_height * unit->current_edge_height;
        unit->standing_on_battle_id_plus_one = 0;
        g_move_step_destination_edge_height = g_move_step_destination_tile->height * 2
            + g_move_step_destination_tile->depth_half_height.half_height * unit->destination_edge_height;
        if (g_move_step_current_edge_height + 1 >= g_move_step_destination_edge_height) {
            move_step_start_float(unit, g_move_step_current_tile, g_move_step_destination_tile);
            unit->step_phase = g_move_step_float_to_center_phases[direction];
        } else {
            unit->step_speed = 0x2000;
            move_step_start_float_jump(unit, g_move_step_current_tile, g_move_step_destination_tile);
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
void move_step_update_float_to_center(unit_t* unit) {
    s32 direction = move_step_get_direction(unit);
    s16 standing_world_y;

    move_step_apply_velocity(unit);
    if (g_game_state != GAME_STATE_EVENT) {
        move_step_displace_overlapping_unit(unit, direction);
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
