/* Expose scene and live-unit commands to event scripts: map transitions, placement, movement, animation, facing and
 * visibility. */

#include "fft/animation.h"
#include "fft/battle_map.h"
#include "fft/battle_state.h"
#include "fft/camera.h"
#include "fft/fade.h"
#include "fft/move.h"
#include "fft/script.h"
#include "fft/sprite.h"
#include "fft/sprite_sheet.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* unit_runtime_display_update as a g_thread_call_target: event threads run it on the
 * main stack. */
void unit_runtime_graphics_update(u32 runtime_id) {
    unit_runtime_display_update(runtime_id);
}

/* animation_stand_set by runtime id; event threads run it on the main
 * stack. */
void animation_runtime_stand_set(u32 runtime_id) {
    animation_stand_set(unit_runtime_get(runtime_id));
}

/* animation_walk_set by runtime id. No retail code
 * calls it. */
void animation_runtime_walk_set(u32 runtime_id) {
    animation_walk_set(unit_runtime_get(runtime_id));
}

/* animation_status_set by runtime id, for event scripts. */
void animation_runtime_status_set(u32 runtime_id) {
    animation_status_set(unit_runtime_get(runtime_id));
}

/* Request `animation` for a unit by runtime id, keeping its facing. */
void animation_runtime_sequence_set(u32 runtime_id, u32 animation) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);
    if (unit != 0)
        animation_facing_store(animation, unit->facing, unit);
}

/* Request `animation` on every unit, keeping its camera-facing quadrant. */
void animation_unit_camera_facings_store(u32 animation) {
    unit_runtime_t* unit = g_unit_list_head;

    while (unit != 0) {
        animation_facing_store(animation, unit->camera_facing_quadrant, unit);
        unit = unit->previous;
    }
}

/* The unit-ready test that script_unit_ready_wait polls. It ignores the unit and always
 * reports ready, so that wait never yields. */
s32 unit_is_ready(s32 unused_runtime_id) {
    return 1;
}

/* The unit's facing in quarter turns (0..3), or -1 when no record has the runtime
 * id. */
s16 unit_runtime_facing_quadrant_get(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit != 0) {
        return (unit->facing / UNIT_FACING_QUARTER_TURN) % 4;
    }
    return -1;
}

/* Faces a unit in a direction given in quarter turns and restarts its animation
 * with that facing, as unit_runtime_facing_sixteenth_set does for
 * sixteenths. No retail code calls it. */
s32 unit_runtime_facing_quadrant_set(u32 runtime_id, s32 facing_quadrant) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);
    s32 animation;
    s16 facing;

    if (unit != 0) {
        animation = unit->animation.sequence_id >> 1;
        facing = facing_quadrant << 10;
        unit->facing = facing;
        if (animation < ANIMATION_IDLE) {
            animation = ANIMATION_STAND;
        }
        animation_facing_force(animation, facing, unit);
        return runtime_id;
    }
    return -1;
}

/* The unit's camera_facing_quadrant modulo 4, or -1 when no record has the runtime
 * id. No retail code calls it. */
s16 unit_runtime_camera_facing_get(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit != 0) {
        return unit->camera_facing_quadrant % 4;
    }
    return -1;
}

/* The unit's camera_facing_quadrant modulo 4, or -1 when no runtime record has the
 * battle id. */
s16 unit_battle_camera_facing_get(u32 battle_id) {
    unit_runtime_t* unit = unit_battle_get(battle_id);

    if (unit != 0) {
        return unit->camera_facing_quadrant % 4;
    }
    return -1;
}

/* The unit's facing in sixteenths of a turn (0..15), or -1 when no record has the
 * runtime id. */
s32 unit_runtime_facing_sixteenth_get(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit != 0) {
        return (unit->facing / 0x100) % 0x10;
    }
    return -1;
}

/* Faces a unit in a direction given in sixteenths of a turn and restarts its
 * current animation with that facing; an animation below idle becomes standing.
 * Returns the runtime id, or -1 when no record has it. */
s32 unit_runtime_facing_sixteenth_set(u32 runtime_id, s32 facing_nibble) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);
    s32 animation;
    s16 facing;

    if (unit != 0) {
        animation = unit->animation.sequence_id >> 1;
        facing = facing_nibble << 8;
        unit->facing = facing;
        if (animation < ANIMATION_IDLE) {
            animation = ANIMATION_STAND;
        }
        animation_facing_force(animation, facing, unit);
        return runtime_id;
    }
    return -1;
}

/* The unit's camera_facing_sixteenth modulo 16, or -1 when no record has the runtime
 * id. No retail code calls it. */
s16 unit_runtime_camera_facing_sixteenth_get(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit != 0) {
        return unit->camera_facing_sixteenth % 0x10;
    }
    return -1;
}

/* Round every unit's facing to the nearest of the four directions. */
void unit_facing_quadrants_snap(void) {
    unit_runtime_t* unit = g_unit_list_head;
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
s32 sprite_runtime_shadow_show(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit != 0) {
        unit->shadow_graphic_trigger = 1;
        return 1;
    }
    return 0;
}

/* Hides a unit's shadow (UnitShadow event instruction). Returns 0 when no unit
 * has the runtime id. */
s32 sprite_runtime_shadow_hide(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

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
    g_battle_map_id = map_id;
    g_battle_state_previous = g_battle_state;
    g_battle_state = BATTLE_STATE_MAP_JUMPING_OUT;
    g_fade_step = duration != 0 ? 0x100 / duration : 0x100;
}

/* Start an event's change to map_id (MapJumpingOut2), saving the current game
 * state: the screen darkens by 0x100 / duration per frame (a duration of 0 takes
 * one frame), the map loads, and the saved state resumes behind a black screen
 * until battle_state_start_change_map_jump_in fades it in. */
void battle_state_start_change_map_jump_out(s32 map_id, s32 duration) {
    g_battle_map_id = map_id;
    g_battle_state_previous = g_battle_state;
    g_battle_state = BATTLE_STATE_MAP_JUMPING_OUT_2;
    g_fade_step = duration != 0 ? 0x100 / duration : 0x100;
}

/* Start the fade-in that ends an event's map change (MapJumpingIn2), saving the
 * current game state to return to. The screen lightens by 0x100 / duration per
 * frame; a duration of 0 takes one frame. */
void battle_state_start_change_map_jump_in(s32 duration) {
    g_battle_state_previous = g_battle_state;
    g_battle_state = BATTLE_STATE_MAP_JUMPING_IN_2;
    g_fade_step = duration != 0 ? 0x100 / duration : 0x100;
}

/* Leave the battle for the world map: enter CloseBattle at two vertical blanks
 * per frame with a fade step of 0x100 / duration (0x100 for a duration of 0). */
void battle_state_enter_close_battle_to_world(s32 duration) {
    g_system_vsync_interval = 2;
    g_battle_state = BATTLE_STATE_CLOSE_BATTLE;
    g_fade_step = duration != 0 ? 0x100 / duration : 0x100;
    g_game_flow_state = SYSTEM_GAME_FLOW_WORLD;
}

/* The number of records in the runtime unit list. No retail code calls it. */
s32 unit_runtime_count(void) {
    s32 count = 0;
    unit_runtime_t* unit = g_unit_list_head;
    while (unit != 0) {
        unit = unit->previous;
        count++;
    }
    return count;
}

/* The integer world position of the unit with this runtime id, or 0 for a missing unit.
 * unit_world_coords_copy and unit_screen_position_project test
 * the result against -1 instead. */
SVECTOR* unit_runtime_world_coords_get(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit == 0) {
        return 0;
    }
    return &unit->world_coords;
}

/* The integer world position of the unit in slot battle_id, or 0. No retail code calls
 * it. */
SVECTOR* unit_battle_world_coords_get(u32 battle_id) {
    unit_runtime_t* unit = unit_battle_get(battle_id);

    if (unit == 0) {
        return 0;
    }
    return &unit->world_coords;
}

/* Store the center of a unit's tile in coords->vx and coords->vz and its world height in
 * coords->vy. Returns 0 when the battle id has no runtime record, otherwise 1. The tile lookup's
 * result is unused, but the call is part of the retail instruction stream. */
s32 unit_battle_tile_center_height_get(u32 battle_id, SVECTOR* coords) {
    unit_runtime_t* unit = unit_battle_get(battle_id);

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
    camera_get_tile_depth_point_unbiased(unit_runtime_world_coords_get(runtime_id & 0xffff), coords);
}

/* Projects a world position to screen x and y. Rebuilds the camera matrices first
 * and leaves them loaded in the GTE. */
void camera_project_world_coords_to_screen(SVECTOR* world_coords, SVECTOR* screen_coords) {
    VECTOR transformed_coords;
    s32 flag;

    camera_matrices_update(&g_camera_matrix, &g_camera_render_state, &g_camera_screen_offset, &g_camera_zoom);
    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    RotTrans(world_coords, &transformed_coords, &flag);
    screen_coords->vx = transformed_coords.vx;
    screen_coords->vy = transformed_coords.vy;
}

/* Writes a unit's tile as three 16-bit values in the order x, layer, y and returns 1. A
 * runtime id without a record raises a pointer exception and returns 0. */
s32 unit_runtime_map_coords_get(u32 runtime_id, SVECTOR* coords) {
    unit_runtime_t* unit = g_unit_list_head;

    for (; unit != 0; unit = unit->previous) {
        if (unit->runtime_id == (runtime_id & 0xffff)) {
            coords->vx = unit->map_x;
            coords->vz = unit->map_y;
            coords->vy = unit->map_layer;
            return 1;
        }
    }
    system_pointer_exception_handle(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
    return 0;
}

/* Path a unit from its tile to `destination` (the WalkTo event instruction) and start it walking.
 * `flags` and `jump` go to the pathfinder; the unit's walk speed is twice `speed`. A riding unit
 * dismounts. Returns 1 when a path was found, 0 when none was; a missing unit raises
 * a pointer exception and returns 0. */
s32 move_start_walk_to(
    s32 runtime_id, const battle_map_tile_coordinates_t* destination, s32 flags, s32 jump, s32 speed) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);
    move_path_t* path;

    if (unit != 0) {
        path = move_path_calculate_walk_to(
            flags, jump, unit->map_x, unit->map_y, unit->map_layer, destination->x, destination->y, destination->layer);
        if (path != 0) {
            unit->path = *path;
            unit->walk_speed = speed * 2;
            unit->step_speed = 0x2000;
            unit->movement_path_offset = 0;
            unit_rider_dismount_display_update(unit);
            return 1;
        }
    } else {
        system_pointer_exception_handle(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
    }
    return 0;
}

/* Start a unit on a one-step path: a jump of jump_length extra tiles in `direction` on its own
 * layer, at full speed. The Jump event instruction uses it. A missing unit raises a pointer
 * exception and returns 0. */
s32 move_start_jump_by_runtime_id(u32 runtime_id, s32 direction, s32 jump_length) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

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
        unit_rider_dismount_display_update(unit);
    } else {
        system_pointer_exception_handle(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
        return 0;
    }
    return 1;
}

/* Seats one unit on another by runtime id (unit_rider_seat).
 * Returns 0 when either record is missing. No retail code calls it. */
s32 unit_runtime_mount(u32 rider_runtime_id, u32 mount_runtime_id) {
    unit_runtime_t* rider = unit_runtime_get(rider_runtime_id);
    unit_runtime_t* mount = unit_runtime_get(mount_runtime_id);

    if ((rider != 0) && (mount != 0)) {
        unit_rider_seat(rider, mount);
        return 1;
    }
    return 0;
}

/* A second, identical copy of unit_add_event_offset_by_runtime_id, placed just before it.
 * No retail code calls it. */
s32 unit_add_event_offset_by_runtime_id_unused(u32 runtime_id, const SVECTOR* amount) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit != 0) {
        unit->event_offset.vx = unit->event_offset.vx + amount->vx;
        unit->event_offset.vy = unit->event_offset.vy + amount->vy;
        unit->event_offset.vz = unit->event_offset.vz + amount->vz;
    } else {
        system_pointer_exception_handle(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
        return 0;
    }
    return 1;
}

/* Add `amount` to a unit's event offset. The copy every caller uses;
 * unit_add_event_offset_by_runtime_id_unused is the same routine compiled twice. */
s32 unit_add_event_offset_by_runtime_id(u32 runtime_id, const SVECTOR* amount) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit != 0) {
        unit->event_offset.vx = unit->event_offset.vx + amount->vx;
        unit->event_offset.vy = unit->event_offset.vy + amount->vy;
        unit->event_offset.vz = unit->event_offset.vz + amount->vz;
    } else {
        system_pointer_exception_handle(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
        return 0;
    }
    return 1;
}

/* The unit's event offset (moved by unit_add_event_offset_by_runtime_id). A missing unit
 * raises a pointer exception and returns 0. */
const SVECTOR* unit_get_event_offset_by_runtime_id(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit == 0) {
        system_pointer_exception_handle(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
        return 0;
    }
    return &unit->event_offset;
}

/* Warp a unit to `tile` facing `facing` (in quarter turns), for event placement and warps:
 * clears its event, sequence and screen offsets, sets both its current and destination tile and
 * recomputes its world position from the tile center and slope height. A unit with battle stats
 * also re-registers its tile there; that call passes the 16-bit facing, whose low byte is always
 * 0, so the stats record facing 0 (south) whatever was asked. Returns 0 when no record has the
 * runtime id. */
s32 unit_place_in_new_location(u32 runtime_id, const battle_map_tile_coordinates_t* tile, s32 facing) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

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
        unit->world_coords.vx = tile->x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        unit->world_coords.vz = tile->y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        unit->world_coords.vy = battle_map_slope_height_calculate(&unit->world_coords, unit->map_layer);
        unit->real.vx = unit->world_coords.vx * ONE;
        unit->real.vz = unit->world_coords.vz * ONE;
        unit->real.vy = unit->world_coords.vy * ONE;
        if (unit->stats != 0) {
            unit_set_tile_position(unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_layer, unit->facing);
        }
        return 1;
    }
    return 0;
}

/* Return 1 when a battle unit with this runtime id exists, else 0. */
s32 unit_has_runtime_id(u32 runtime_id) {
    return unit_runtime_get(runtime_id) != 0;
}

/* Sets UNIT_MOTION_FLAG_SUPPRESS_FACING_UPDATE on a unit and clears its 16-bit sprite
 * display flags. */
void unit_set_facing_update_suppression(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit != 0) {
        unit->sprite_display_flags = 0;
        unit->movement.suppress_facing_update = 1;
    }
}

/* Clears UNIT_MOTION_FLAG_SUPPRESS_FACING_UPDATE on a unit; unlike the set function it leaves
 * the sprite display flags alone. */
void unit_clear_facing_update_suppression(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit != 0) {
        unit->movement.suppress_facing_update = 0;
    }
}

/* Mirror a unit's sprite (MirrorSprite event instruction, mode 1). */
void sprite_set_horizontal_flip_flag(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit != 0) {
        unit->horizontal_flip_flag = SPRITE_DISPLAY_HORIZONTAL_FLIP;
    }
}

/* Undo MirrorSprite's mirroring of a unit's sprite. */
void sprite_clear_horizontal_flip_flag(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit != 0) {
        unit->horizontal_flip_flag = 0;
    }
}

/* Stop tile and status palette refreshes on a unit: sprite_palette_check_tile_status_mod skips
 * it while this is set. */
void unit_set_palette_update_suppression(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit != 0) {
        unit->movement.suppress_palette_update = 1;
    }
}

/* Let tile and status palette refreshes reach a unit again; the counterpart of
 * unit_set_palette_update_suppression. */
void unit_clear_palette_update_suppression(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit != 0) {
        unit->movement.suppress_palette_update = 0;
    }
}

/* Reloads a unit's palette with its weather and time-of-day tint and no tile
 * effect level, for the ResetPalette event instruction. An absent unit is
 * skipped. */
void sprite_runtime_palette_reset(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit != 0) {
        sprite_palette_tint_unit(unit, 0, 1);
    }
}

/* Silence a unit's sounds and landing effects (unit_sfx_play and the
 * landing handlers check it). */
void unit_set_movement_effect_suppression(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit != 0) {
        unit->movement.suppress_sfx_and_landing_effects = 1;
    }
}

/* Restore a unit's sounds and landing effects; the counterpart of
 * unit_set_movement_effect_suppression. */
void unit_clear_movement_effect_suppression(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit != 0) {
        unit->movement.suppress_sfx_and_landing_effects = 0;
    }
}

/* The battle unit slot of a runtime record, or -1 when the record has no battle
 * stats. A runtime id without a record raises a pointer exception and also returns
 * -1. */
s32 unit_runtime_battle_id_get(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit != 0) {
        unit_stats_t* stats = unit->stats;

        if (stats != 0) {
            return stats->battle_id;
        }
    } else {
        system_pointer_exception_handle(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
    }
    return -1;
}

/* The runtime id of the record whose battle stats occupy unit slot battle_id, or -1. */
s32 unit_battle_runtime_id_get(u32 battle_id) {
    unit_runtime_t* unit = g_unit_list_head;

    while (unit != 0) {
        unit_stats_t* stats = unit->stats;

        if (stats != 0 && stats->battle_id == battle_id) {
            return unit->runtime_id;
        }
        unit = unit->previous;
    }
    return -1;
}

/* The sprite sheet VRAM slot of the unit in slot battle_id, or -1. */
s32 sprite_sheet_get_slot_by_battle_id(u32 battle_id) {
    unit_runtime_t* unit = g_unit_list_head;

    while (unit != 0) {
        unit_stats_t* stats = unit->stats;

        if (stats != 0 && stats->battle_id == battle_id) {
            return unit->spritesheet_vram_slot;
        }
        unit = unit->previous;
    }
    return -1;
}

/* The command action record (command_state.command.data.action) of the unit in slot
 * battle_id, or 0 when it has no runtime record. */
action_command_t* unit_battle_action_command_get(u32 battle_id) {
    unit_runtime_t* unit = unit_battle_get(battle_id);

    if (unit == 0) {
        return 0;
    }
    return &unit->command_state.command.data.action;
}

/* 1 while the runtime unit still has a movement path. A missing unit raises a pointer
 * exception and reports 0. */
s32 move_is_unit_moving_by_runtime_id(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit == 0) {
        system_pointer_exception_handle(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
        return 0;
    }
    return unit->path.count != 0;
}

/* TempWeapon event instruction: sprite_thrown_item_palette_set for a
 * runtime id. An absent unit is skipped. */
void sprite_runtime_thrown_item_palette_set(u32 item_id, u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit != 0) {
        sprite_thrown_item_palette_set(item_id, unit);
    }
}

/* AddGhostUnit: when `runtime_id` is free, restart the unit graphics load queue with one
 * descriptor for a sprite with no unit stats at the given tile and facing, and return
 * 1; return 0 when the id is taken. */
s32 script_opcode_47_add_ghost_unit(
    s32 map_x, s32 map_y, s32 map_level, s16 facing, s16 spritesheet_id, s32 runtime_id, s32 flags) {
    s32 ret;

    if (unit_runtime_get(runtime_id) != 0) {
        ret = 0;
    } else {
        g_unit_graphics_load_queue.count = 0;
        g_sprite_load_state.file_kind = SPRITE_LOAD_KIND_FIRST;
        g_sprite_load_state.phase = SPRITE_LOAD_PHASE_START_READ;
        g_sprite_load_state.index = 0;
        g_sprite_load_state.buffer = 0;
        sprite_unit_graphics_load_queue(map_x, map_y, map_level, facing, spritesheet_id, 0, runtime_id, 0, flags);
        ret = 1;
    }
    return ret;
}

/* Restarts the unit graphics loader with one descriptor, for a unit an event adds
 * during the battle: the unit is put on the field
 * (unit_put_on_field) and queued at its map position
 * and facing with its sheet and palette, the caller's flags and runtime id 0xff,
 * which takes the first free runtime slot. Always returns 1. */
s32 sprite_unit_graphics_load_start(s32 battle_id, s32 flags) {
    unit_stats_t* unit;

    g_unit_graphics_load_queue.count = 0;
    g_sprite_load_state.file_kind = SPRITE_LOAD_KIND_FIRST;
    g_sprite_load_state.phase = SPRITE_LOAD_PHASE_START_READ;
    g_sprite_load_state.index = 0;
    g_sprite_load_state.buffer = 0;
    unit = unit_stats_get(battle_id);
    unit_put_on_field(battle_id);
    sprite_unit_graphics_load_queue(unit->x, unit->position.y, unit->position.higher_elevation, unit->position.facing,
        unit->spritesheet_id, unit->job_portrait_palette, 0xFF, unit, flags);
    return 1;
}

/* Advance the queued unit graphics load as far as it goes this frame: a step that ends on a step
 * boundary (2) continues at once, and the loop stops on 1 (a read in progress) or 0 (every
 * descriptor done), which it returns. */
s32 sprite_unit_graphics_load_poll(void) {
    s32 status;

    do {
        status = sprite_queued_load_update();
    } while (status == 2);
    return status;
}

/* Makes a unit visible: enables its drawing, triggers its sprite graphic and,
 * when it has battle stats, puts it on the field (unit_put_on_field).
 * Returns 0 when no record has the runtime id. */
s32 unit_runtime_show(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);
    unit_stats_t* stats;

    if (unit != 0) {
        stats = unit->stats;
        unit->draw_enabled = 1;
        unit->animation.trigger = 1;
        if (stats != 0) {
            unit_put_on_field(stats->battle_id);
        }
        return 1;
    }
    return 0;
}

/* Hides a unit: disables its drawing and sprite trigger, undoing
 * unit_runtime_show (Erase event instruction). Returns 0 when no record
 * has the runtime id. */
s32 unit_runtime_hide(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit != 0) {
        unit->draw_enabled = 0;
        unit->animation.trigger = 0;
        return 1;
    }
    return 0;
}

/* Hides every unit that has the Jump status. */
void unit_jumping_hide(void) {
    unit_runtime_t* unit = g_unit_list_head;

    while (unit != 0) {
        if ((unit->status_flags_5_6 & UNIT_STATUS_JUMP) != 0) {
            unit_runtime_hide(unit->runtime_id);
        }
        unit = unit->previous;
    }
}

/* unit_remove by runtime id. Returns 1, or 0 after raising a pointer
 * exception when no record has the id. */
s32 unit_runtime_remove(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit != 0) {
        unit_remove(unit);
        return 1;
    }
    system_pointer_exception_handle(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
    return 0;
}

/* Calls unit_fade_out_start with the same
 * arguments; the event instructions that remove a unit start its fade-out here. */
void unit_event_fade_out_start(s32 runtime_id, s32 fade_step, s16 red, s16 green, s16 blue) {
    unit_fade_out_start(runtime_id, fade_step, red, green, blue);
}
