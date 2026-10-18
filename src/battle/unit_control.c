/* Unit control by runtime id, including map background color, animation and facing setters, shadow triggers, map jump
 * state starts, coordinate conversions and mounting. */

#include "fft/animation.h"
#include "fft/battle_map.h"
#include "fft/battle_map_light.h"
#include "fft/camera.h"
#include "fft/fade.h"
#include "fft/gfx.h"
#include "fft/move.h"
#include "fft/sprite.h"
#include "fft/state.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/libgte.h"
#include "psx/types.h"

extern rgb24_t g_battle_map_light_back_color_backup;

static rgb24_t g_battle_map_light_back_color = { 128, 128, 128 };

/* Save the map's ambient (GTE back) color for battle_map_light_restore_back_color. */
void battle_map_light_copy_back_color(void) {
    /* Component copies retain the original byte loads and separate addresses. */
    g_battle_map_light_back_color_backup.r = g_battle_map_light_back_color.r;
    g_battle_map_light_back_color_backup.g = g_battle_map_light_back_color.g;
    g_battle_map_light_back_color_backup.b = g_battle_map_light_back_color.b;
}

/* Reload the saved ambient color and load it into the GTE back color. */
void battle_map_light_restore_back_color(void) {
    g_battle_map_light_back_color.r = g_battle_map_light_back_color_backup.r;
    g_battle_map_light_back_color.g = g_battle_map_light_back_color_backup.g;
    g_battle_map_light_back_color.b = g_battle_map_light_back_color_backup.b;
    battle_map_light_control_back_color(BATTLE_MAP_LIGHT_BACK_COLOR_SET, g_battle_map_light_back_color.r,
        g_battle_map_light_back_color.g, g_battle_map_light_back_color.b);
}

rgb24_t* battle_map_light_get_back_color(void) {
    return &g_battle_map_light_back_color;
}

/* Set the map's ambient color and load it into the GTE back color; the GTE load is skipped while
 * the back color is disabled. */
void battle_map_light_set_back_color(s32 red, s32 green, s32 blue) {
    g_battle_map_light_back_color.r = red;
    g_battle_map_light_back_color.g = green;
    g_battle_map_light_back_color.b = blue;
    battle_map_light_control_back_color(BATTLE_MAP_LIGHT_BACK_COLOR_SET, g_battle_map_light_back_color.r,
        g_battle_map_light_back_color.g, g_battle_map_light_back_color.b);
}

/* Add per-component deltas to the map background color and apply it.
 *
 * A component whose tested delta is negative (`current + delta < current`)
 * becomes 0xff instead. The retail routine tests green with red_delta. */
void battle_map_light_adjust_back_color(s16 red_delta, s16 green_delta, s16 blue_delta) {
    if (g_battle_map_light_back_color.r + red_delta < g_battle_map_light_back_color.r) {
        g_battle_map_light_back_color.r = 0xff;
    } else {
        g_battle_map_light_back_color.r += red_delta;
    }
    if (g_battle_map_light_back_color.g + red_delta < g_battle_map_light_back_color.g) {
        g_battle_map_light_back_color.g = 0xff;
    } else {
        g_battle_map_light_back_color.g += green_delta;
    }
    if (g_battle_map_light_back_color.b + blue_delta < g_battle_map_light_back_color.b) {
        g_battle_map_light_back_color.b = 0xff;
    } else {
        g_battle_map_light_back_color.b += blue_delta;
    }
    battle_map_light_control_back_color(BATTLE_MAP_LIGHT_BACK_COLOR_SET, g_battle_map_light_back_color.r,
        g_battle_map_light_back_color.g, g_battle_map_light_back_color.b);
}

/* unit_update_display_by_runtime_id as a g_thread_call_target: event threads run it on the
 * main stack. */
void unit_update_graphics_by_runtime_id(u32 runtime_id) {
    unit_update_display_by_runtime_id(runtime_id);
}

/* animation_set_mounted by runtime id; event threads run it on the main
 * stack. */
void animation_set_mounted_by_runtime_id(u32 runtime_id) {
    animation_set_mounted(unit_get_by_runtime_id(runtime_id));
}

/* animation_set_walk by runtime id. No retail code
 * calls it. */
void animation_set_walk_by_runtime_id(u32 runtime_id) {
    animation_set_walk(unit_get_by_runtime_id(runtime_id));
}

/* animation_set_for_status by runtime id, for event scripts. */
void animation_set_status_by_runtime_id(u32 runtime_id) {
    animation_set_for_status(unit_get_by_runtime_id(runtime_id));
}

/* Request `animation` for a unit by runtime id, keeping its facing. */
void animation_set_specific_by_runtime_id(u32 runtime_id, u32 animation) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);
    if (unit != 0)
        animation_store_facing(animation, unit->facing, unit);
}

/* Request `animation` on every unit, keeping its camera-facing quadrant. */
void animation_store_camera_facing_for_all(u32 animation) {
    unit_t* unit = g_unit_list_head;

    while (unit != 0) {
        animation_store_facing(animation, unit->camera_facing_quadrant, unit);
        unit = unit->previous;
    }
}

/* The unit-ready test that script_wait_for_unit_ready polls. It ignores the unit and always
 * reports ready, so that wait never yields. */
s32 unit_is_ready(s32 unused_runtime_id) {
    return 1;
}

/* The unit's facing in quarter turns (0..3), or -1 when no record has the runtime
 * id. */
s16 unit_facing_get_quadrant_by_runtime_id(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        return (unit->facing / UNIT_FACING_QUARTER_TURN) % 4;
    }
    return -1;
}

/* Faces a unit in a direction given in quarter turns and restarts its animation
 * with that facing, as unit_facing_set_sixteenth_by_runtime_id does for
 * sixteenths. No retail code calls it. */
s32 unit_facing_set_quadrant_by_runtime_id(u32 runtime_id, s32 facing_quadrant) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);
    s32 animation;
    s16 facing;

    if (unit != 0) {
        animation = unit->animation.sequence_id >> 1;
        facing = facing_quadrant << 10;
        unit->facing = facing;
        if (animation < ANIMATION_IDLE) {
            animation = ANIMATION_STAND;
        }
        animation_force_facing(animation, facing, unit);
        return runtime_id;
    }
    return -1;
}

/* The unit's camera_facing_quadrant modulo 4, or -1 when no record has the runtime
 * id. No retail code calls it. */
s16 unit_facing_get_camera_quadrant_by_runtime_id(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        return unit->camera_facing_quadrant % 4;
    }
    return -1;
}

/* The unit's camera_facing_quadrant modulo 4, or -1 when no runtime record has the
 * battle id. */
s16 unit_facing_get_camera_quadrant_by_battle_id(u32 battle_id) {
    unit_t* unit = unit_get_by_battle_id(battle_id);

    if (unit != 0) {
        return unit->camera_facing_quadrant % 4;
    }
    return -1;
}

/* The unit's facing in sixteenths of a turn (0..15), or -1 when no record has the
 * runtime id. */
s32 unit_facing_get_sixteenth_by_runtime_id(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        return (unit->facing / 0x100) % 0x10;
    }
    return -1;
}

/* Faces a unit in a direction given in sixteenths of a turn and restarts its
 * current animation with that facing; an animation below idle becomes standing.
 * Returns the runtime id, or -1 when no record has it. */
s32 unit_facing_set_sixteenth_by_runtime_id(u32 runtime_id, s32 facing_nibble) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);
    s32 animation;
    s16 facing;

    if (unit != 0) {
        animation = unit->animation.sequence_id >> 1;
        facing = facing_nibble << 8;
        unit->facing = facing;
        if (animation < ANIMATION_IDLE) {
            animation = ANIMATION_STAND;
        }
        animation_force_facing(animation, facing, unit);
        return runtime_id;
    }
    return -1;
}

/* The unit's camera_facing_sixteenth modulo 16, or -1 when no record has the runtime
 * id. No retail code calls it. */
s16 unit_facing_get_camera_sixteenth_by_runtime_id(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        return unit->camera_facing_sixteenth % 0x10;
    }
    return -1;
}

/* Round every unit's facing to the nearest of the four directions. */
void unit_facing_snap_all_to_quadrants(void) {
    unit_t* unit = g_unit_list_head;
    s32 facing;

    while (unit != 0) {
        facing = unit->facing & UNIT_FACING_MASK;
        if (facing < UNIT_FACING_SOUTH_WEST) {
            unit->facing = UNIT_FACING_SOUTH;
        } else if (facing < UNIT_FACING_NORTH_WEST) {
            unit->facing = UNIT_FACING_WEST;
        } else if (facing < UNIT_FACING_NORTH_EAST) {
            unit->facing = UNIT_FACING_NORTH;
        } else if (facing < UNIT_FACING_SOUTH_EAST) {
            unit->facing = UNIT_FACING_EAST;
        } else {
            unit->facing = UNIT_FACING_SOUTH;
        }
        unit = unit->previous;
    }
}

/* Shows a unit's shadow (UnitShadow event instruction). Returns 0 when no unit has the runtime id. */
s32 sprite_shadow_show_by_runtime_id(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->shadow_graphic_trigger = 1;
        return 1;
    }
    return 0;
}

/* Hides a unit's shadow (UnitShadow event instruction). Returns 0 when no unit
 * has the runtime id. */
s32 sprite_shadow_hide_by_runtime_id(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->shadow_graphic_trigger = 0;
        return 1;
    }
    return 0;
}

/* Start a change to map_id (MapJumpingOut), saving the current game state: the
 * screen darkens by 0x100 / duration per frame (a duration of 0 takes one frame),
 * the map loads, the screen fades back in and the saved state resumes. */
void state_start_map_jump_out(s32 map_id, s32 duration) {
    g_battle_map_id = map_id;
    g_game_state_previous = g_game_state;
    g_game_state = GAME_STATE_MAP_JUMPING_OUT;
    g_fade_step = duration != 0 ? 0x100 / duration : 0x100;
}

/* Start an event's change to map_id (MapJumpingOut2), saving the current game
 * state: the screen darkens by 0x100 / duration per frame (a duration of 0 takes
 * one frame), the map loads, and the saved state resumes behind a black screen
 * until state_start_change_map_jump_in fades it in. */
void state_start_change_map_jump_out(s32 map_id, s32 duration) {
    g_battle_map_id = map_id;
    g_game_state_previous = g_game_state;
    g_game_state = GAME_STATE_MAP_JUMPING_OUT_2;
    g_fade_step = duration != 0 ? 0x100 / duration : 0x100;
}

/* Start the fade-in that ends an event's map change (MapJumpingIn2), saving the
 * current game state to return to. The screen lightens by 0x100 / duration per
 * frame; a duration of 0 takes one frame. */
void state_start_change_map_jump_in(s32 duration) {
    g_game_state_previous = g_game_state;
    g_game_state = GAME_STATE_MAP_JUMPING_IN_2;
    g_fade_step = duration != 0 ? 0x100 / duration : 0x100;
}

/* Leave the battle for the world map: enter CloseBattle at two vertical blanks
 * per frame with a fade step of 0x100 / duration (0x100 for a duration of 0). */
void state_enter_close_battle_to_world(s32 duration) {
    g_system_vsync_interval = 2;
    g_game_state = GAME_STATE_CLOSE_BATTLE;
    g_fade_step = duration != 0 ? 0x100 / duration : 0x100;
    g_game_flow_state = SYSTEM_GAME_FLOW_WORLD;
}

/* The number of records in the runtime unit list. No retail code calls it. */
s32 unit_get_count(void) {
    s32 count = 0;
    unit_t* unit = g_unit_list_head;
    while (unit != 0) {
        unit = unit->previous;
        count++;
    }
    return count;
}

/* The integer world position of the unit with this runtime id, or 0 for a missing unit.
 * unit_copy_world_coords and unit_project_to_screen test
 * the result against -1 instead. */
SVECTOR* unit_get_world_coords_by_runtime_id(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit == 0) {
        return 0;
    }
    return &unit->world_coords;
}

/* The integer world position of the unit in slot battle_id, or 0. No retail code calls
 * it. */
SVECTOR* unit_get_world_coords_by_battle_id(u32 battle_id) {
    unit_t* unit = unit_get_by_battle_id(battle_id);

    if (unit == 0) {
        return 0;
    }
    return &unit->world_coords;
}

/* Store the center of a unit's tile in coords->vx and coords->vz and its world height in
 * coords->vy. Returns 0 when the battle id has no runtime record, otherwise 1. The tile lookup's
 * result is unused, but the call is part of the retail instruction stream. */
s32 unit_get_tile_center_and_height_by_battle_id(u32 battle_id, SVECTOR* coords) {
    unit_t* unit = unit_get_by_battle_id(battle_id);

    if (unit != 0) {
        battle_map_tile_get_pointer(unit->stats->x, unit->stats->position.y, unit->stats->position.higher_elevation);
        coords->vx = unit->stats->x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        coords->vz = unit->stats->position.y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        coords->vy = unit_height_calculate_from_battle_tile(unit);
        return 1;
    }
    return 0;
}

/* The depth point (camera_get_tile_depth_point_unbiased) of a unit's world position. A
 * missing unit is not checked. No retail code calls it. */
void camera_get_unit_depth_point_by_runtime_id(u32 runtime_id, SVECTOR* coords) {
    camera_get_tile_depth_point_unbiased(unit_get_world_coords_by_runtime_id(runtime_id & 0xffff), coords);
}

/* Projects a world position to screen x and y. Rebuilds the camera matrices first
 * and leaves them loaded in the GTE. */
void camera_project_world_coords_to_screen(SVECTOR* world_coords, SVECTOR* screen_coords) {
    VECTOR transformed_coords;
    s32 flag;

    camera_update_matrices(&g_camera_matrix, &g_camera_render_state, &g_camera_screen_offset, &g_camera_zoom);
    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    RotTrans(world_coords, &transformed_coords, &flag);
    screen_coords->vx = transformed_coords.vx;
    screen_coords->vy = transformed_coords.vy;
}

/* Writes a unit's tile as three 16-bit values in the order x, layer, y and returns 1. A
 * runtime id without a record raises a pointer exception and returns 0. */
s32 unit_get_map_coords_by_runtime_id(u32 runtime_id, SVECTOR* coords) {
    unit_t* unit = g_unit_list_head;

    for (; unit != 0; unit = unit->previous) {
        if (unit->runtime_id == (runtime_id & 0xffff)) {
            coords->vx = unit->map_x;
            coords->vz = unit->map_y;
            coords->vy = unit->map_layer;
            return 1;
        }
    }
    system_handle_pointer_exception(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
    return 0;
}

/* Path a unit from its tile to `destination` (the WalkTo event instruction) and start it walking.
 * `flags` and `jump` go to the pathfinder; the unit's walk speed is twice `speed`. A riding unit
 * dismounts. Returns 1 when a path was found, 0 when none was; a missing unit raises
 * a pointer exception and returns 0. */
s32 move_start_walk_to(
    s32 runtime_id, const battle_map_tile_coordinates_t* destination, s32 flags, s32 jump, s32 speed) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);
    move_path_t* path;

    if (unit != 0) {
        path = move_path_calculate_walk_to(
            flags, jump, unit->map_x, unit->map_y, unit->map_layer, destination->x, destination->y, destination->layer);
        if (path != 0) {
            unit->path = *path;
            unit->walk_speed = speed * 2;
            unit->step_speed = 0x2000;
            unit->movement_path_offset = 0;
            unit_mount_dismount_rider_and_update_display(unit);
            return 1;
        }
    } else {
        system_handle_pointer_exception(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
    }
    return 0;
}

/* Start a unit on a one-step path: a jump of jump_length extra tiles in `direction` on its own
 * layer, at full speed. The Jump event instruction uses it. A missing unit raises a pointer
 * exception and returns 0. */
s32 move_start_jump_by_runtime_id(u32 runtime_id, s32 direction, s32 jump_length) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->path.count = 1;
        unit->walk_speed = 0x2000;
        unit->step_speed = 0x2000;
        unit->path.pending_mount = 0;
        unit->movement_path_offset = 0;
        unit->path.height_offsets.source_height_offset = 0;
        unit->path.height_offsets.source_present = 0;
        unit->path.height_offsets.destination_height_offset = 0;
        unit->path.height_offsets.destination_present = 0;
        unit->path.data.steps[0].length = jump_length;
        unit->path.data.steps[0].slow = 0;
        unit->path.data.steps[0].climb = 0;
        unit->path.data.steps[0].mount = 0;
        unit->path.data.steps[0].higher = unit->map_layer;
        unit->path.data.steps[0].direction = direction;
        unit->path.flags = 0;
        unit_mount_dismount_rider_and_update_display(unit);
    } else {
        system_handle_pointer_exception(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
        return 0;
    }
    return 1;
}

/* Seats one unit on another by runtime id (unit_mount_seat_rider).
 * Returns 0 when either record is missing. No retail code calls it. */
s32 unit_mount_by_runtime_ids(u32 rider_runtime_id, u32 mount_runtime_id) {
    unit_t* rider = unit_get_by_runtime_id(rider_runtime_id);
    unit_t* mount = unit_get_by_runtime_id(mount_runtime_id);

    if ((rider != 0) && (mount != 0)) {
        unit_mount_seat_rider(rider, mount);
        return 1;
    }
    return 0;
}
