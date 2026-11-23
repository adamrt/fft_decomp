/* Expose scene and live-unit commands to event scripts: map transitions, placement, movement, animation, facing and
 * visibility. */

#include "fft/animation.h"
#include "fft/battle_state.h"
#include "fft/camera.h"
#include "fft/fade.h"
#include "fft/map.h"
#include "fft/map_unit.h"
#include "fft/move.h"
#include "fft/script.h"
#include "fft/sprite.h"
#include "fft/sprite_sheet.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* map_unit_update_display as a g_thread_call_target: event threads run it on the
 * main stack. */
void map_unit_update_graphics(u32 map_unit_id) {
    map_unit_update_display(map_unit_id);
}

/* animation_set_stand by map unit id; event threads run it on the main
 * stack. */
void animation_map_unit_set_stand(u32 map_unit_id) {
    animation_set_stand(map_unit_get(map_unit_id));
}

/* animation_set_walk by map unit id. No retail code
 * calls it. */
void animation_map_unit_set_walk(u32 map_unit_id) {
    animation_set_walk(map_unit_get(map_unit_id));
}

/* animation_set_status by map unit id, for event scripts. */
void animation_map_unit_set_status(u32 map_unit_id) {
    animation_set_status(map_unit_get(map_unit_id));
}

/* Request `animation` for a unit by map unit id, keeping its facing. */
void animation_map_unit_set_sequence(u32 map_unit_id, u32 animation) {
    map_unit_t* unit = map_unit_get(map_unit_id);
    if (unit != 0)
        animation_store_with_facing(animation, unit->facing, unit);
}

/* Request `animation` on every unit, keeping its camera-facing quadrant. */
void animation_store_all_with_camera_facing(u32 animation) {
    map_unit_t* unit = g_map_unit_list_head;

    while (unit != 0) {
        animation_store_with_facing(animation, unit->camera_facing_quadrant, unit);
        unit = unit->previous;
    }
}

/* The unit-ready test that script_wait_unit_ready polls. It ignores the unit and always
 * reports ready, so that wait never yields. */
s32 map_unit_is_ready(s32 unused_map_unit_id) {
    return 1;
}

/* The unit's facing in quarter turns (0..3), or -1 when no record has the runtime
 * id. */
s16 map_unit_get_facing_quadrant(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        return (unit->facing / UNIT_FACING_QUARTER_TURN) % 4;
    }
    return -1;
}

/* Faces a unit in a direction given in quarter turns and restarts its animation
 * with that facing, as map_unit_set_facing_sixteenth does for
 * sixteenths. No retail code calls it. */
s32 map_unit_set_facing_quadrant(u32 map_unit_id, s32 facing_quadrant) {
    map_unit_t* unit = map_unit_get(map_unit_id);
    s32 animation;
    s16 facing;

    if (unit != 0) {
        animation = unit->animation.sequence_id >> 1;
        facing = facing_quadrant << 10;
        unit->facing = facing;
        if (animation < ANIMATION_IDLE) {
            animation = ANIMATION_STAND;
        }
        animation_force_with_facing(animation, facing, unit);
        return map_unit_id;
    }
    return -1;
}

/* The unit's camera_facing_quadrant modulo 4, or -1 when no record has the runtime
 * id. No retail code calls it. */
s16 map_unit_get_camera_facing(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        return unit->camera_facing_quadrant % 4;
    }
    return -1;
}

/* The unit's camera_facing_quadrant modulo 4, or -1 when no map unit record has the
 * battle unit id. */
s16 map_unit_get_camera_facing_by_battle_unit_id(u32 battle_unit_id) {
    map_unit_t* unit = map_unit_get_by_battle_unit_id(battle_unit_id);

    if (unit != 0) {
        return unit->camera_facing_quadrant % 4;
    }
    return -1;
}

/* The unit's facing in sixteenths of a turn (0..15), or -1 when no record has the
 * map unit id. */
s32 map_unit_get_facing_sixteenth(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        return (unit->facing / 0x100) % 0x10;
    }
    return -1;
}

/* Faces a unit in a direction given in sixteenths of a turn and restarts its
 * current animation with that facing; an animation below idle becomes standing.
 * Returns the map unit id, or -1 when no record has it. */
s32 map_unit_set_facing_sixteenth(u32 map_unit_id, s32 facing_nibble) {
    map_unit_t* unit = map_unit_get(map_unit_id);
    s32 animation;
    s16 facing;

    if (unit != 0) {
        animation = unit->animation.sequence_id >> 1;
        facing = facing_nibble << 8;
        unit->facing = facing;
        if (animation < ANIMATION_IDLE) {
            animation = ANIMATION_STAND;
        }
        animation_force_with_facing(animation, facing, unit);
        return map_unit_id;
    }
    return -1;
}

/* The unit's camera_facing_sixteenth modulo 16, or -1 when no record has the runtime
 * id. No retail code calls it. */
s16 map_unit_get_camera_facing_sixteenth(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        return unit->camera_facing_sixteenth % 0x10;
    }
    return -1;
}

/* Round every unit's facing to the nearest of the four directions. */
void map_unit_facing_snap_to_quadrants(void) {
    map_unit_t* unit = g_map_unit_list_head;
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

/* Shows a unit's shadow (UnitShadow event instruction). Returns 0 when no unit has the map unit id. */
s32 sprite_map_unit_show_shadow(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        unit->shadow_graphic_trigger = 1;
        return 1;
    }
    return 0;
}

/* Hides a unit's shadow (UnitShadow event instruction). Returns 0 when no unit
 * has the map unit id. */
s32 sprite_map_unit_hide_shadow(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        unit->shadow_graphic_trigger = 0;
        return 1;
    }
    return 0;
}

/* Start a change to map_id (MapJumpingOut), saving the current game state: the
 * screen darkens by 0x100 / duration per frame (a duration of 0 takes one frame),
 * the map loads, the screen fades back in and the saved state resumes. */
void battle_state_map_jumping_out_start(s32 map_id, s32 duration) {
    g_map_id = map_id;
    g_battle_state_previous = g_battle_state;
    g_battle_state = BATTLE_STATE_MAP_JUMPING_OUT;
    g_fade_step = duration != 0 ? 0x100 / duration : 0x100;
}

/* Start an event's change to map_id (MapJumpingOut2), saving the current game
 * state: the screen darkens by 0x100 / duration per frame (a duration of 0 takes
 * one frame), the map loads, and the saved state resumes behind a black screen
 * until battle_state_change_map_jumping_in_start fades it in. */
void battle_state_change_map_jumping_out_start(s32 map_id, s32 duration) {
    g_map_id = map_id;
    g_battle_state_previous = g_battle_state;
    g_battle_state = BATTLE_STATE_MAP_JUMPING_OUT_2;
    g_fade_step = duration != 0 ? 0x100 / duration : 0x100;
}

/* Start the fade-in that ends an event's map change (MapJumpingIn2), saving the
 * current game state to return to. The screen lightens by 0x100 / duration per
 * frame; a duration of 0 takes one frame. */
void battle_state_change_map_jumping_in_start(s32 duration) {
    g_battle_state_previous = g_battle_state;
    g_battle_state = BATTLE_STATE_MAP_JUMPING_IN_2;
    g_fade_step = duration != 0 ? 0x100 / duration : 0x100;
}

/* Leave the battle for the world map: enter CloseBattle at two vertical blanks
 * per frame with a fade step of 0x100 / duration (0x100 for a duration of 0). */
void battle_state_close_battle_to_world_enter(s32 duration) {
    g_system_vsync_interval = 2;
    g_battle_state = BATTLE_STATE_CLOSE_BATTLE;
    g_fade_step = duration != 0 ? 0x100 / duration : 0x100;
    g_game_flow_state = SYSTEM_GAME_FLOW_WORLD;
}

/* The number of records in the map unit list. No retail code calls it. */
s32 map_unit_count(void) {
    s32 count = 0;
    map_unit_t* unit = g_map_unit_list_head;
    while (unit != 0) {
        unit = unit->previous;
        count++;
    }
    return count;
}

/* The integer world position of the unit with this map unit id, or 0 for a missing unit.
 * map_unit_copy_world_coords and map_unit_project_screen_position test
 * the result against -1 instead. */
SVECTOR* map_unit_get_world_coords(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit == 0) {
        return 0;
    }
    return &unit->world_coords;
}

/* The integer world position of the unit in slot battle_unit_id, or 0. No retail code calls
 * it. */
SVECTOR* map_unit_get_world_coords_by_battle_unit_id(u32 battle_unit_id) {
    map_unit_t* unit = map_unit_get_by_battle_unit_id(battle_unit_id);

    if (unit == 0) {
        return 0;
    }
    return &unit->world_coords;
}

/* Store the center of a unit's tile in coords->vx and coords->vz and its world height in
 * coords->vy. Returns 0 when the battle unit id has no map unit record, otherwise 1. The tile lookup's
 * result is unused, but the call is part of the retail instruction stream. */
s32 map_unit_get_tile_center_height_by_battle_unit_id(u32 battle_unit_id, SVECTOR* coords) {
    map_unit_t* unit = map_unit_get_by_battle_unit_id(battle_unit_id);

    if (unit != 0) {
        map_get_tile(unit->battle_unit->x, unit->battle_unit->position.y, unit->battle_unit->position.higher_elevation);
        coords->vx = unit->battle_unit->x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        coords->vz = unit->battle_unit->position.y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        coords->vy = map_unit_calculate_height_from_battle_tile(unit);
        return 1;
    }
    return 0;
}

/* The depth point (camera_get_unbiased_tile_depth_point) of a unit's world position. A
 * missing unit is not checked. No retail code calls it. */
void camera_map_unit_get_depth_point(u32 map_unit_id, SVECTOR* coords) {
    camera_get_unbiased_tile_depth_point(map_unit_get_world_coords(map_unit_id & 0xffff), coords);
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
 * map unit id without a record raises a pointer exception and returns 0. */
s32 map_unit_get_map_coords(u32 map_unit_id, SVECTOR* coords) {
    map_unit_t* unit = g_map_unit_list_head;

    for (; unit != 0; unit = unit->previous) {
        if (unit->map_unit_id == (map_unit_id & 0xffff)) {
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
s32 move_start_walk_to(s32 map_unit_id, const map_tile_coordinates_t* destination, s32 flags, s32 jump, s32 speed) {
    map_unit_t* unit = map_unit_get(map_unit_id);
    move_path_t* path;

    if (unit != 0) {
        path = move_path_calculate_walk_to(
            flags, jump, unit->map_x, unit->map_y, unit->map_layer, destination->x, destination->y, destination->layer);
        if (path != 0) {
            unit->path = *path;
            unit->walk_speed = speed * 2;
            unit->step_speed = 0x2000;
            unit->movement_path_offset = 0;
            map_unit_rider_dismount_and_update_display(unit);
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
s32 move_start_jump(u32 map_unit_id, s32 direction, s32 jump_length) {
    map_unit_t* unit = map_unit_get(map_unit_id);

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
        map_unit_rider_dismount_and_update_display(unit);
    } else {
        system_handle_pointer_exception(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
        return 0;
    }
    return 1;
}

/* Seats one unit on another by map unit id (map_unit_rider_seat).
 * Returns 0 when either record is missing. No retail code calls it. */
s32 map_unit_mount(u32 rider_map_unit_id, u32 mount_map_unit_id) {
    map_unit_t* rider = map_unit_get(rider_map_unit_id);
    map_unit_t* mount = map_unit_get(mount_map_unit_id);

    if ((rider != 0) && (mount != 0)) {
        map_unit_rider_seat(rider, mount);
        return 1;
    }
    return 0;
}

/* A second, identical copy of map_unit_add_event_offset, placed just before it.
 * No retail code calls it. */
s32 map_unit_add_event_offset_unused(u32 map_unit_id, const SVECTOR* amount) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        unit->event_offset.vx = unit->event_offset.vx + amount->vx;
        unit->event_offset.vy = unit->event_offset.vy + amount->vy;
        unit->event_offset.vz = unit->event_offset.vz + amount->vz;
    } else {
        system_handle_pointer_exception(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
        return 0;
    }
    return 1;
}

/* Add `amount` to a unit's event offset. The copy every caller uses;
 * map_unit_add_event_offset_unused is the same routine compiled twice. */
s32 map_unit_add_event_offset(u32 map_unit_id, const SVECTOR* amount) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        unit->event_offset.vx = unit->event_offset.vx + amount->vx;
        unit->event_offset.vy = unit->event_offset.vy + amount->vy;
        unit->event_offset.vz = unit->event_offset.vz + amount->vz;
    } else {
        system_handle_pointer_exception(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
        return 0;
    }
    return 1;
}

/* The unit's event offset (moved by map_unit_add_event_offset). A missing unit
 * raises a pointer exception and returns 0. */
const SVECTOR* map_unit_get_event_offset(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit == 0) {
        system_handle_pointer_exception(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
        return 0;
    }
    return &unit->event_offset;
}

/* Warp a unit to `tile` facing `facing` (in quarter turns), for event placement and warps:
 * clears its event, sequence and screen offsets, sets both its current and destination tile and
 * recomputes its world position from the tile center and slope height. A unit with battle stats
 * also re-registers its tile there; that call passes the 16-bit facing, whose low byte is always
 * 0, so the stats record facing 0 (south) whatever was asked. Returns 0 when no record has the
 * map unit id. */
s32 map_unit_place_in_new_location(u32 map_unit_id, const map_tile_coordinates_t* tile, s32 facing) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        unit->event_offset.vz = 0;
        unit->event_offset.vy = 0;
        unit->event_offset.vx = 0;
        unit->seq_offset.vz = 0;
        unit->seq_offset.vy = 0;
        unit->seq_offset.vx = 0;
        unit->screen_offset.vz = 0;
        unit->screen_offset.vy = 0;
        unit->screen_offset.vx = 0;
        unit->map_x = unit->movement.destination_x = tile->x;
        unit->map_y = unit->movement.destination_y = tile->y;
        unit->map_layer = unit->movement.destination_layer = tile->layer;
        unit->facing = facing << 10;
        unit->world_coords.vx = tile->x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        unit->world_coords.vz = tile->y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        unit->world_coords.vy = map_calculate_slope_height(&unit->world_coords, unit->map_layer);
        unit->real.vx = unit->world_coords.vx * ONE;
        unit->real.vz = unit->world_coords.vz * ONE;
        unit->real.vy = unit->world_coords.vy * ONE;
        if (unit->battle_unit != 0) {
            battle_unit_set_tile_position(
                unit->battle_unit->battle_unit_id, unit->map_x, unit->map_y, unit->map_layer, unit->facing);
        }
        return 1;
    }
    return 0;
}

/* Return 1 when a battle unit with this map unit id exists, else 0. */
s32 map_unit_exists(u32 map_unit_id) {
    return map_unit_get(map_unit_id) != 0;
}

/* Sets UNIT_MOTION_FLAG_SUPPRESS_FACING_UPDATE on a unit and clears its 16-bit sprite
 * display flags. */
void map_unit_set_facing_update_suppression(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        unit->sprite_display_flags = 0;
        unit->movement.suppress_facing_update = 1;
    }
}

/* Clears UNIT_MOTION_FLAG_SUPPRESS_FACING_UPDATE on a unit; unlike the set function it leaves
 * the sprite display flags alone. */
void map_unit_clear_facing_update_suppression(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        unit->movement.suppress_facing_update = 0;
    }
}

/* Mirror a unit's sprite (MirrorSprite event instruction, mode 1). */
void sprite_map_unit_set_horizontal_flip(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        unit->horizontal_flip_flag = SPRITE_DISPLAY_HORIZONTAL_FLIP;
    }
}

/* Undo MirrorSprite's mirroring of a unit's sprite. */
void sprite_map_unit_clear_horizontal_flip(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        unit->horizontal_flip_flag = 0;
    }
}

/* Stop tile and status palette refreshes on a unit: sprite_palette_update skips
 * it while this is set. */
void map_unit_set_palette_update_suppression(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        unit->movement.suppress_palette_update = 1;
    }
}

/* Let tile and status palette refreshes reach a unit again; the counterpart of
 * map_unit_set_palette_update_suppression. */
void map_unit_clear_palette_update_suppression(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        unit->movement.suppress_palette_update = 0;
    }
}

/* Reloads a unit's palette with its weather and time-of-day tint and no tile
 * effect level, for the ResetPalette event instruction. An absent unit is
 * skipped. */
void sprite_map_unit_reset_palette(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        sprite_palette_tint_unit(unit, 0, 1);
    }
}

/* Silence a unit's sounds and landing effects (map_unit_play_sfx and the
 * landing handlers check it). */
void map_unit_set_movement_effect_suppression(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        unit->movement.suppress_sfx_and_landing_effects = 1;
    }
}

/* Restore a unit's sounds and landing effects; the counterpart of
 * map_unit_set_movement_effect_suppression. */
void map_unit_clear_movement_effect_suppression(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        unit->movement.suppress_sfx_and_landing_effects = 0;
    }
}

/* The battle unit slot of a map unit record, or -1 when the record has no battle
 * stats. A map unit id without a record raises a pointer exception and also returns
 * -1. */
s32 map_unit_get_battle_unit_id(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        battle_unit_t* stats = unit->battle_unit;

        if (stats != 0) {
            return stats->battle_unit_id;
        }
    } else {
        system_handle_pointer_exception(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
    }
    return -1;
}

/* The map unit id of the record whose battle stats occupy unit slot battle_unit_id, or -1. */
s32 map_unit_get_id_by_battle_unit_id(u32 battle_unit_id) {
    map_unit_t* unit = g_map_unit_list_head;

    while (unit != 0) {
        battle_unit_t* stats = unit->battle_unit;

        if (stats != 0 && stats->battle_unit_id == battle_unit_id) {
            return unit->map_unit_id;
        }
        unit = unit->previous;
    }
    return -1;
}

/* The sprite sheet VRAM slot of the unit in slot battle_unit_id, or -1. */
s32 map_unit_get_sprite_sheet_slot_by_battle_unit_id(u32 battle_unit_id) {
    map_unit_t* unit = g_map_unit_list_head;

    while (unit != 0) {
        battle_unit_t* stats = unit->battle_unit;

        if (stats != 0 && stats->battle_unit_id == battle_unit_id) {
            return unit->spritesheet_vram_slot;
        }
        unit = unit->previous;
    }
    return -1;
}

/* The command action record (command_state.command.data.action) of the unit in slot
 * battle_unit_id, or 0 when it has no map unit record. */
action_command_t* map_unit_get_action_command_by_battle_unit_id(u32 battle_unit_id) {
    map_unit_t* unit = map_unit_get_by_battle_unit_id(battle_unit_id);

    if (unit == 0) {
        return 0;
    }
    return &unit->command_state.command.data.action;
}

/* 1 while the map unit still has a movement path. A missing unit raises a pointer
 * exception and reports 0. */
s32 move_map_unit_is_moving(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit == 0) {
        system_handle_pointer_exception(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
        return 0;
    }
    return unit->path.count != 0;
}

/* TempWeapon event instruction: sprite_set_weapon_graphic for a
 * map unit id. An absent unit is skipped. */
void sprite_map_unit_set_weapon_graphic(u32 item_id, u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        sprite_set_weapon_graphic(item_id, unit);
    }
}

/* AddGhostUnit: when `map_unit_id` is free, restart the unit graphics load queue with one
 * descriptor for a sprite with no unit stats at the given tile and facing, and return
 * 1; return 0 when the id is taken. */
s32 script_opcode_47_add_ghost_unit(
    s32 map_x, s32 map_y, s32 map_level, s16 facing, s16 spritesheet_id, s32 map_unit_id, s32 flags) {
    s32 ret;

    if (map_unit_get(map_unit_id) != 0) {
        ret = 0;
    } else {
        g_unit_graphics_load_queue.count = 0;
        g_sprite_load_state.file_kind = SPRITE_LOAD_KIND_FIRST;
        g_sprite_load_state.phase = SPRITE_LOAD_PHASE_START_READ;
        g_sprite_load_state.index = 0;
        g_sprite_load_state.buffer = 0;
        sprite_queue_unit_graphics_load(map_x, map_y, map_level, facing, spritesheet_id, 0, map_unit_id, 0, flags);
        ret = 1;
    }
    return ret;
}

/* Restarts the unit graphics loader with one descriptor, for a unit an event adds
 * during the battle: the unit is put on the field
 * (battle_unit_put_on_field) and queued at its map position
 * and facing with its sheet and palette, the caller's flags and map unit id 0xff,
 * which takes the first free map unit slot. Always returns 1. */
s32 sprite_start_unit_graphics_load(s32 battle_unit_id, s32 flags) {
    battle_unit_t* unit;

    g_unit_graphics_load_queue.count = 0;
    g_sprite_load_state.file_kind = SPRITE_LOAD_KIND_FIRST;
    g_sprite_load_state.phase = SPRITE_LOAD_PHASE_START_READ;
    g_sprite_load_state.index = 0;
    g_sprite_load_state.buffer = 0;
    unit = battle_unit_get(battle_unit_id);
    battle_unit_put_on_field(battle_unit_id);
    sprite_queue_unit_graphics_load(unit->x, unit->position.y, unit->position.higher_elevation, unit->position.facing,
        unit->spritesheet_id, unit->job_portrait_palette, 0xFF, unit, flags);
    return 1;
}

/* Advance the queued unit graphics load as far as it goes this frame: a step that ends on a step
 * boundary (2) continues at once, and the loop stops on 1 (a read in progress) or 0 (every
 * descriptor done), which it returns. */
s32 sprite_poll_unit_graphics_load(void) {
    s32 status;

    do {
        status = sprite_update_queued_load();
    } while (status == 2);
    return status;
}

/* Makes a unit visible: enables its drawing, triggers its sprite graphic and,
 * when it has battle stats, puts it on the field (battle_unit_put_on_field).
 * Returns 0 when no record has the map unit id. */
s32 map_unit_show(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);
    battle_unit_t* stats;

    if (unit != 0) {
        stats = unit->battle_unit;
        unit->draw_enabled = 1;
        unit->animation.trigger = 1;
        if (stats != 0) {
            battle_unit_put_on_field(stats->battle_unit_id);
        }
        return 1;
    }
    return 0;
}

/* Hides a unit: disables its drawing and sprite trigger, undoing
 * map_unit_show (Erase event instruction). Returns 0 when no record
 * has the map unit id. */
s32 map_unit_hide(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        unit->draw_enabled = 0;
        unit->animation.trigger = 0;
        return 1;
    }
    return 0;
}

/* Hides every unit that has the Jump status. */
void map_unit_hide_jumping(void) {
    map_unit_t* unit = g_map_unit_list_head;

    while (unit != 0) {
        if ((unit->status_flags_5_6 & UNIT_STATUS_JUMP) != 0) {
            map_unit_hide(unit->map_unit_id);
        }
        unit = unit->previous;
    }
}

/* map_unit_remove by map unit id. Returns 1, or 0 after raising a pointer
 * exception when no record has the id. */
s32 map_unit_remove_by_id(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        map_unit_remove(unit);
        return 1;
    }
    system_handle_pointer_exception(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
    return 0;
}

/* Calls map_unit_start_fade_out with the same
 * arguments; the event instructions that remove a unit start its fade-out here. */
void map_unit_start_event_fade_out(s32 map_unit_id, s32 fade_step, s16 red, s16 green, s16 blue) {
    map_unit_start_fade_out(map_unit_id, fade_step, red, green, blue);
}
