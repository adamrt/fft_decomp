/* Expose scene operations to event scripts and effect callbacks, including effect requests, camera, map, deployment and
 * display controls. */

#include "fft/battle_map.h"
#include "fft/battle_map_load.h"
#include "fft/battle_state.h"
#include "fft/camera.h"
#include "fft/command.h"
#include "fft/cursor.h"
#include "fft/deploy.h"
#include "fft/effect.h"
#include "fft/effect_script.h"
#include "fft/effect_secondary.h"
#include "fft/projectile.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "fft/vector.h"
#include "psx/libgte.h"
#include "psx/types.h"

extern u16 g_fade_tint_fade_frames;

/* Start an ability's effect cast by runtime unit 0 on runtime unit 1 with a normal hit. No retail
 * code calls it. */
void effect_basic_ability_start(s32 ability_id) {
    effect_context_t effect_data;

    effect_data.target_count = 1;
    effect_data.targets[EFFECT_CASTER_ENTRY].target_type = EFFECT_TARGET_TYPE_UNIT;
    effect_data.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id = 0;
    effect_data.targets[0].target_type = EFFECT_TARGET_TYPE_UNIT;
    effect_data.targets[0].data.unit.runtime_id = 1;
    effect_data.targets[0].data.unit.result_animation = EFFECT_HIT_NORMAL;
    effect_data.spectator_count = 0;
    effect_ability_animation_set(0, ability_id, &effect_data);
}

/* Starts an ability's effect outside an action, on a unit, on a tile or
 * on a unit that is also its caster. */
void effect_request_start(s32 ability_id, s32 target_mode, effect_start_target_data_t target) {
    effect_context_t effect_data;

    effect_data.target_count = 1;
    effect_data.targets[EFFECT_CASTER_ENTRY].target_type = EFFECT_TARGET_TYPE_UNIT;
    effect_data.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id = 0;

    if (target_mode == EFFECT_START_ON_UNIT) {
        effect_data.targets[0].target_type = EFFECT_TARGET_TYPE_UNIT;
        effect_data.targets[0].data.unit.runtime_id = target.runtime_id;
        effect_data.targets[0].data.unit.result_animation = EFFECT_HIT_NORMAL;
    } else if (target_mode == EFFECT_START_ON_TILE) {
        effect_data.targets[0].target_type = EFFECT_TARGET_TYPE_TILE;
        effect_data.targets[0].data.tile.vx = target.tile->vx;
        effect_data.targets[0].data.tile.vz = target.tile->vz;
        effect_data.targets[0].data.tile.vy = target.tile->vy;
    } else {
        effect_data.targets[0].target_type = EFFECT_TARGET_TYPE_UNIT;
        effect_data.targets[0].data.unit.runtime_id = target.runtime_id;
        effect_data.targets[0].data.unit.result_animation = EFFECT_HIT_NORMAL;
        effect_data.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id = target.runtime_id;
    }

    effect_data.spectator_count = 0;
    effect_ability_animation_set(0, ability_id, &effect_data);
}

/* effect_file_load_update as a 0 or 1 result: the event script's EffectStart
 * instruction calls it on the main stack each frame until it returns 0. */
s32 effect_request_load_poll(void) {
    /* No argument is passed: for a load state outside 0..3, effect_file_load_update
     * returns whatever this caller left in $a0. */
    return effect_file_load_update() != 0;
}

/* effect_playback_start as a g_thread_call_target: script_effect_wait runs it on the main
 * stack. */
void effect_request_play(void) {
    effect_playback_start();
}

/* Return g_battle_animation_wait_state, nonzero while the action's effect is still
 * loading or playing; script_effect_wait polls it on the main stack. */
s32 battle_animation_wait_state_get(void) {
    return g_battle_animation_wait_state;
}

/* Enter the effect-playback game state, saving the state and vsync interval
 * that battle_state_effect_update restores. Returns 1 when the effect data is
 * not ready yet and the caller must retry. No code calls it. */
s32 battle_state_try_enter_effect_playback(void) {
    s32 previous_state;
    s32 previous_vsync_interval;

    if (effect_file_load_update() != 0) {
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
void system_frame_pacing_reset(void) {
    g_system_frame_pacing_timer = 0;
}

/* Count one more entry into the Status screen (BATTLE's own or DEBUGCHR's for a mounted unit). While the count is
 * exactly 1 the battle state handlers stop animating the units and the camera;
 * menu_leave_status_screen_selection resets it. */
void menu_enter_status_screen_selection(void) {
    if (g_menu_status_screen_selected != 0) {
        g_menu_status_screen_selected++;
    } else {
        g_menu_status_screen_selected = 1;
    }
}

/* Reset the status-screen count to 0, whatever its depth. */
void menu_leave_status_screen_selection(void) {
    g_menu_status_screen_selected = 0;
}

/* Return g_system_vsync_interval, the vertical blanks per frame (1, or 2 during effects and
 * events); event waits divide their frame counts by it. */
s32 system_vsync_interval_get(void) {
    return g_system_vsync_interval;
}

/* Set g_system_vsync_interval, the number of vertical blanks per frame, to
 * 1 or 2. Returns 0, or -1 for any other value. */
s32 system_vsync_interval_set(s32 speed) {
    if (speed == 1 || speed == 2) {
        g_system_vsync_interval = speed;
        return 0;
    }
    return -1;
}

/* Write the cursor tile to `destination` as (x, map level, y). */
void battle_map_selected_tile_store(SVECTOR* destination) {
    vector_short_set(destination, g_cursor_x, g_cursor_layer, g_cursor_y);
}

/* Write the map's width and depth in tiles to `destination` as (x, 0, z). */
void battle_map_dimensions_store(SVECTOR* destination) {
    vector_short_set(destination, g_battle_map_terrain_width, 0, g_battle_map_terrain_depth);
}

/* The tile a unit stands on, by runtime id. The runtime record lookup is not checked:
 * an id without one is read through address 0. */
battle_map_tile_t* battle_map_runtime_unit_tile_get(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id & 0xffff);

    return battle_map_tile_get_pointer(unit->map_x, unit->map_y, unit->map_layer);
}

/* The tile a unit stands on, by battle id. The runtime record lookup is not checked:
 * an id without one is read through address 0. */
battle_map_tile_t* battle_map_unit_tile_data_get(u32 battle_id) {
    unit_runtime_t* unit = unit_battle_get(battle_id & 0xffff);

    return battle_map_tile_get_pointer(unit->map_x, unit->map_y, unit->map_layer);
}

/* The tile under a unit's integer world position (28 units per tile) on its map layer. */
battle_map_tile_t* battle_map_world_tile_get(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id & 0xffff);

    return battle_map_tile_get_pointer(
        unit->world_coords.vx / BATTLE_MAP_TILE_SIZE, unit->world_coords.vz / BATTLE_MAP_TILE_SIZE, unit->map_layer);
}

/* Ask the map loader to load what the current map state needs. 1 when the load
 * started, 0 when a file read is still in progress. */
s32 battle_map_load_start(void) {
    return battle_map_load_data(g_battle_map_id, BATTLE_MAP_LOAD_COMMAND_START) != 0;
}

/* Advance the current map's load by one step. Nonzero while it is still loading. */
s32 battle_map_load_poll(void) {
    return battle_map_load_data(g_battle_map_id, BATTLE_MAP_LOAD_COMMAND_POLL);
}

/* Event Use3DObject: start a state row of the map mesh part (map data command
 * 0x80, with flag 1). Always returns 1. */
s32 battle_map_object_state_set(s32 part_index, s32 state_row) {
    battle_map_data_command_dispatch(BATTLE_MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, part_index, state_row, 1);
    return 1;
}

/* Return the map mesh part's animation state; zero means its animation has ended. */
s32 battle_map_object_state_get(s32 part_index) {
    return battle_map_data_command_dispatch(BATTLE_MAP_DATA_COMMAND_GET_3D_OBJECT_STATE, part_index, 1, 1);
}

/* Start a map texture animation from its first frame (map data command 0x83).
 * Forward object_state unchanged; the dispatcher does not read it for this command.
 * Always returns 1. */
s32 battle_map_texture_animation_start(s32 animation_index, s32 object_state) {
    battle_map_data_command_dispatch(BATTLE_MAP_DATA_COMMAND_START_TEXTURE_ANIMATION, animation_index, object_state, 1);
    return 1;
}

/* Return whether the map texture animation is still playing. */
s32 battle_map_texture_animation_is_active(s32 animation_index) {
    return battle_map_data_command_dispatch(
        BATTLE_MAP_DATA_COMMAND_GET_TEXTURE_ANIMATION_ACTIVE, animation_index, 1, 1);
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

/* Move the cursor to the unit with this battle id. Returns 1 when the unit has
 * runtime data, otherwise 0 with the cursor unchanged. */
s32 cursor_unit_tile_move(u32 battle_id) {
    unit_runtime_t* unit = unit_battle_get(battle_id & 0xFFFF);

    if (unit != 0) {
        cursor_move_to_unit(unit);
        return 1;
    }
    return 0;
}

/* Aim a projectile from one unit at another: set up the direct trajectory to the
 * target's tile and make the target the attacker's only target. Returns 1 when
 * both units exist. Nothing in the game calls it. */
s32 effect_projectile_single_target_init(u32 attacker_id, u32 target_id) {
    unit_runtime_t* attacker = unit_battle_get(attacker_id);
    unit_runtime_t* target = unit_battle_get(target_id);
    SVECTOR target_position;

    if (target != 0) {
        vector_short_set(&target_position, target->map_x, target->map_y, target->map_layer);
        projectile_direct_check_to_target(attacker_id, &target_position, target_id);
        if (attacker != 0) {
            attacker->strike.target_count = 1;
            attacker->strike.target_list[0] = target_id;
            return 1;
        }
    }
    return 0;
}

/* effect_secondary_teleport_start for a runtime id, for the event TeleportOut and TeleportIn
 * instructions. Returns 1, or 0 when the id has no unit. */
s32 effect_secondary_unit_teleport_start(u32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);

    if (unit != 0) {
        effect_secondary_teleport_start(unit);
        return 1;
    }
    return 0;
}

/* Release step 8 of battle_map_deployment_init_update to build the deployed units
 * and queue their graphics. The deployment screen sets this after choosing coordinates,
 * before its title and fade finish; battle_map_skip_deployment_thread sets it when skipped. */
void deploy_mark_finished(void) {
    g_deploy_finished = 1;
}

/* Returns the hsync count battle_frame_submit measured for the last frame. No
 * code calls it. */
s32 system_frame_measurement_get(void) {
    return g_system_battle_frame_hsync_count;
}

/* Set g_system_min_vsync_interval, the minimum number of vertical blanks
 * battle_frame_submit waits per frame in the effect and action-execute
 * states. Values outside 1-9 are ignored. */
void system_min_vsync_interval_set(s32 value) {
    if (value >= 1 && value <= 9) {
        g_system_min_vsync_interval = value;
    }
}

/* Remove the minimum frame wait system_min_vsync_interval_set set. */
void system_min_vsync_interval_clear(void) {
    g_system_min_vsync_interval = 0;
}

/* Apply a GNS camera command: aim the camera at the negated integer position (in 20.12) with the
 * rotation masked to 13 bits, put the cursor on the tile below at level 0, and set the camera
 * height from that tile. */
void camera_cursor_focus_set(const SVECTOR* coords, const SVECTOR* rotation) {
    s32 tile_x;
    s32 tile_y;

    vector_long_set(&g_camera_real_coords, -coords->vx * ONE, -coords->vy * ONE, -coords->vz * ONE);
    vector_short_set(&g_camera_render_state, rotation->vx & 0x1fff, rotation->vy & 0x1fff, rotation->vz & 0x1fff);
    g_cursor_layer = 0;
    tile_x = g_camera_real_coords.vx / (BATTLE_MAP_TILE_SIZE * ONE);
    g_cursor_x = tile_x;
    tile_y = g_camera_real_coords.vz / (BATTLE_MAP_TILE_SIZE * ONE);
    g_cursor_y = tile_y;
    g_camera_real_coords.vy = -battle_map_tile_get_pointer(tile_x, tile_y, 0)->height * (BATTLE_MAP_HEIGHT_STEP * ONE);
}
