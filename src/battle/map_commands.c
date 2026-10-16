/* Battle state timing, tile lookups, map data load polling, map command dispatch, ENTD unit loading, the map init
 * sequence and screen color modulation. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_effect_projectile.h"
#include "fft/battle_effect_script.h"
#include "fft/battle_effect_secondary.h"
#include "fft/battle_gfx.h"
#include "fft/battle_gfx_resource.h"
#include "fft/battle_map.h"
#include "fft/battle_map_light.h"
#include "fft/battle_map_mesh.h"
#include "fft/battle_map_palette.h"
#include "fft/battle_map_resource.h"
#include "fft/battle_map_weather.h"
#include "fft/battle_menu.h"
#include "fft/battle_sound.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/battle_unit_encounter.h"
#include "fft/event_attack.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_gfx.h"
#include "fft/main_save.h"
#include "fft/main_sound.h"
#include "fft/main_unit.h"
#include "fft/menu.h"
#include "fft/menu_overlay.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Return g_battle_state_animation_continue_check, nonzero while the action's effect is still
 * loading or playing; script_play_effect_and_wait polls it on the main stack. */
s32 battle_state_get_animation_continue_check(void) {
    return g_battle_state_animation_continue_check;
}

/* Enter the effect-playback game state, saving the state and animation speed
 * that battle_state_handle_effect_state restores. Returns 1 when the effect data is
 * not ready yet and the caller must retry. No code calls it. */
s32 battle_state_enter_effect_playback(void) {
    s32 previous_state;
    s32 previous_animation_speed;

    /* The target loads no argument; the parameter is the $a0 value unhandled states return. */
    if (battle_effect_update_file_load() != 0) {
        return 1;
    }
    battle_effect_play();
    previous_state = g_battle_game_state;
    previous_animation_speed = g_main_system_vsync_interval;
    g_battle_game_state = BATTLE_GAME_STATE_EFFECT;
    g_main_system_vsync_interval = 2;
    g_battle_gfx_screen_color_modulation_fade_frames = 0;
    g_battle_camera_screen_offset_countdown = 0;
    g_battle_camera_rotation_countdown = 0;
    g_battle_camera_zoom_countdown = 0;
    g_battle_previous_game_state = previous_state;
    g_main_saved_animation_speed = previous_animation_speed;
    return 0;
}

/* Empty, and nothing in the game calls it. */
void battle_noop_8008de88(void) {
}

/* Stop the slow-down timer the game loop starts when a frame of the effect or
 * action-execute state takes more than 0x200 hsyncs. */
void battle_state_reset_frame_pacing_timer(void) {
    g_main_frame_pacing_timer = 0;
}

/* Count one more entry into the status screen (the DEBUGCHR unit editor). While the count is
 * exactly 1 the battle state handlers stop animating the units and the camera;
 * battle_menu_leave_status_screen_selection resets it. */
void battle_menu_enter_status_screen_selection(void) {
    if (g_battle_menu_status_screen_selected != 0) {
        g_battle_menu_status_screen_selected++;
    } else {
        g_battle_menu_status_screen_selected = 1;
    }
}

/* Reset the status-screen count to 0, whatever its depth. */
void battle_menu_leave_status_screen_selection(void) {
    g_battle_menu_status_screen_selected = 0;
}

/* Return g_main_system_vsync_interval, the vertical blanks per frame (1, or 2 during effects and
 * events); event waits divide their frame counts by it. */
s32 battle_state_get_vsync_interval(void) {
    return g_main_system_vsync_interval;
}

/* Set g_main_system_vsync_interval, the number of vertical blanks per frame, to
 * 1 or 2. Returns 0, or -1 for any other value. */
s32 battle_state_set_animation_speed(s32 speed) {
    if (speed == 1 || speed == 2) {
        g_main_system_vsync_interval = speed;
        return 0;
    }
    return -1;
}

/* Write the cursor tile to `destination` as (x, map level, y). */
void battle_map_store_selected_tile_coordinates(SVECTOR* destination) {
    main_util_set_svector(destination, g_battle_cursor_x, g_battle_cursor_z, g_battle_cursor_y);
}

/* Write the map's width and depth in tiles to `destination` as (x, 0, z). */
void battle_map_store_max_coordinates(SVECTOR* destination) {
    main_util_set_svector(destination, g_battle_map_max_x, 0, g_battle_map_max_y);
}

/* The tile a unit stands on, by runtime id. The runtime record lookup is not checked:
 * an id without one is read through address 0. */
map_tile_t* battle_map_get_unit_tile_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    return battle_map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z);
}

/* The tile a unit stands on, by battle id. The runtime record lookup is not checked:
 * an id without one is read through address 0. */
map_tile_t* battle_map_get_tile_data_ptr_from_battle_id(u32 battle_id) {
    battle_unit_t* unit = battle_unit_get_by_battle_id(battle_id & 0xffff);

    return battle_map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z);
}

/* The tile under a unit's integer world position (28 units per tile) on its map layer. */
map_tile_t* battle_map_get_unit_tile_from_world_coords(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    return battle_map_get_tile_data_pointer(
        unit->world_coords.vx / MAP_TILE_SIZE, unit->world_coords.vz / MAP_TILE_SIZE, unit->map_z);
}

/* Ask the map loader to load what the current map state needs. 1 when the load
 * started, 0 when a file read is still in progress. */
s32 battle_map_start_data_load(void) {
    return battle_map_load_data(g_battle_map_id, MAP_LOAD_COMMAND_START) != 0;
}

/* Advance the current map's load by one step. Nonzero while it is still loading. */
s32 battle_map_poll_data_load(void) {
    return battle_map_load_data(g_battle_map_id, MAP_LOAD_COMMAND_POLL);
}

/* Event Use3DObject: start state row value_b of map mesh part value_a (map data
 * command 0x80, with flag 1). Always returns 1. */
s32 battle_map_command_set_3d_object_state(s32 value_a, s32 value_b) {
    battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, value_a, value_b, 1);
    return 1;
}

/* Return the map mesh part's animation state; zero means its animation has ended. */
s32 battle_map_command_get_3d_object_state(s32 value) {
    return battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_GET_3D_OBJECT_STATE, value, 1, 1);
}

/* Start map texture animation value_a from its first frame (map data command
 * 0x83). value_b goes in the object state argument, which that command does not
 * read. Always returns 1. */
s32 battle_map_command_start_texture_animation(s32 value_a, s32 value_b) {
    battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_START_TEXTURE_ANIMATION, value_a, value_b, 1);
    return 1;
}

/* Return whether the map texture animation is still playing. */
s32 battle_map_command_get_texture_animation_active(s32 value) {
    return battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_GET_TEXTURE_ANIMATION_ACTIVE, value, 1, 1);
}

/* Nonzero while the camera moves: a pan step, a rotation, a tilt or a zoom is in progress. Event
 * code waits for it to return 0. */
s32 battle_camera_is_active(void) {
    if (g_battle_camera_step_vector.vx == 0 && g_battle_camera_step_vector.vy == 0
        && g_battle_camera_step_vector.vz == 0 && g_battle_camera_rotation_action == CAMERA_ROTATION_NONE
        && g_battle_camera_tilt_action == CAMERA_STEP_NONE && g_battle_camera_zoom_action == CAMERA_STEP_NONE) {
        return 0;
    }
    return 1;
}

/* Move the cursor to the unit with this battle id. Returns 1 when the unit has
 * runtime data, otherwise 0 with the cursor unchanged. */
s32 battle_target_move_cursor_to_battle_id(u32 battle_id) {
    battle_unit_t* unit = battle_unit_get_by_battle_id(battle_id & 0xFFFF);

    if (unit != 0) {
        battle_target_move_cursor_to_unit(unit);
        return 1;
    }
    return 0;
}

/* Aim a projectile from one unit at another: set up the direct trajectory to the
 * target's tile and make the target the attacker's only target. Returns 1 when
 * both units exist. Nothing in the game calls it. */
s32 battle_effect_init_single_target_trajectory(u32 attacker_id, u32 target_id) {
    battle_unit_t* attacker = battle_unit_get_by_battle_id(attacker_id);
    battle_unit_t* target = battle_unit_get_by_battle_id(target_id);
    SVECTOR target_position;

    if (target != 0) {
        main_util_set_svector(&target_position, target->map_x, target->map_y, target->map_z);
        battle_effect_check_direct_trajectory_to_target(attacker_id, &target_position, target_id);
        if (attacker != 0) {
            attacker->strike.target_count = 1;
            attacker->strike.target_list[0] = target_id;
            return 1;
        }
    }
    return 0;
}

/* battle_effect_set_secondary_teleport for a runtime id, for the event TeleportOut and TeleportIn
 * instructions. Returns 1, or 0 when the id has no unit. */
s32 battle_effect_set_secondary_teleport_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        battle_effect_set_secondary_teleport(unit);
        return 1;
    }
    return 0;
}

/* Release step 8 of battle_map_init_units_sprites_event_and_music, which waits for this flag
 * before setting up deployment graphics. The deployment screen sets it after placing units, and
 * map_data_load_complete_thread when deployment is skipped. */
void battle_deployment_mark_finished(void) {
    g_battle_map_data_load_complete = 1;
}

/* Returns the hsync count battle_state_sync_frame measured for the last frame. No
 * code calls it. */
s32 battle_state_get_frame_measurement(void) {
    return g_main_system_battle_frame_hsync_count;
}

/* Set g_main_system_min_vsync_interval, the minimum number of vertical blanks
 * battle_state_sync_frame waits per frame in the effect and action-execute
 * states. Values outside 1-9 are ignored. */
void battle_state_set_min_vsync_interval(s32 value) {
    if (value >= 1 && value <= 9) {
        g_main_system_min_vsync_interval = value;
    }
}

/* Remove the minimum frame wait battle_state_set_min_vsync_interval set. */
void battle_state_clear_frame_pacing(void) {
    g_main_system_min_vsync_interval = 0;
}

/* Apply a GNS camera command: aim the camera at the negated integer position (in 20.12) with the
 * rotation masked to 13 bits, put the cursor on the tile below at level 0, and set the camera
 * height from that tile. */
void battle_camera_set_focus_and_cursor_tile(const SVECTOR* coords, const SVECTOR* rotation) {
    s32 tile_x;
    s32 tile_y;

    main_util_set_vector(&g_battle_camera_current_real_coords, -coords->vx * ONE, -coords->vy * ONE, -coords->vz * ONE);
    main_util_set_svector(
        &g_battle_camera_render_state, rotation->vx & 0x1fff, rotation->vy & 0x1fff, rotation->vz & 0x1fff);
    g_battle_cursor_z = 0;
    tile_x = g_battle_camera_current_real_coords.vx / (MAP_TILE_SIZE * ONE);
    g_battle_cursor_x = tile_x;
    tile_y = g_battle_camera_current_real_coords.vz / (MAP_TILE_SIZE * ONE);
    g_battle_cursor_y = tile_y;
    g_battle_camera_current_real_coords.vy
        = -battle_map_get_tile_data_pointer(tile_x, tile_y, 0)->height * (MAP_HEIGHT_STEP * ONE);
}

/* Resets the camera for a new map: screen offset (0x100, 0xa0, 0x280), zoom ONE,
 * pitch 0x12e (the low tilt), yaw 0xe00, position at the origin, cursor on tile
 * (0, 0), no acting or casting unit, no rotation, tilt or zoom in progress and
 * directional panning off. */
static void battle_camera_init_defaults(void) {
    main_util_set_vector(&g_battle_camera_current_screen_offset, 0x100, 0xA0, 0x280);
    main_util_set_vector(&g_battle_camera_zoom, ONE, ONE, ONE);
    main_util_set_svector(&g_battle_camera_render_state, 0x12E, 0xE00, 0);
    main_util_set_vector(&g_battle_camera_current_real_coords, 0, 0, 0);

    g_battle_casting_runtime_id = 0xFF;
    g_battle_unit_source_runtime_id = 0xFF;
    g_battle_cursor_y = 0;
    g_battle_cursor_z = 0;
    g_battle_cursor_x = 0;
    g_battle_camera_rotation_action = CAMERA_ROTATION_NONE;
    g_battle_camera_zoom_action = CAMERA_STEP_NONE;
    g_battle_map_zoom_target = MAP_ZOOM_IN;
    g_battle_camera_tilt_action = CAMERA_STEP_NONE;
    g_battle_map_tilt_target = MAP_TILT_LOW;
    g_battle_camera_map_pan_enabled = 0;
}

/* Appends one unit to the graphics load queue that
 * battle_gfx_step_queued_unit_graphics_load works through.
 *
 * A spritesheet id of BATTLE_SPRITESHEET_ID_COUNT or more is replaced by
 * Ramza's chapter 1 sheet. The count stops at 16, the size of the
 * queue, so a seventeenth call writes the record one past its end. */
void battle_gfx_queue_unit_graphics_load(s32 map_x, s32 map_y, s32 map_level, s16 facing, s16 spritesheet_id,
    s16 palette_id, s16 runtime_id, unit_stats_t* unit, u32 flags) {
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].map_x = map_x;
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].map_y = map_y;
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].map_level = map_level;
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].facing = facing;
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].spritesheet_id
        = spritesheet_id;
    if (spritesheet_id >= BATTLE_SPRITESHEET_ID_COUNT) {
        g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].spritesheet_id
            = BATTLE_SPRITESHEET_ID_RAMZA_CHAPTER_1;
    }
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].palette_id = palette_id;
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].runtime_id = runtime_id;
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].stats = unit;
    g_main_gfx_unit_graphics_load_queue.descriptors[g_main_gfx_unit_graphics_load_queue.count].flags = flags;
    if (g_main_gfx_unit_graphics_load_queue.count < BATTLE_UNIT_RUNTIME_SLOT_COUNT) {
        g_main_gfx_unit_graphics_load_queue.count++;
    }
}

/* Always 0. No code calls it. */
s32 battle_return_zero_8008e6d0(void) {
    return 0;
}

/* A volatile view of battle_unit_encounter.h's global: with the plain
 * declaration battle_unit_select_and_open_entd compiles differently. */
extern volatile s32 g_main_entd_selection_mode;

/* Chooses the encounter's ENTD id and starts reading its ENTD file.
 *
 * The id is script variable EVENT_SCRIPT_VAR_CURRENT_ENTD unless
 * g_main_entd_selection_mode overrides it: 250, a random id 75..79, a random id
 * 1..59, or the map id when that is not 0. Returns main_entd_open_file's result. */
static s32 battle_unit_select_and_open_entd(void) {
    volatile s32* current_event_id;
    s32 event_id;
    s32 selection_mode;

    event_id = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_ENTD);
    selection_mode = g_main_entd_selection_mode;
    current_event_id = &g_main_entd_current_event_id;
    *current_event_id = event_id;

    if (selection_mode == BATTLE_ENTD_SELECTION_ENTD_250) {
        *current_event_id = 250;
    } else if (selection_mode == BATTLE_ENTD_SELECTION_RANDOM_75_TO_79) {
        *current_event_id = (rand() * 5) / 0x8000 + 75;
    } else if (selection_mode == BATTLE_ENTD_SELECTION_RANDOM_1_TO_59) {
        *current_event_id = (rand() * 59) / 0x8000 + 1;
    } else if ((selection_mode == BATTLE_ENTD_SELECTION_MAP_ID) && (g_battle_map_id != 0)) {
        *current_event_id = g_battle_map_id;
    }

    return main_entd_open_file();
}

/* Loads the encounter's ENTD units once the ENTD file has arrived, then frees the
 * file. Returns 1 while the file is still loading, else 0 (also when there is no
 * current encounter). */
static s32 battle_unit_poll_and_load_entd_units(void) {
    entd_encounter_t* entd_data = main_entd_get_encounter();

    if (entd_data != (entd_encounter_t*)-1) {
        if (entd_data == 0) {
            return 1;
        }
        battle_unit_load_entd_units(entd_data, 0);
        main_heap_call_free(g_main_entd_set);
    }
    return 0;
}

enum {
    BATTLE_WEATHER_CALENDAR_PERIOD_COUNT = 4,
    BATTLE_WEATHER_RANDOM_VARIANT_COUNT = 10,
};

/* Randomizes the battle's weather and time of day for the current date.
 *
 * The calendar cutoffs divide the year into four weather periods (the last
 * cutoff wraps back to the first period's row). Each row of signed
 * adjustments is indexed by a uniform `rand()` bucket and applied to the
 * current weather strength before it is clamped to none..strong storm. One
 * battle in three takes place at night.
 */
static void battle_weather_randomize_from_calendar(void) {
    u8 period_end_month[BATTLE_WEATHER_CALENDAR_PERIOD_COUNT] = { 2, 5, 7, 11 };
    u8 period_end_day[BATTLE_WEATHER_CALENDAR_PERIOD_COUNT] = { 19, 21, 23, 23 };
    u8 weather_period[BATTLE_WEATHER_CALENDAR_PERIOD_COUNT + 1] = { 3, 0, 2, 1, 3 };
    s8 weather_adjustments[BATTLE_WEATHER_CALENDAR_PERIOD_COUNT][BATTLE_WEATHER_RANDOM_VARIANT_COUNT] = {
        { -1, -1, -1, -1, -1, -1, 0, 0, 0, 1 },
        { -1, -1, -1, -1, -1, -1, -1, -1, 0, 1 },
        { -1, 0, 0, 0, 1, 1, 1, 1, 1, 1 },
        { -1, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
    };
    s32 month;
    s32 day;
    s32 weather;
    s32 period;
    s32 random_bucket;
    u8 selected_weather_period;

    month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    day = script_get_variable(EVENT_SCRIPT_VAR_DAY);
    weather = script_get_variable(EVENT_SCRIPT_VAR_WEATHER);

    period = 0;
    while (period < BATTLE_WEATHER_CALENDAR_PERIOD_COUNT) {
        if (month < period_end_month[period]) {
            break;
        }
        if (month == period_end_month[period] && day < period_end_day[period]) {
            break;
        }
        period++;
    }

    selected_weather_period = weather_period[period];
    random_bucket = (rand() * BATTLE_WEATHER_RANDOM_VARIANT_COUNT) / 0x8000;
    weather += weather_adjustments[selected_weather_period][random_bucket];
    if (weather < BATTLE_WEATHER_NONE) {
        weather = BATTLE_WEATHER_NONE;
    } else if (weather >= BATTLE_WEATHER_SNOW) {
        weather = BATTLE_WEATHER_STRONG_STORM;
    }
    script_set_variable(EVENT_SCRIPT_VAR_WEATHER, weather);

    if ((rand() * 3) / 0x8000 != 0) {
        script_set_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY, 0);
    } else {
        script_set_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY, 1);
    }
}

/* Runs one step of the map initialization the MapInitialize game states run
 * behind a black screen, and returns the next step (13 when done); steps that
 * poll a loader stay on the same step until it finishes. In order: reset the
 * camera and palettes and open SMALL, load the map, open and load the ENTD
 * units, load their graphics, build the deployed units, pick random battle
 * music for a random battle (world result 1) or a debug battle, then start the
 * weather sound and seed the color fades. `unused_18` reproduces the target's
 * unreferenced frame slot at sp+0x18.
 *
 * Jump table: .rodata 0x80068260, 13 32-bit addresses. */
s32 battle_map_step_init_sequence(s32 map_id, s32 step) {
    s32 result;
    u8 unused_18[4];

    switch (step) {
    case 0:
        /* No sound id is loaded: the callee receives map_id, still in $a0. */
        main_sound_stop_sfx(map_id);
        step++;
        battle_camera_init_defaults();
        battle_map_init_palette_state();
        battle_map_load_palette_data(&g_battle_map_weather_particle_palette, MAP_CLUT_ROW_SPRITES, 0, 0);
        g_main_gfx_unit_graphics_load_queue.count = 0;
        menu_request_open_small();
        break;
    case 1:
        if (menu_poll_small_load() == 0) {
            step++;
        }
        break;
    case 2:
        if (battle_map_load_data(map_id, MAP_LOAD_COMMAND_START) != 0) {
            step++;
        }
        break;
    case 3:
        if (battle_map_load_data(map_id, MAP_LOAD_COMMAND_POLL) == 0) {
            step++;
        }
        break;
    case 4:
        if (battle_unit_select_and_open_entd() != 0) {
            step++;
        }
        break;
    case 5:
        if (battle_unit_poll_and_load_entd_units() == 0) {
            step++;
        }
        break;
    case 6:
        battle_gfx_clear_unit_graphics_load_state(map_id);
        step++;
        break;
    case 7:
        do {
            result = battle_gfx_step_queued_unit_graphics_load();
        } while (result == 2);
        if (result == 0) {
            step++;
        }
        break;
    case 8:
        battle_unit_build_deployed_units_data(g_main_deployed_unit_map_coordinates, 0);
        step++;
        break;
    case 9:
    case 10:
        step++;
        break;
    case 11:
        if (g_main_system_frontend_world_result == 1
            || script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
            main_sound_unload_current_scenario_music();
            /* Called with no argument: the sound id is whatever
             * main_sound_unload_current_scenario_music left in $a0. */
            ((void (*)(void))main_sound_stop_sfx)();
            main_sound_set_current_music_track(g_battle_random_battle_music_tracks[rand() / 0x2000]);
        }
        step++;
        break;
    case 12:
        battle_sound_start_weather_sfx();
        /* Seed the three color fades from the colors the loaded map set. */
        battle_map_init_background_gradient();
        battle_map_init_ambient_light();
        battle_map_init_darkness();
        battle_map_upload_clut_rows();
        battle_action_run_between_turn_control(BATTLE_BETWEEN_TURN_MODE_RESET);
        battle_ai_init_workspace();
        step++;
        break;
    }
    return step;
}

/* Runs one step of the battle-entry map initialization, which the deployment
 * loop drives, and returns the next step (13 when done).
 *
 * Unlike battle_map_step_init_sequence it randomizes a random battle's weather,
 * loads the shared SHP and SEQ data once (skipped when already loaded), waits
 * at step 8 until the deployment screen or map_data_load_complete_thread
 * releases it (battle_deployment_mark_finished), and at step 10 waits for
 * deployment to end before restoring the scenario music and darkness. Loader
 * steps retain their current state while work is pending. */
s32 battle_map_init_units_sprites_event_and_music(s32 map_id, s32 step, s32 deployment_running) {
    s32 result;
    u8 unused_18[4];

    switch (step) {
    case 0:
        /* Called with no argument: the callee receives map_id, still in $a0
         * (passing it explicitly changes the register allocation). */
        ((void (*)(void))main_sound_stop_sfx)();
        battle_camera_init_defaults();
        battle_map_init_palette_state();
        if (g_main_system_frontend_world_result == 1) {
            battle_weather_randomize_from_calendar();
        }
        step++;
        battle_map_load_palette_data(&g_battle_map_weather_particle_palette, MAP_CLUT_ROW_SPRITES, 0, 0);
        g_main_gfx_unit_graphics_load_queue.count = 0;
        break;
    case 1:
        step++;
        break;
    case 2:
        if (battle_map_load_data(map_id, MAP_LOAD_COMMAND_START) != 0) {
            step++;
        }
        break;
    case 3:
        if (battle_map_load_data(map_id, MAP_LOAD_COMMAND_POLL) == 0) {
            step++;
        }
        break;
    case 4:
        if (battle_unit_select_and_open_entd() != 0) {
            step++;
        }
        break;
    case 5:
        if (battle_unit_poll_and_load_entd_units() != 0) {
            break;
        }
        if (g_battle_unit_graphics_load_complete != 0) {
            battle_gfx_clear_unit_graphics_load_state(map_id);
            step = 7;
        } else {
            step++;
            battle_gfx_reset_unit_graphics_load_state();
        }
        break;
    case 6:
        if (battle_gfx_step_load_unit_shp_seq_data() == 8) {
            g_battle_unit_graphics_load_complete = 1;
            battle_gfx_clear_unit_graphics_load_state(map_id);
            step++;
        }
        break;
    case 7:
        if (script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
            step++;
            break;
        }
        do {
            result = battle_gfx_step_queued_unit_graphics_load();
        } while (result == 2);
        if (result == 0) {
            step++;
        }
        break;
    case 8:
        if (g_battle_map_data_load_complete != 0) {
            step++;
            battle_gfx_init_deployment_and_reset_unit_graphics_state();
            g_battle_deployment_skipped = 0;
        }
        break;
    case 9:
        do {
            result = battle_gfx_step_queued_unit_graphics_load();
        } while (result == 2);
        if (result == 0) {
            step++;
        }
        break;
    case 10:
        if (deployment_running == 0) {
            step++;
            attack_scenario_restore_music_and_darkness();
        }
        break;
    case 11:
        if (g_main_system_frontend_world_result == 1
            || script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
            main_sound_unload_current_scenario_music();
            /* Called with no argument: the sound id is whatever
             * main_sound_unload_current_scenario_music left in $a0. */
            ((void (*)(void))main_sound_stop_sfx)();
            main_sound_set_current_music_track(g_battle_random_battle_music_tracks[rand() / 0x2000]);
        }
        step++;
        break;
    case 12:
        battle_sound_start_weather_sfx();
        /* Seed the three color fades from the colors the loaded map set. */
        battle_map_init_background_gradient();
        battle_map_init_ambient_light();
        battle_map_init_darkness();
        battle_map_upload_clut_rows();
        battle_action_run_between_turn_control(BATTLE_BETWEEN_TURN_MODE_RESET);
        battle_ai_init_workspace();
        step++;
        break;
    }
    return step;
}

/* No retail code calls it. */
void battle_gfx_clear_screen_color_modulation(void) {
    g_battle_gfx_screen_color_modulation.b = 0;
    g_battle_gfx_screen_color_modulation.g = 0;
    g_battle_gfx_screen_color_modulation.r = 0;
}

/* Saves the screen color modulation for
 * battle_gfx_restore_screen_color_modulation. No retail code calls either. */
void battle_gfx_copy_screen_color_modulation(void) {
    g_battle_gfx_screen_color_modulation_backup.r = g_battle_gfx_screen_color_modulation.r;
    g_battle_gfx_screen_color_modulation_backup.g = g_battle_gfx_screen_color_modulation.g;
    g_battle_gfx_screen_color_modulation_backup.b = g_battle_gfx_screen_color_modulation.b;
}

/* No retail code calls it. */
void battle_gfx_restore_screen_color_modulation(void) {
    g_battle_gfx_screen_color_modulation.r = g_battle_gfx_screen_color_modulation_backup.r;
    g_battle_gfx_screen_color_modulation.g = g_battle_gfx_screen_color_modulation_backup.g;
    g_battle_gfx_screen_color_modulation.b = g_battle_gfx_screen_color_modulation_backup.b;
}

/*
 * Start a screen color-modulation fade towards (red, green, blue).
 *
 * The duration is scaled by the animation speed; a fade that rounds to zero
 * frames only records the target color, and a zero duration does nothing. The
 * overlay draw mode is rebuilt with the given semi-transparency mode for both
 * screen polarities.
 */
void battle_gfx_start_screen_color_modulation_fade(u16 blend_mode, u16 red, u16 green, u16 blue, u16 duration) {
    s16 frames;

    if (duration != 0) {
        frames = duration / g_main_system_vsync_interval;
        g_battle_gfx_screen_color_modulation_target.r = red;
        g_battle_gfx_screen_color_modulation_target.g = green;
        g_battle_gfx_screen_color_modulation_target.b = blue;
        g_battle_gfx_screen_color_modulation_fade_frames = frames;
        if (frames != 0) {
            g_battle_gfx_screen_color_modulation_value[0] = g_battle_gfx_screen_color_modulation.r << 8;
            g_battle_gfx_screen_color_modulation_value[1] = g_battle_gfx_screen_color_modulation.g << 8;
            g_battle_gfx_screen_color_modulation_value[2] = g_battle_gfx_screen_color_modulation.b << 8;
            g_battle_gfx_screen_color_modulation_step[0]
                = ((g_battle_gfx_screen_color_modulation_target.r - g_battle_gfx_screen_color_modulation.r) << 8)
                / frames;
            g_battle_gfx_screen_color_modulation_step[1]
                = ((g_battle_gfx_screen_color_modulation_target.g - g_battle_gfx_screen_color_modulation.g) << 8)
                / frames;
            g_battle_gfx_screen_color_modulation_step[2]
                = ((g_battle_gfx_screen_color_modulation_target.b - g_battle_gfx_screen_color_modulation.b) << 8)
                / frames;
            SetDrawMode(&g_battle_gfx_screen_modulation_draw_modes[0], 0, 0, blend_mode << 5,
                &g_battle_gfx_screen_modulation_tex_window);
            g_battle_gfx_screen_modulation_draw_modes[1] = g_battle_gfx_screen_modulation_draw_modes[0];
        }
    }
}

/* Advance the screen color-modulation fade by one frame: each channel's Q8
 * accumulator takes its step and the color in effect its integer part, and the
 * last frame lands exactly on the target color.
 *
 * The fade state (0x800961c4..0x800961e1) holds the target color, the color in
 * effect, the Q8 accumulator and per-frame Q8 step per channel, and the frames
 * left. The three-element array shape is what makes GCC hold the base
 * addresses of the current-color and accumulator arrays in registers for
 * element 0. */
void battle_gfx_update_screen_color_modulation_fade(void) {
    u16 count = g_battle_gfx_screen_color_modulation_fade_frames;
    u16 remaining;
    u16 red;
    u16 green;
    u16 blue;

    if (count != 0) {
        remaining = count - 1;
        g_battle_gfx_screen_color_modulation_fade_frames = remaining;
        red = g_battle_gfx_screen_color_modulation_value[0] + g_battle_gfx_screen_color_modulation_step[0];
        g_battle_gfx_screen_color_modulation_value[0] = red;
        green = g_battle_gfx_screen_color_modulation_value[1] + g_battle_gfx_screen_color_modulation_step[1];
        g_battle_gfx_screen_color_modulation_value[1] = green;
        blue = g_battle_gfx_screen_color_modulation_value[2] + g_battle_gfx_screen_color_modulation_step[2];
        g_battle_gfx_screen_color_modulation_value[2] = blue;
        g_battle_gfx_screen_color_modulation.r = red >> 8;
        g_battle_gfx_screen_color_modulation.g = green >> 8;
        g_battle_gfx_screen_color_modulation.b = blue >> 8;
        if (remaining == 0) {
            g_battle_gfx_screen_color_modulation.r = g_battle_gfx_screen_color_modulation_target.r;
            g_battle_gfx_screen_color_modulation.g = g_battle_gfx_screen_color_modulation_target.g;
            g_battle_gfx_screen_color_modulation.b = g_battle_gfx_screen_color_modulation_target.b;
        }
    }
}

/* Queues the screen color-modulation overlay for this frame, the polygon and its
 * draw mode of the current screen polarity, unless all three components are 0.
 * `unused` reproduces the frame size. */
void battle_gfx_draw_screen_color_modulation_overlay(void) {
    char unused[24];

    if ((g_battle_gfx_screen_color_modulation.r | g_battle_gfx_screen_color_modulation.g
            | g_battle_gfx_screen_color_modulation.b)
        != 0) {
        g_battle_gfx_screen_modulation_polygons[g_main_gfx_screen_polarity].r0 = g_battle_gfx_screen_color_modulation.r;
        g_battle_gfx_screen_modulation_polygons[g_main_gfx_screen_polarity].g0 = g_battle_gfx_screen_color_modulation.g;
        g_battle_gfx_screen_modulation_polygons[g_main_gfx_screen_polarity].b0 = g_battle_gfx_screen_color_modulation.b;
        AddPrim(main_gfx_get_otag(), &g_battle_gfx_screen_modulation_polygons[g_main_gfx_screen_polarity]);
        AddPrim(main_gfx_get_otag(), &g_battle_gfx_screen_modulation_draw_modes[g_main_gfx_screen_polarity]);
    }
}
