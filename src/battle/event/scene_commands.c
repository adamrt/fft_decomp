/* Expose scene operations to event scripts and effect callbacks, including effect requests, camera, map, deployment and
 * display controls. */

#include "fft/battle_state.h"
#include "fft/camera.h"
#include "fft/command.h"
#include "fft/cursor.h"
#include "fft/deploy.h"
#include "fft/effect.h"
#include "fft/effect_script.h"
#include "fft/effect_secondary.h"
#include "fft/map.h"
#include "fft/map_load.h"
#include "fft/map_unit.h"
#include "fft/projectile.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/vector.h"
#include "psx/libgte.h"
#include "psx/types.h"

extern u16 g_fade_tint_fade_frames;

/* Start an ability's effect cast by map unit 0 on map unit 1 with a normal hit. No retail
 * code calls it. */
void effect_start_basic_ability(s32 ability_id) {
    effect_context_t effect_data;

    effect_data.target_count = 1;
    effect_data.targets[EFFECT_CASTER_ENTRY].target_type = EFFECT_TARGET_TYPE_UNIT;
    effect_data.targets[EFFECT_CASTER_ENTRY].data.unit.map_unit_id = 0;
    effect_data.targets[0].target_type = EFFECT_TARGET_TYPE_UNIT;
    effect_data.targets[0].data.unit.map_unit_id = 1;
    effect_data.targets[0].data.unit.result_animation = EFFECT_HIT_NORMAL;
    effect_data.spectator_count = 0;
    effect_playback_request(0, ability_id, &effect_data);
}

/* Starts an ability's effect outside an action, on a unit, on a tile or
 * on a unit that is also its caster. */
void effect_start_request(s32 ability_id, s32 target_mode, effect_start_target_data_t target) {
    effect_context_t effect_data;

    effect_data.target_count = 1;
    effect_data.targets[EFFECT_CASTER_ENTRY].target_type = EFFECT_TARGET_TYPE_UNIT;
    effect_data.targets[EFFECT_CASTER_ENTRY].data.unit.map_unit_id = 0;

    if (target_mode == EFFECT_START_ON_UNIT) {
        effect_data.targets[0].target_type = EFFECT_TARGET_TYPE_UNIT;
        effect_data.targets[0].data.unit.map_unit_id = target.map_unit_id;
        effect_data.targets[0].data.unit.result_animation = EFFECT_HIT_NORMAL;
    } else if (target_mode == EFFECT_START_ON_TILE) {
        effect_data.targets[0].target_type = EFFECT_TARGET_TYPE_TILE;
        effect_data.targets[0].data.tile.vx = target.tile->vx;
        effect_data.targets[0].data.tile.vz = target.tile->vz;
        effect_data.targets[0].data.tile.vy = target.tile->vy;
    } else {
        effect_data.targets[0].target_type = EFFECT_TARGET_TYPE_UNIT;
        effect_data.targets[0].data.unit.map_unit_id = target.map_unit_id;
        effect_data.targets[0].data.unit.result_animation = EFFECT_HIT_NORMAL;
        effect_data.targets[EFFECT_CASTER_ENTRY].data.unit.map_unit_id = target.map_unit_id;
    }

    effect_data.spectator_count = 0;
    effect_playback_request(0, ability_id, &effect_data);
}

/* effect_update_file_load as a 0 or 1 result: the event script's EffectStart
 * instruction calls it on the main stack each frame until it returns 0. */
s32 effect_poll_request_load(void) {
    /* No argument is passed: for a load state outside 0..3, effect_update_file_load
     * returns whatever this caller left in $a0. */
    return effect_update_file_load() != 0;
}

/* effect_playback_start as a g_thread_call_target: script_play_effect_and_wait runs it on the main
 * stack. */
void effect_play_request(void) {
    effect_playback_start();
}

/* Return g_battle_animation_wait_state, nonzero while the action's effect is still
 * loading or playing; script_play_effect_and_wait polls it on the main stack. */
s32 battle_animation_get_wait_state(void) {
    return g_battle_animation_wait_state;
}

/* Enter the effect-playback game state, saving the state and vsync interval
 * that battle_state_effect_update restores. Returns 1 when the effect data is
 * not ready yet and the caller must retry. No code calls it. */
s32 battle_state_effect_playback_try_enter(void) {
    s32 previous_state;
    s32 previous_vsync_interval;

    if (effect_update_file_load() != 0) {
        return 1;
    }
    effect_playback_start();
    previous_state = g_battle_state;
    previous_vsync_interval = g_system_vsync_interval;
    g_battle_state = BATTLE_STATE_EFFECT;
    g_system_vsync_interval = 2;
    g_fade_tint_fade_frames = 0;
    g_camera_screen_offset_countdown = 0;
    g_camera_rotation_countdown = 0;
    g_camera_zoom_countdown = 0;
    g_battle_state_previous = previous_state;
    g_system_saved_vsync_interval = previous_vsync_interval;
    return 0;
}

/* Empty, and nothing in the game calls it. */
void battle_state_noop_1(void) {
}

/* Stop the slow-down timer the game loop starts when a frame of the effect or
 * action-execute state takes more than 0x200 hsyncs. */
void system_reset_frame_pacing(void) {
    g_system_frame_pacing_timer = 0;
}

/* Count one more entry into the Status screen (BATTLE's own or DEBUGCHR's for a mounted unit). While the count is
 * exactly 1 the battle state handlers stop animating the units and the camera;
 * status_screen_leave_selection resets it. */
void status_screen_enter_selection(void) {
    if (g_menu_status_screen_selected != 0) {
        g_menu_status_screen_selected++;
    } else {
        g_menu_status_screen_selected = 1;
    }
}

/* Reset the status-screen count to 0, whatever its depth. */
void status_screen_leave_selection(void) {
    g_menu_status_screen_selected = 0;
}

/* Return g_system_vsync_interval, the vertical blanks per frame (1, or 2 during effects and
 * events); event waits divide their frame counts by it. */
s32 system_get_vsync_interval(void) {
    return g_system_vsync_interval;
}

/* Set g_system_vsync_interval, the number of vertical blanks per frame, to
 * 1 or 2. Returns 0, or -1 for any other value. */
s32 system_set_vsync_interval(s32 speed) {
    if (speed == 1 || speed == 2) {
        g_system_vsync_interval = speed;
        return 0;
    }
    return -1;
}

/* Write the cursor tile to `destination` as (x, map level, y). */
void map_store_selected_tile(SVECTOR* destination) {
    vector_set_short(destination, g_cursor_x, g_cursor_layer, g_cursor_y);
}

/* Write the map's width and depth in tiles to `destination` as (x, 0, z). */
void map_store_dimensions(SVECTOR* destination) {
    vector_set_short(destination, g_map_terrain_width, 0, g_map_terrain_depth);
}

/* The tile a unit stands on, by map unit id. The map unit record lookup is not checked:
 * an id without one is read through address 0. */
map_tile_t* map_unit_get_tile(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id & 0xffff);

    return map_get_tile(unit->map_x, unit->map_y, unit->map_layer);
}

/* The tile a unit stands on, by battle unit id. The map unit record lookup is not checked:
 * an id without one is read through address 0. */
map_tile_t* map_unit_get_tile_by_battle_unit_id(u32 battle_unit_id) {
    map_unit_t* unit = map_unit_get_by_battle_unit_id(battle_unit_id & 0xffff);

    return map_get_tile(unit->map_x, unit->map_y, unit->map_layer);
}

/* The tile under a unit's integer world position (28 units per tile) on its map layer. */
map_tile_t* map_unit_get_world_tile(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id & 0xffff);

    return map_get_tile(unit->world_coords.vx / MAP_TILE_SIZE, unit->world_coords.vz / MAP_TILE_SIZE, unit->map_layer);
}

/* Ask the map loader to load what the current map state needs. 1 when the load
 * started, 0 when a file read is still in progress. */
s32 map_start_load(void) {
    return map_update_load(g_map_id, MAP_LOAD_COMMAND_START) != 0;
}

/* Advance the current map's load by one step. Nonzero while it is still loading. */
s32 map_poll_load(void) {
    return map_update_load(g_map_id, MAP_LOAD_COMMAND_POLL);
}

/* Event Use3DObject: start a state row of the map mesh part (map data command
 * 0x80, with flag 1). Always returns 1. */
s32 map_set_object_state(s32 part_index, s32 state_row) {
    map_dispatch_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, part_index, state_row, 1);
    return 1;
}

/* Return the map mesh part's animation state; zero means its animation has ended. */
s32 map_get_object_state(s32 part_index) {
    return map_dispatch_data_command(MAP_DATA_COMMAND_GET_3D_OBJECT_STATE, part_index, 1, 1);
}

/* Start a map texture animation from its first frame (map data command 0x83).
 * Forward object_state unchanged; the dispatcher does not read it for this command.
 * Always returns 1. */
s32 map_start_texture_animation(s32 animation_index, s32 object_state) {
    map_dispatch_data_command(MAP_DATA_COMMAND_START_TEXTURE_ANIMATION, animation_index, object_state, 1);
    return 1;
}

/* Return whether the map texture animation is still playing. */
s32 map_texture_animation_is_active(s32 animation_index) {
    return map_dispatch_data_command(MAP_DATA_COMMAND_GET_TEXTURE_ANIMATION_ACTIVE, animation_index, 1, 1);
}

/* Nonzero while the camera moves: a pan step, a rotation, a tilt or a zoom is in progress. Event
 * code waits for it to return 0. */
s32 camera_is_active(void) {
    if (g_camera_step_vector.vx == 0 && g_camera_step_vector.vy == 0 && g_camera_step_vector.vz == 0
        && g_camera_rotation_action == CAMERA_ROTATION_NONE && g_camera_tilt_action == CAMERA_STEP_NONE
        && g_camera_zoom_action == CAMERA_STEP_NONE) {
        return 0;
    }
    return 1;
}

/* Move the cursor to the unit with this battle unit id. Returns 1 when the unit has
 * runtime data, otherwise 0 with the cursor unchanged. */
s32 cursor_move_to_battle_unit(u32 battle_unit_id) {
    map_unit_t* unit = map_unit_get_by_battle_unit_id(battle_unit_id & 0xFFFF);

    if (unit != 0) {
        cursor_move_to_unit(unit);
        return 1;
    }
    return 0;
}

/* Aim a projectile from one unit at another: set up the direct trajectory to the
 * target's tile and make the target the attacker's only target. Returns 1 when
 * both units exist. Nothing in the game calls it. */
s32 projectile_direct_init_single_target(u32 attacker_id, u32 target_id) {
    map_unit_t* attacker = map_unit_get_by_battle_unit_id(attacker_id);
    map_unit_t* target = map_unit_get_by_battle_unit_id(target_id);
    SVECTOR target_position;

    if (target != 0) {
        vector_set_short(&target_position, target->map_x, target->map_y, target->map_layer);
        projectile_direct_check_to_target(attacker_id, &target_position, target_id);
        if (attacker != 0) {
            attacker->strike.target_count = 1;
            attacker->strike.target_list[0] = target_id;
            return 1;
        }
    }
    return 0;
}

/* effect_secondary_start_teleport for a map unit id, for the event TeleportOut and TeleportIn
 * instructions. Returns 1, or 0 when the id has no unit. */
s32 effect_secondary_map_unit_start_teleport(u32 map_unit_id) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        effect_secondary_start_teleport(unit);
        return 1;
    }
    return 0;
}

/* Release step 8 of map_update_deployment_init_sequence to build the deployed units
 * and queue their graphics. The deployment screen sets this after choosing coordinates,
 * before its title and fade finish; deploy_skip_thread sets it when skipped. */
void deploy_mark_finished(void) {
    g_deploy_finished = 1;
}

/* Returns the hsync count battle_submit_frame measured for the last frame. No
 * code calls it. */
s32 system_get_frame_measurement(void) {
    return g_system_battle_frame_hsync_count;
}

/* Set g_system_min_vsync_interval, the minimum number of vertical blanks
 * battle_submit_frame waits per frame in the effect and action-execute
 * states. Values outside 1-9 are ignored. */
void system_set_min_vsync_interval(s32 value) {
    if (value >= 1 && value <= 9) {
        g_system_min_vsync_interval = value;
    }
}

/* Remove the minimum frame wait system_set_min_vsync_interval set. */
void system_clear_min_vsync_interval(void) {
    g_system_min_vsync_interval = 0;
}

/* Apply a GNS camera command: aim the camera at the negated integer position (in 20.12) with the
 * rotation masked to 13 bits, put the cursor on the tile below at level 0, and set the camera
 * height from that tile. */
void camera_set_position_and_cursor(const SVECTOR* coords, const SVECTOR* rotation) {
    s32 tile_x;
    s32 tile_y;

    vector_set_long(&g_camera_real_coords, -coords->vx * ONE, -coords->vy * ONE, -coords->vz * ONE);
    vector_set_short(&g_camera_render_state, rotation->vx & 0x1fff, rotation->vy & 0x1fff, rotation->vz & 0x1fff);
    g_cursor_layer = 0;
    tile_x = g_camera_real_coords.vx / (MAP_TILE_SIZE * ONE);
    g_cursor_x = tile_x;
    tile_y = g_camera_real_coords.vz / (MAP_TILE_SIZE * ONE);
    g_cursor_y = tile_y;
    g_camera_real_coords.vy = -map_get_tile(tile_x, tile_y, 0)->height * (MAP_HEIGHT_STEP * ONE);
}
