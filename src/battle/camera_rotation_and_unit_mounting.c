/* Camera rotation and unit mounting. Its start is a boundary the binary proves; its end is a boundary the binary
 * proves. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "psx/libgpu.h"
#include "psx/types.h"

void camera_save_rotation(void) {
    copyVector(&g_camera_rotation_backup, &g_camera_render_state);
}

void camera_restore_rotation(void) {
    copyVector(&g_camera_render_state, &g_camera_rotation_backup);
}

SVECTOR* camera_get_rotation(void) {
    return &g_camera_render_state;
}

void camera_set_rotation(const SVECTOR* rotation) {
    copyVector(&g_camera_render_state, rotation);
}

/* Starts a linear turn of all three camera angles to the one value
 * *destination_value over frame_count frames (divided by the animation speed),
 * applied by camera_update_rotation_animation. No retail code calls it. */
void camera_set_rotation_delta_from_value(const s16* destination_value, s32 frame_count) {
    s32 animation_frames;
    s32 delta_x;

    animation_frames = (frame_count & 0xffff) / g_main_system_animation_speed;
    delta_x = *destination_value - g_camera_render_state.vx;
    g_camera_rotation_countdown = animation_frames;
    animation_frames = g_camera_rotation_countdown;
    setVector(&g_camera_rotation_delta, delta_x / animation_frames,
        (*destination_value - g_camera_render_state.vy) / animation_frames,
        (*destination_value - g_camera_render_state.vz) / animation_frames);
}

/* Starts a linear turn of the camera angles by values[0..2] over frame_count
 * frames (divided by the animation speed). No retail code calls it. */
void camera_set_rotation_delta_from_values(const s16* values, s32 frame_count) {
    s32 animation_frames;
    s32 value_x;

    animation_frames = (frame_count & 0xffff) / g_main_system_animation_speed;
    value_x = values[0];
    g_camera_rotation_countdown = animation_frames;
    animation_frames = g_camera_rotation_countdown;
    setVector(&g_camera_rotation_delta, value_x / animation_frames, values[1] / animation_frames,
        values[2] / animation_frames);
}

/* Advances the camera-angle move by one frame while its countdown runs. Only the
 * uncalled camera_set_rotation_delta_* routines start the countdown, so
 * nothing moves in retail. */
void camera_update_rotation_animation(void) {
    if (g_camera_rotation_countdown != 0) {
        g_camera_rotation_countdown--;
        g_camera_render_state.vx += g_camera_rotation_delta.vx;
        g_camera_render_state.vy += g_camera_rotation_delta.vy;
        g_camera_render_state.vz += g_camera_rotation_delta.vz;
    }
}

void map_copy_background_color(void) {
    /* Component copies retain the original byte loads and separate addresses. */
    g_map_background_color_backup.r = g_map_background_color.r;
    g_map_background_color_backup.g = g_map_background_color.g;
    g_map_background_color_backup.b = g_map_background_color.b;
}

void map_restore_background_color(void) {
    g_map_background_color.r = g_map_background_color_backup.r;
    g_map_background_color.g = g_map_background_color_backup.g;
    g_map_background_color.b = g_map_background_color_backup.b;
    map_control_gte_background_color(
        BATTLE_BACKGROUND_COLOR_SET, g_map_background_color.r, g_map_background_color.g, g_map_background_color.b);
}

rgb24_t* map_get_background_color(void) {
    return &g_map_background_color;
}

void map_set_background_color(s32 red, s32 green, s32 blue) {
    g_map_background_color.r = red;
    g_map_background_color.g = green;
    g_map_background_color.b = blue;
    map_control_gte_background_color(
        BATTLE_BACKGROUND_COLOR_SET, g_map_background_color.r, g_map_background_color.g, g_map_background_color.b);
}

/* Add per-component deltas to the map background color and apply it.
 *
 * A component whose tested delta is negative (`current + delta < current`)
 * becomes 0xff instead. The retail routine tests green with red_delta. */
void map_adjust_background_color(s16 red_delta, s16 green_delta, s16 blue_delta) {
    if (g_map_background_color.r + red_delta < g_map_background_color.r) {
        g_map_background_color.r = 0xff;
    } else {
        g_map_background_color.r += red_delta;
    }
    if (g_map_background_color.g + red_delta < g_map_background_color.g) {
        g_map_background_color.g = 0xff;
    } else {
        g_map_background_color.g += green_delta;
    }
    if (g_map_background_color.b + blue_delta < g_map_background_color.b) {
        g_map_background_color.b = 0xff;
    } else {
        g_map_background_color.b += blue_delta;
    }
    map_control_gte_background_color(
        BATTLE_BACKGROUND_COLOR_SET, g_map_background_color.r, g_map_background_color.g, g_map_background_color.b);
}

void battle_unit_update_graphics_by_runtime_id(u32 runtime_id) {
    battle_unit_update_display_by_runtime_id(runtime_id);
}

void battle_unit_set_mounted_animation_by_runtime_id(u32 runtime_id) {
    battle_unit_set_animation_based_on_mounted_state(battle_unit_get_by_runtime_id(runtime_id & 0xffff));
}

/* battle_unit_set_idle_animation_for_movement by unit id. No retail code calls
 * it. */
void battle_unit_set_movement_idle_animation_by_runtime_id(u32 runtime_id) {
    battle_unit_set_idle_animation_for_movement(battle_unit_get_by_runtime_id(runtime_id & 0xffff));
}

void battle_unit_set_status_animation_by_runtime_id(u32 runtime_id) {
    battle_unit_set_animation_based_on_status(battle_unit_get_by_runtime_id(runtime_id & 0xffff));
}

/* Request `animation` for a unit by runtime id, keeping its facing. */
void battle_unit_set_specific_animation_by_runtime_id(u32 runtime_id, u32 animation) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);
    if (unit != 0)
        battle_unit_store_animation_facing(animation, unit->facing, unit);
}

/* Request animation on every unit, keeping its camera-facing quadrant. */
void battle_unit_store_camera_facing_animation_for_all(u32 animation) {
    battle_unit_t* unit = g_battle_unit_list_head;

    while (unit != 0) {
        battle_unit_store_animation_facing(animation, unit->camera_facing_quadrant, unit);
        unit = unit->previous;
    }
}

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

s32 battle_gfx_set_shadow_graphic_trigger_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->shadow_graphic_trigger = 1;
        return 1;
    }
    return 0;
}

/* Hides a unit's shadow (UnitShadow event instruction). Returns 0 when no unit
 * has the runtime id. */
s32 battle_gfx_clear_shadow_graphic_trigger_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->shadow_graphic_trigger = 0;
        return 1;
    }
    return 0;
}

/* Start a change to map_id (MapJumpingOut), saving the current game state: the
 * screen darkens by 0x100 / duration per frame (a duration of 0 takes one frame),
 * the map loads, the screen fades back in and the saved state resumes. */
void battle_state_start_map_jump_out(s32 map_id, s32 duration) {
    g_map_id = map_id;
    g_battle_previous_game_state = g_battle_game_state;
    g_battle_game_state = BATTLE_GAME_STATE_MAP_JUMPING_OUT;
    g_battle_state_screen_fade_step = duration != 0 ? 0x100 / duration : 0x100;
}

/* Start an event's change to map_id (MapJumpingOut2), saving the current game
 * state: the screen darkens by 0x100 / duration per frame (a duration of 0 takes
 * one frame), the map loads, and the saved state resumes behind a black screen
 * until battle_state_start_change_map_jump_in fades it in. */
void battle_state_start_change_map_jump_out(s32 map_id, s32 duration) {
    g_map_id = map_id;
    g_battle_previous_game_state = g_battle_game_state;
    g_battle_game_state = BATTLE_GAME_STATE_MAP_JUMPING_OUT_2;
    g_battle_state_screen_fade_step = duration != 0 ? 0x100 / duration : 0x100;
}

/* Start the fade-in that ends an event's map change (MapJumpingIn2), saving the
 * current game state to return to. The screen lightens by 0x100 / duration per
 * frame; a duration of 0 takes one frame. */
void battle_state_start_change_map_jump_in(s32 duration) {
    g_battle_previous_game_state = g_battle_game_state;
    g_battle_game_state = BATTLE_GAME_STATE_MAP_JUMPING_IN_2;
    g_battle_state_screen_fade_step = duration != 0 ? 0x100 / duration : 0x100;
}

/* Leave the battle for the world map: enter CloseBattle at two vertical blanks
 * per frame with a fade step of 0x100 / duration (0x100 for a duration of 0). */
void battle_state_start_close_battle(s32 duration) {
    g_main_system_animation_speed = 2;
    g_battle_game_state = BATTLE_GAME_STATE_CLOSE_BATTLE;
    g_battle_state_screen_fade_step = duration != 0 ? 0x100 / duration : 0x100;
    g_main_system_game_flow_state = MAIN_GAME_FLOW_WORLD;
}

s32 battle_unit_count(void) {
    s32 count = 0;
    battle_unit_t* unit = g_battle_unit_list_head;
    while (unit != 0) {
        unit = unit->previous;
        count++;
    }
    return count;
}

/* The integer world position of the unit with this runtime id, or 0 for a missing unit.
 * unit_copy_world_coords and unit_project_to_screen test
 * the result against -1 instead. */
SVECTOR* battle_unit_get_world_coords_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit == 0) {
        return 0;
    }
    return &unit->world_coords;
}

/* The integer world position of the unit in slot battle_id, or 0. No retail code calls
 * it. */
SVECTOR* battle_unit_get_world_coords_by_battle_id(u32 battle_id) {
    battle_unit_t* unit = battle_unit_get_by_battle_id(battle_id & 0xffff);

    if (unit == 0) {
        return 0;
    }
    return &unit->world_coords;
}

/*
 * Store a unit's tile-center coordinates and calculated screen Z.
 *
 * Return 0 when the battle ID has no renderer-side runtime record, otherwise 1.
 * The original performs a tile lookup whose returned pointer is unused; keep
 * that call because it is part of the target instruction stream.
 */
s32 battle_unit_get_tile_center_and_height_by_battle_id(u32 battle_id, SVECTOR* coords) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_battle_id(battle_id & 0xffff);
    if (unit != 0) {
        map_get_tile_data_pointer(unit->stats->x, unit->stats->position.y, unit->stats->position.higher_elevation);
        coords->vx = unit->stats->x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        coords->vz = unit->stats->position.y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        coords->vy = battle_gfx_calculate_world_height_from_battle_tile(unit);
        return 1;
    }
    return 0;
}

void camera_convert_unit_coords_by_runtime_id(u32 runtime_id, SVECTOR* coords) {
    camera_convert_screen_coords_modify_by_1(battle_unit_get_world_coords_by_runtime_id(runtime_id & 0xffff), coords);
}

/* Projects a world position to screen x and y. Rebuilds the camera matrices first
 * and leaves them loaded in the GTE. */
void camera_project_world_coords_to_screen(SVECTOR* world_coords, SVECTOR* screen_coords) {
    VECTOR transformed_coords;
    s32 result;

    camera_update_matrices(&g_camera_matrix, &g_camera_render_state, &g_camera_current_screen_offset, &g_camera_zoom);
    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    RotTrans(world_coords, &transformed_coords, &result);
    screen_coords->vx = transformed_coords.vx;
    screen_coords->vy = transformed_coords.vy;
}

/* Writes a unit's tile as three 16-bit values in the order x, z, y and returns 1. A
 * runtime id without a record raises pointer exception 12 and returns 0. */
s32 battle_unit_get_map_coords_by_runtime_id(u32 runtime_id, SVECTOR* coords) {
    battle_unit_t* unit = g_battle_unit_list_head;

    for (; unit != 0; unit = unit->previous) {
        if (unit->runtime_id == (runtime_id & 0xffff)) {
            coords->vx = unit->map_x;
            coords->vz = unit->map_y;
            coords->vy = unit->map_z;
            return 1;
        }
    }
    main_system_handle_pointer_exception(12);
    return 0;
}

/* Path a unit to a destination tile and start it walking at the given speed. */
s32 move_start_unit_walk_to(s32 runtime_id, map_tile_coordinates_t* destination, s32 flags, s32 jump, s32 speed) {
    battle_unit_t* unit;
    battle_walk_path_t* path;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xFFFF);
    if (unit != 0) {
        path = move_calculate_walkto_pathing(
            flags, jump, unit->map_x, unit->map_y, unit->map_z, destination->x, destination->y, destination->layer);
        if (path != 0) {
            unit->path = *path;
            unit->walk_speed = speed * 2;
            unit->step_speed = 0x2000;
            unit->movement_path_offset = 0;
            battle_unit_dismount_rider_and_update_display(unit);
            return 1;
        }
    } else {
        main_system_handle_pointer_exception(0xC);
    }
    return 0;
}

s32 battle_unit_prepare_rider_dismount(u32 runtime_id, s32 direction, s32 jump_length) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);
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
        unit->path.data.steps[0].higher = unit->map_z;
        unit->path.data.steps[0].direction = direction;
        unit->path.flags = 0;
        battle_unit_dismount_rider_and_update_display(unit);
    } else {
        main_system_handle_pointer_exception(12);
        return 0;
    }
    return 1;
}

/* Seats one unit on another by unit id (battle_unit_mount_rider_onto_unit).
 * Returns 0 when either record is missing. No retail code calls it. */
s32 battle_unit_mount_by_runtime_ids(u32 rider_runtime_id, u32 mount_runtime_id) {
    battle_unit_t* rider;
    battle_unit_t* mount;

    rider = battle_unit_get_by_runtime_id(rider_runtime_id & 0xffff);
    mount = battle_unit_get_by_runtime_id(mount_runtime_id & 0xffff);
    if ((rider != 0) && (mount != 0)) {
        battle_unit_mount_rider_onto_unit(rider, mount);
        return 1;
    }
    return 0;
}
