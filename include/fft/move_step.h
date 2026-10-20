#ifndef MOVE_STEP_H
#define MOVE_STEP_H

/* Movement steps: the step phases, and the routines that carry a unit from one tile to the next by walking, climbing,
 * jumping, flying, floating or teleporting. */

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

void move_units_update(void);
void move_step_update(struct unit_runtime* unit);

#endif
