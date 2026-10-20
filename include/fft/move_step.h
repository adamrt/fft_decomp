#ifndef MOVE_STEP_H
#define MOVE_STEP_H

/* Movement steps: the step phases, and the routines that carry a unit from one tile to the next by walking, climbing,
 * jumping, flying, floating or teleporting. */

#include "fft/battle_map.h"
#include "fft/move.h"
#include "psx/types.h"

struct unit_runtime;

/* Travel direction of the step in progress, as move_step_get_direction derives it from the
 * unit's tile and its destination tile. */
typedef enum move_direction {
    MOVE_DIRECTION_SOUTH = 0, /* -y */
    MOVE_DIRECTION_EAST = 1,  /* +x */
    MOVE_DIRECTION_NORTH = 2, /* +y */
    MOVE_DIRECTION_WEST = 3   /* -x */
} move_direction_e;

/* unit_runtime_t.step_phase: the stage of the movement step in progress. Every stage has one
 * value per move_direction_e direction, in that order. The handlers dispatch on the stage and
 * take the direction from the unit's coordinates, and the stores that change stage in the middle of
 * a step write the NORTH value whatever the direction. */
typedef enum move_step_phase {
    MOVE_STEP_PHASE_NONE = 0,
    /* walking: from the tile center to its exit edge, then on to the destination center */
    MOVE_STEP_PHASE_WALK_TO_EDGE_SOUTH = 1,
    MOVE_STEP_PHASE_WALK_TO_CENTER_SOUTH = 2,
    MOVE_STEP_PHASE_WALK_TO_EDGE_EAST = 3,
    MOVE_STEP_PHASE_WALK_TO_CENTER_EAST = 4,
    MOVE_STEP_PHASE_WALK_TO_EDGE_NORTH = 5,
    MOVE_STEP_PHASE_WALK_TO_CENTER_NORTH = 6,
    MOVE_STEP_PHASE_WALK_TO_EDGE_WEST = 7,
    MOVE_STEP_PHASE_WALK_TO_CENTER_WEST = 8,
    /* the same two stages at climb speed */
    MOVE_STEP_PHASE_CLIMB_TO_EDGE_SOUTH = 9,
    MOVE_STEP_PHASE_CLIMB_TO_CENTER_SOUTH = 10,
    MOVE_STEP_PHASE_CLIMB_TO_EDGE_EAST = 11,
    MOVE_STEP_PHASE_CLIMB_TO_CENTER_EAST = 12,
    MOVE_STEP_PHASE_CLIMB_TO_EDGE_NORTH = 13,
    MOVE_STEP_PHASE_CLIMB_TO_CENTER_NORTH = 14,
    MOVE_STEP_PHASE_CLIMB_TO_EDGE_WEST = 15,
    MOVE_STEP_PHASE_CLIMB_TO_CENTER_WEST = 16,
    /* jumping: the take-off animation, rising, falling to the destination edge, the landing animation */
    MOVE_STEP_PHASE_JUMP_START_SOUTH = 17,
    MOVE_STEP_PHASE_JUMP_ASCENT_SOUTH = 18,
    MOVE_STEP_PHASE_JUMP_DESCENT_SOUTH = 19,
    MOVE_STEP_PHASE_JUMP_LANDING_SOUTH = 20,
    MOVE_STEP_PHASE_JUMP_START_EAST = 21,
    MOVE_STEP_PHASE_JUMP_ASCENT_EAST = 22,
    MOVE_STEP_PHASE_JUMP_DESCENT_EAST = 23,
    MOVE_STEP_PHASE_JUMP_LANDING_EAST = 24,
    MOVE_STEP_PHASE_JUMP_START_NORTH = 25,
    MOVE_STEP_PHASE_JUMP_ASCENT_NORTH = 26,
    MOVE_STEP_PHASE_JUMP_DESCENT_NORTH = 27,
    MOVE_STEP_PHASE_JUMP_LANDING_NORTH = 28,
    MOVE_STEP_PHASE_JUMP_START_WEST = 29,
    MOVE_STEP_PHASE_JUMP_ASCENT_WEST = 30,
    MOVE_STEP_PHASE_JUMP_DESCENT_WEST = 31,
    MOVE_STEP_PHASE_JUMP_LANDING_WEST = 32,
    /* flying */
    MOVE_STEP_PHASE_FLY_TO_EDGE_SOUTH = 34,
    MOVE_STEP_PHASE_FLY_TO_CENTER_SOUTH = 35,
    MOVE_STEP_PHASE_FLY_TO_EDGE_EAST = 36,
    MOVE_STEP_PHASE_FLY_TO_CENTER_EAST = 37,
    MOVE_STEP_PHASE_FLY_TO_EDGE_NORTH = 38,
    MOVE_STEP_PHASE_FLY_TO_CENTER_NORTH = 39,
    MOVE_STEP_PHASE_FLY_TO_EDGE_WEST = 40,
    MOVE_STEP_PHASE_FLY_TO_CENTER_WEST = 41,
    /* floating: the two walking stages, and the jump up to a higher tile */
    MOVE_STEP_PHASE_FLOAT_TO_EDGE_SOUTH = 43,
    MOVE_STEP_PHASE_FLOAT_TO_CENTER_SOUTH = 44,
    MOVE_STEP_PHASE_FLOAT_JUMP_SOUTH = 45,
    MOVE_STEP_PHASE_FLOAT_TO_EDGE_EAST = 47,
    MOVE_STEP_PHASE_FLOAT_TO_CENTER_EAST = 48,
    MOVE_STEP_PHASE_FLOAT_JUMP_EAST = 49,
    MOVE_STEP_PHASE_FLOAT_TO_EDGE_NORTH = 51,
    MOVE_STEP_PHASE_FLOAT_TO_CENTER_NORTH = 52,
    MOVE_STEP_PHASE_FLOAT_JUMP_NORTH = 53,
    MOVE_STEP_PHASE_FLOAT_TO_EDGE_WEST = 55,
    MOVE_STEP_PHASE_FLOAT_TO_CENTER_WEST = 56,
    MOVE_STEP_PHASE_FLOAT_JUMP_WEST = 57,
    /* a teleport path (MOVE_PATH_TELEPORT and _FAILED): start the animation, then wait for it */
    MOVE_STEP_PHASE_TELEPORT_START = 59,
    MOVE_STEP_PHASE_TELEPORT_ANIMATION = 60
} move_step_phase_e;

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

void move_units_update(void);
void move_step_update(struct unit_runtime* unit);
move_direction_e move_step_get_direction(const struct unit_runtime* unit);
void move_step_get_current_and_destination_tiles(
    struct unit_runtime* unit, battle_map_tile_t** current_tile, battle_map_tile_t** destination_tile);
void move_step_start_walk(struct unit_runtime* unit, const battle_map_tile_t* from, const battle_map_tile_t* unused_to);
void move_step_start_climb(
    struct unit_runtime* unit, const battle_map_tile_t* from, const battle_map_tile_t* unused_to);
void move_step_start_float(struct unit_runtime* unit, const battle_map_tile_t* from, const battle_map_tile_t* to);
void move_step_start_fly(struct unit_runtime* unit, const battle_map_tile_t* unused_current_tile,
    const battle_map_tile_t* unused_destination_tile);
void move_step_start_teleport(struct unit_runtime* unit);
void move_step_start_teleport_animation(struct unit_runtime* unit);
void move_step_set_velocity_with_final_height(
    struct unit_runtime* unit, const move_path_t* path, const move_step_t* step);
void move_step_set_velocity_clamped(struct unit_runtime* unit, const move_path_t* path, const move_step_t* step);
void move_step_scale_velocity(struct unit_runtime* unit, s32 scale); /* scales each velocity component by scale / ONE */
void move_step_apply_velocity(struct unit_runtime* unit);
void move_step_update_walk_to_edge(struct unit_runtime* unit);
void move_step_update_walk_to_center(struct unit_runtime* unit);
void move_step_update_float_to_edge(struct unit_runtime* unit);
void move_step_update_float_to_center(struct unit_runtime* unit);
void move_step_update_jump_ascent(struct unit_runtime* unit);
void move_step_update_jump_flight(struct unit_runtime* unit);
void move_step_update_jump_landing(struct unit_runtime* unit);

/* Directions come from move_step_get_direction; other values leave the result unset.
 * The signed parameter preserves the original dispatch. */
s32 move_step_has_reached_exit_edge(s32 direction, const struct unit_runtime* unit);
s32 move_step_has_reached_center(s32 direction, const struct unit_runtime* unit);
s32 move_step_has_reached_entry_edge(s32 direction, const struct unit_runtime* unit);
void move_step_snap_axis_to_exit_edge(s32 direction, struct unit_runtime* unit);
void move_step_snap_axis_to_center(s32 direction, struct unit_runtime* unit);
void move_step_displace_unit_at_destination_tile(struct unit_runtime* unit, s32 direction);

#endif
