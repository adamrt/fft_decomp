#include "fft/battle.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Return 1 without examining the runtime ID. */
s32 battle_return_one(s32 unused_runtime_id) {
    return 1;
}

/* The unit's facing in quarter turns (0..3), or -1 when no record has the runtime
 * id. */
s16 battle_unit_get_facing_quadrant_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);
    if (unit != 0) {
        return (unit->facing / BATTLE_FACING_QUARTER_TURN) % 4;
    }
    return -1;
}

/* Faces a unit in a direction given in quarter turns and restarts its animation
 * with that facing, as battle_unit_set_animation_from_facing_nibble does for
 * sixteenths. No retail code calls it. */
s32 battle_unit_set_animation_from_facing_hint(u32 runtime_id, s32 facing_hint) {
    battle_unit_t* unit;
    s32 animation;
    s16 facing;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);
    if (unit != 0) {
        animation = unit->animation.sequence_id >> 1;
        facing = facing_hint << 10;
        unit->facing = facing;
        if (animation < BATTLE_UNIT_ANIMATION_IDLE) {
            animation = BATTLE_UNIT_ANIMATION_STAND;
        }
        battle_unit_store_animation_facing_movement_data(animation, facing, unit);
        return runtime_id;
    }
    return -1;
}

/* The unit's camera_facing_quadrant modulo 4, or -1 when no record has the runtime
 * id. No retail code calls it. */
s16 battle_unit_get_camera_facing_quadrant_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);
    if (unit != 0) {
        return unit->camera_facing_quadrant % 4;
    }
    return -1;
}

/* The unit's camera_facing_quadrant modulo 4, or -1 when no runtime record has the
 * battle id. */
s16 battle_unit_get_camera_facing_quadrant_by_battle_id(u32 battle_id) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_battle_id(battle_id & 0xffff);
    if (unit != 0) {
        return unit->camera_facing_quadrant % 4;
    }
    return -1;
}

/* The unit's facing in sixteenths of a turn (0..15), or -1 when no record has the
 * runtime id. */
s32 battle_unit_get_facing_nibble_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);
    if (unit != 0) {
        return (unit->facing / 0x100) % 0x10;
    }
    return -1;
}

/* Faces a unit in a direction given in sixteenths of a turn and restarts its
 * current animation with that facing; an animation below idle becomes standing.
 * Returns the runtime id, or -1 when no record has it. */
s32 battle_unit_set_animation_from_facing_nibble(u32 runtime_id, s32 facing_nibble) {
    battle_unit_t* unit;
    s32 animation;
    s16 facing;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);
    if (unit != 0) {
        animation = unit->animation.sequence_id >> 1;
        facing = facing_nibble << 8;
        unit->facing = facing;
        if (animation < BATTLE_UNIT_ANIMATION_IDLE) {
            animation = BATTLE_UNIT_ANIMATION_STAND;
        }
        battle_unit_store_animation_facing_movement_data(animation, facing, unit);
        return runtime_id;
    }
    return -1;
}

/* The unit's camera_facing_sixteenth modulo 16, or -1 when no record has the runtime
 * id. No retail code calls it. */
s16 battle_unit_get_camera_facing_sixteenth_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);
    if (unit != 0) {
        return unit->camera_facing_sixteenth % 0x10;
    }
    return -1;
}

void battle_unit_snap_all_facings_to_quadrants(void) {
    battle_unit_t* unit;
    s32 facing;

    unit = g_battle_unit_list_head;
    while (unit != 0) {
        facing = unit->facing & BATTLE_FACING_MASK;
        if (facing < BATTLE_FACING_SOUTH_WEST) {
            unit->facing = BATTLE_FACING_SOUTH;
        } else if (facing < BATTLE_FACING_NORTH_WEST) {
            unit->facing = BATTLE_FACING_WEST;
        } else if (facing < BATTLE_FACING_NORTH_EAST) {
            unit->facing = BATTLE_FACING_NORTH;
        } else if (facing < BATTLE_FACING_SOUTH_EAST) {
            unit->facing = BATTLE_FACING_EAST;
        } else {
            unit->facing = BATTLE_FACING_SOUTH;
        }
        unit = unit->previous;
    }
}
