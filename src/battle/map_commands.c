/* Battle state timing, tile lookups, map data load polling, map command dispatch, ENTD unit loading, the map init
 * sequence and screen color modulation. */

#include "fft/action.h"
#include "fft/ai.h"
#include "fft/battle_map.h"
#include "fft/battle_map_light.h"
#include "fft/battle_map_load.h"
#include "fft/battle_map_palette.h"
#include "fft/camera.h"
#include "fft/command.h"
#include "fft/cursor.h"
#include "fft/deploy.h"
#include "fft/effect_script.h"
#include "fft/effect_secondary.h"
#include "fft/encounter.h"
#include "fft/fade.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/music.h"
#include "fft/overlay.h"
#include "fft/projectile.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/sprite_sheet.h"
#include "fft/state.h"
#include "fft/system.h"
#include "fft/turn.h"
#include "fft/unit.h"
#include "fft/unit_entd.h"
#include "fft/unit_runtime.h"
#include "fft/weather.h"
#include "fft/world_map.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Return g_state_animation_continue_check, nonzero while the action's effect is still
 * loading or playing; script_play_effect_and_wait polls it on the main stack. */
s32 state_get_animation_continue_check(void) {
    return g_state_animation_continue_check;
}

/* Enter the effect-playback game state, saving the state and vsync interval
 * that state_handle_effect restores. Returns 1 when the effect data is
 * not ready yet and the caller must retry. No code calls it. */
s32 state_enter_effect_playback(void) {
    s32 previous_state;
    s32 previous_vsync_interval;

    /* The target loads no argument; the parameter is the $a0 value unhandled states return. */
    if (effect_file_update_load() != 0) {
        return 1;
    }
    effect_play();
    previous_state = g_game_state;
    previous_vsync_interval = g_system_vsync_interval;
    g_game_state = GAME_STATE_EFFECT;
    g_system_vsync_interval = 2;
    g_fade_tint_fade_frames = 0;
    g_camera_screen_offset_countdown = 0;
    g_camera_rotation_countdown = 0;
    g_camera_zoom_countdown = 0;
    g_game_state_previous = previous_state;
    g_system_saved_vsync_interval = previous_vsync_interval;
    return 0;
}

/* Empty, and nothing in the game calls it. */
void state_noop_1(void) {
}

/* Stop the slow-down timer the game loop starts when a frame of the effect or
 * action-execute state takes more than 0x200 hsyncs. */
void system_reset_frame_pacing_timer(void) {
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
void battle_map_store_selected_tile_coordinates(SVECTOR* destination) {
    vector_set_svector(destination, g_cursor_x, g_cursor_z, g_cursor_y);
}

/* Write the map's width and depth in tiles to `destination` as (x, 0, z). */
void battle_map_store_max_coordinates(SVECTOR* destination) {
    vector_set_svector(destination, g_battle_map_max_x, 0, g_battle_map_max_y);
}

/* The tile a unit stands on, by runtime id. The runtime record lookup is not checked:
 * an id without one is read through address 0. */
battle_map_tile_t* battle_map_get_unit_tile_by_runtime_id(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id & 0xffff);

    return battle_map_tile_get_pointer(unit->map_x, unit->map_y, unit->map_z);
}

/* The tile a unit stands on, by battle id. The runtime record lookup is not checked:
 * an id without one is read through address 0. */
battle_map_tile_t* battle_map_get_tile_data_ptr_from_battle_id(u32 battle_id) {
    unit_t* unit = unit_get_by_battle_id(battle_id & 0xffff);

    return battle_map_tile_get_pointer(unit->map_x, unit->map_y, unit->map_z);
}

/* The tile under a unit's integer world position (28 units per tile) on its map layer. */
battle_map_tile_t* battle_map_get_unit_tile_from_world_coords(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id & 0xffff);

    return battle_map_tile_get_pointer(
        unit->world_coords.vx / BATTLE_MAP_TILE_SIZE, unit->world_coords.vz / BATTLE_MAP_TILE_SIZE, unit->map_z);
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

/* Event Use3DObject: start state row value_b of map mesh part value_a (map data
 * command 0x80, with flag 1). Always returns 1. */
s32 battle_map_set_3d_object_state(s32 value_a, s32 value_b) {
    battle_map_dispatch_data_command(BATTLE_MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, value_a, value_b, 1);
    return 1;
}

/* Return the map mesh part's animation state; zero means its animation has ended. */
s32 battle_map_get_3d_object_state(s32 value) {
    return battle_map_dispatch_data_command(BATTLE_MAP_DATA_COMMAND_GET_3D_OBJECT_STATE, value, 1, 1);
}

/* Start map texture animation value_a from its first frame (map data command
 * 0x83). value_b goes in the object state argument, which that command does not
 * read. Always returns 1. */
s32 battle_map_texture_start_animation(s32 value_a, s32 value_b) {
    battle_map_dispatch_data_command(BATTLE_MAP_DATA_COMMAND_START_TEXTURE_ANIMATION, value_a, value_b, 1);
    return 1;
}

/* Return whether the map texture animation is still playing. */
s32 battle_map_texture_get_animation_active(s32 value) {
    return battle_map_dispatch_data_command(BATTLE_MAP_DATA_COMMAND_GET_TEXTURE_ANIMATION_ACTIVE, value, 1, 1);
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
s32 cursor_move_to_battle_id(u32 battle_id) {
    unit_t* unit = unit_get_by_battle_id(battle_id & 0xFFFF);

    if (unit != 0) {
        cursor_move_to_unit(unit);
        return 1;
    }
    return 0;
}

/* Aim a projectile from one unit at another: set up the direct trajectory to the
 * target's tile and make the target the attacker's only target. Returns 1 when
 * both units exist. Nothing in the game calls it. */
s32 effect_projectile_init_single_target(u32 attacker_id, u32 target_id) {
    unit_t* attacker = unit_get_by_battle_id(attacker_id);
    unit_t* target = unit_get_by_battle_id(target_id);
    SVECTOR target_position;

    if (target != 0) {
        vector_set_svector(&target_position, target->map_x, target->map_y, target->map_z);
        projectile_direct_check_to_target(attacker_id, &target_position, target_id);
        if (attacker != 0) {
            attacker->strike.target_count = 1;
            attacker->strike.target_list[0] = target_id;
            return 1;
        }
    }
    return 0;
}

/* effect_secondary_set_teleport for a runtime id, for the event TeleportOut and TeleportIn
 * instructions. Returns 1, or 0 when the id has no unit. */
s32 effect_secondary_set_teleport_by_runtime_id(u32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        effect_secondary_set_teleport(unit);
        return 1;
    }
    return 0;
}

/* Release step 8 of battle_map_step_deployment_init_sequence, which waits for this flag
 * before setting up deployment graphics. The deployment screen sets it after placing units, and
 * battle_map_skip_deployment_thread when deployment is skipped. */
void deploy_mark_finished(void) {
    g_deploy_finished = 1;
}

/* Returns the hsync count state_sync_frame measured for the last frame. No
 * code calls it. */
s32 system_get_frame_measurement(void) {
    return g_system_battle_frame_hsync_count;
}

/* Set g_system_min_vsync_interval, the minimum number of vertical blanks
 * state_sync_frame waits per frame in the effect and action-execute
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
void camera_set_focus_and_cursor_tile(const SVECTOR* coords, const SVECTOR* rotation) {
    s32 tile_x;
    s32 tile_y;

    vector_set_vector(&g_camera_real_coords, -coords->vx * ONE, -coords->vy * ONE, -coords->vz * ONE);
    vector_set_svector(&g_camera_render_state, rotation->vx & 0x1fff, rotation->vy & 0x1fff, rotation->vz & 0x1fff);
    g_cursor_z = 0;
    tile_x = g_camera_real_coords.vx / (BATTLE_MAP_TILE_SIZE * ONE);
    g_cursor_x = tile_x;
    tile_y = g_camera_real_coords.vz / (BATTLE_MAP_TILE_SIZE * ONE);
    g_cursor_y = tile_y;
    g_camera_real_coords.vy = -battle_map_tile_get_pointer(tile_x, tile_y, 0)->height * (BATTLE_MAP_HEIGHT_STEP * ONE);
}

/* Resets the camera for a new map: screen offset (0x100, 0xa0, 0x280), zoom ONE,
 * pitch 0x12e (the low tilt), yaw 0xe00, position at the origin, cursor on tile
 * (0, 0), no acting or casting unit, no rotation, tilt or zoom in progress and
 * directional panning off. */
static void camera_init_defaults(void) {
    vector_set_vector(&g_camera_screen_offset, 0x100, 0xA0, 0x280);
    vector_set_vector(&g_camera_zoom, ONE, ONE, ONE);
    vector_set_svector(&g_camera_render_state, 0x12E, 0xE00, 0);
    vector_set_vector(&g_camera_real_coords, 0, 0, 0);

    g_unit_casting_runtime_id = 0xFF;
    g_unit_source_runtime_id = 0xFF;
    g_cursor_y = 0;
    g_cursor_z = 0;
    g_cursor_x = 0;
    g_camera_rotation_action = CAMERA_ROTATION_NONE;
    g_camera_zoom_action = CAMERA_STEP_NONE;
    g_camera_zoom_target = BATTLE_MAP_ZOOM_IN;
    g_camera_tilt_action = CAMERA_STEP_NONE;
    g_camera_tilt_target = BATTLE_MAP_TILT_LOW;
    g_camera_pan_enabled = 0;
}

/* Appends one unit to the graphics load queue that
 * sprite_load_step_queued works through.
 *
 * A spritesheet id of SPRITE_SHEET_ID_COUNT or more is replaced by
 * Ramza's chapter 1 sheet. The count stops at 16, the size of the
 * queue, so a seventeenth call writes the record one past its end. */
void sprite_queue_unit_graphics_load(s32 map_x, s32 map_y, s32 map_level, s16 facing, s16 spritesheet_id,
    s16 palette_id, s16 runtime_id, unit_stats_t* unit, u32 flags) {
    g_unit_graphics_load_queue.descriptors[g_unit_graphics_load_queue.count].map_x = map_x;
    g_unit_graphics_load_queue.descriptors[g_unit_graphics_load_queue.count].map_y = map_y;
    g_unit_graphics_load_queue.descriptors[g_unit_graphics_load_queue.count].map_level = map_level;
    g_unit_graphics_load_queue.descriptors[g_unit_graphics_load_queue.count].facing = facing;
    g_unit_graphics_load_queue.descriptors[g_unit_graphics_load_queue.count].spritesheet_id = spritesheet_id;
    if (spritesheet_id >= SPRITE_SHEET_ID_COUNT) {
        g_unit_graphics_load_queue.descriptors[g_unit_graphics_load_queue.count].spritesheet_id
            = SPRITE_SHEET_ID_RAMZA_CHAPTER_1;
    }
    g_unit_graphics_load_queue.descriptors[g_unit_graphics_load_queue.count].palette_id = palette_id;
    g_unit_graphics_load_queue.descriptors[g_unit_graphics_load_queue.count].runtime_id = runtime_id;
    g_unit_graphics_load_queue.descriptors[g_unit_graphics_load_queue.count].stats = unit;
    g_unit_graphics_load_queue.descriptors[g_unit_graphics_load_queue.count].flags = flags;
    if (g_unit_graphics_load_queue.count < UNIT_RUNTIME_SLOT_COUNT) {
        g_unit_graphics_load_queue.count++;
    }
}

/* Always 0. No code calls it. */
s32 unit_return_zero_1(void) {
    return 0;
}

/* A volatile view of encounter.h's global: with the plain
 * declaration unit_entd_select_and_open compiles differently. */
extern volatile s32 g_encounter_selection_mode;

/* Chooses the encounter's ENTD id and starts reading its ENTD file.
 *
 * The id is script variable EVENT_SCRIPT_VAR_CURRENT_ENTD unless
 * g_encounter_selection_mode overrides it: 250, a random id 75..79, a random id
 * 1..59, or the map id when that is not 0. Returns encounter_open_file's result. */
static s32 unit_entd_select_and_open(void) {
    volatile s32* current_event_id;
    s32 event_id;
    s32 selection_mode;

    event_id = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_ENTD);
    selection_mode = g_encounter_selection_mode;
    current_event_id = &g_encounter_current_event_id;
    *current_event_id = event_id;

    if (selection_mode == UNIT_ENTD_SELECTION_ENTD_250) {
        *current_event_id = 250;
    } else if (selection_mode == UNIT_ENTD_SELECTION_RANDOM_75_TO_79) {
        *current_event_id = (rand() * 5) / 0x8000 + 75;
    } else if (selection_mode == UNIT_ENTD_SELECTION_RANDOM_1_TO_59) {
        *current_event_id = (rand() * 59) / 0x8000 + 1;
    } else if ((selection_mode == UNIT_ENTD_SELECTION_MAP_ID) && (g_battle_map_id != 0)) {
        *current_event_id = g_battle_map_id;
    }

    return encounter_open_file();
}

/* Loads the encounter's ENTD units once the ENTD file has arrived, then frees the
 * file. Returns 1 while the file is still loading, else 0 (also when there is no
 * current encounter). */
static s32 unit_entd_poll_and_load_units(void) {
    entd_encounter_t* entd_data = encounter_get_current();

    if (entd_data != (entd_encounter_t*)-1) {
        if (entd_data == 0) {
            return 1;
        }
        unit_entd_load(entd_data, 0);
        heap_call_free(g_encounter_set);
    }
    return 0;
}

enum {
    WEATHER_CALENDAR_PERIOD_COUNT = 4,
    WEATHER_RANDOM_VARIANT_COUNT = 10,
};

/* Randomizes the battle's weather and time of day for the current date.
 *
 * The calendar cutoffs divide the year into four weather periods (the last
 * cutoff wraps back to the first period's row). Each row of signed
 * adjustments is indexed by a uniform `rand()` bucket and applied to the
 * current weather strength before it is clamped to none..strong storm. One
 * battle in three takes place at night.
 */
static void weather_randomize_from_calendar(void) {
    u8 period_end_month[WEATHER_CALENDAR_PERIOD_COUNT] = { 2, 5, 7, 11 };
    u8 period_end_day[WEATHER_CALENDAR_PERIOD_COUNT] = { 19, 21, 23, 23 };
    u8 weather_period[WEATHER_CALENDAR_PERIOD_COUNT + 1] = { 3, 0, 2, 1, 3 };
    s8 weather_adjustments[WEATHER_CALENDAR_PERIOD_COUNT][WEATHER_RANDOM_VARIANT_COUNT] = {
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
    while (period < WEATHER_CALENDAR_PERIOD_COUNT) {
        if (month < period_end_month[period]) {
            break;
        }
        if (month == period_end_month[period] && day < period_end_day[period]) {
            break;
        }
        period++;
    }

    selected_weather_period = weather_period[period];
    random_bucket = (rand() * WEATHER_RANDOM_VARIANT_COUNT) / 0x8000;
    weather += weather_adjustments[selected_weather_period][random_bucket];
    if (weather < WEATHER_NONE) {
        weather = WEATHER_NONE;
    } else if (weather >= WEATHER_SNOW) {
        weather = WEATHER_STRONG_STORM;
    }
    script_set_variable(EVENT_SCRIPT_VAR_WEATHER, weather);

    if ((rand() * 3) / 0x8000 != 0) {
        script_set_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY, TIME_OF_DAY_DAY);
    } else {
        script_set_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY, TIME_OF_DAY_NIGHT);
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
        sound_stop_sfx(map_id);
        step++;
        camera_init_defaults();
        battle_map_palette_init_state();
        battle_map_palette_load_data(&g_weather_particle_palette, BATTLE_MAP_CLUT_ROW_SPRITES, 0, 0);
        g_unit_graphics_load_queue.count = 0;
        overlay_request_small();
        break;
    case 1:
        if (overlay_poll_small() == 0) {
            step++;
        }
        break;
    case 2:
        if (battle_map_load_data(map_id, BATTLE_MAP_LOAD_COMMAND_START) != 0) {
            step++;
        }
        break;
    case 3:
        if (battle_map_load_data(map_id, BATTLE_MAP_LOAD_COMMAND_POLL) == 0) {
            step++;
        }
        break;
    case 4:
        if (unit_entd_select_and_open() != 0) {
            step++;
        }
        break;
    case 5:
        if (unit_entd_poll_and_load_units() == 0) {
            step++;
        }
        break;
    case 6:
        sprite_load_clear_state(map_id);
        step++;
        break;
    case 7:
        do {
            result = sprite_load_step_queued();
        } while (result == 2);
        if (result == 0) {
            step++;
        }
        break;
    case 8:
        deploy_build_units(g_deploy_deployed_unit_map_coordinates, 0);
        step++;
        break;
    case 9:
    case 10:
        step++;
        break;
    case 11:
        if (g_system_frontend_world_result == WORLD_MAP_RESULT_RANDOM_BATTLE
            || script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
            music_unload_current_scenario();
            /* Called with no argument: the sound id is whatever
             * music_unload_current_scenario left in $a0. */
            ((void (*)(void))sound_stop_sfx)();
            music_set_track(g_battle_map_random_battle_music_tracks[rand() / 0x2000]);
        }
        step++;
        break;
    case 12:
        weather_start_sfx();
        /* Seed the three color fades from the colors the loaded map set. */
        battle_map_background_init();
        battle_map_ambient_init();
        battle_map_darkness_init();
        battle_map_clut_upload_rows();
        turn_run_clock(TURN_CLOCK_MODE_RESET);
        ai_init_workspace();
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
 * at step 8 until the deployment screen or battle_map_skip_deployment_thread
 * releases it (deploy_mark_finished), and at step 10 waits for
 * deployment to end before restoring the scenario music and darkness. Loader
 * steps retain their current state while work is pending. */
s32 battle_map_step_deployment_init_sequence(s32 map_id, s32 step, s32 deployment_running) {
    s32 result;
    u8 unused_18[4];

    switch (step) {
    case 0:
        /* Called with no argument: the callee receives map_id, still in $a0
         * (passing it explicitly changes the register allocation). */
        ((void (*)(void))sound_stop_sfx)();
        camera_init_defaults();
        battle_map_palette_init_state();
        if (g_system_frontend_world_result == WORLD_MAP_RESULT_RANDOM_BATTLE) {
            weather_randomize_from_calendar();
        }
        step++;
        battle_map_palette_load_data(&g_weather_particle_palette, BATTLE_MAP_CLUT_ROW_SPRITES, 0, 0);
        g_unit_graphics_load_queue.count = 0;
        break;
    case 1:
        step++;
        break;
    case 2:
        if (battle_map_load_data(map_id, BATTLE_MAP_LOAD_COMMAND_START) != 0) {
            step++;
        }
        break;
    case 3:
        if (battle_map_load_data(map_id, BATTLE_MAP_LOAD_COMMAND_POLL) == 0) {
            step++;
        }
        break;
    case 4:
        if (unit_entd_select_and_open() != 0) {
            step++;
        }
        break;
    case 5:
        if (unit_entd_poll_and_load_units() != 0) {
            break;
        }
        if (g_sprite_unit_graphics_load_complete != 0) {
            sprite_load_clear_state(map_id);
            step = 7;
        } else {
            step++;
            sprite_load_reset_state();
        }
        break;
    case 6:
        if (sprite_load_step_shp_seq_data() == 8) {
            g_sprite_unit_graphics_load_complete = 1;
            sprite_load_clear_state(map_id);
            step++;
        }
        break;
    case 7:
        if (script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
            step++;
            break;
        }
        do {
            result = sprite_load_step_queued();
        } while (result == 2);
        if (result == 0) {
            step++;
        }
        break;
    case 8:
        if (g_deploy_finished != 0) {
            step++;
            deploy_init_and_reset_sprite_load_state();
            g_deploy_skipped = 0;
        }
        break;
    case 9:
        do {
            result = sprite_load_step_queued();
        } while (result == 2);
        if (result == 0) {
            step++;
        }
        break;
    case 10:
        if (deployment_running == 0) {
            step++;
            scenario_restore_music_and_darkness();
        }
        break;
    case 11:
        if (g_system_frontend_world_result == WORLD_MAP_RESULT_RANDOM_BATTLE
            || script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
            music_unload_current_scenario();
            /* Called with no argument: the sound id is whatever
             * music_unload_current_scenario left in $a0. */
            ((void (*)(void))sound_stop_sfx)();
            music_set_track(g_battle_map_random_battle_music_tracks[rand() / 0x2000]);
        }
        step++;
        break;
    case 12:
        weather_start_sfx();
        /* Seed the three color fades from the colors the loaded map set. */
        battle_map_background_init();
        battle_map_ambient_init();
        battle_map_darkness_init();
        battle_map_clut_upload_rows();
        turn_run_clock(TURN_CLOCK_MODE_RESET);
        ai_init_workspace();
        step++;
        break;
    }
    return step;
}

/* No retail code calls it. */
void fade_tint_clear(void) {
    g_fade_tint.b = 0;
    g_fade_tint.g = 0;
    g_fade_tint.r = 0;
}

/* Saves the screen color modulation for
 * fade_tint_restore. No retail code calls either. */
void fade_tint_copy(void) {
    g_fade_tint_backup.r = g_fade_tint.r;
    g_fade_tint_backup.g = g_fade_tint.g;
    g_fade_tint_backup.b = g_fade_tint.b;
}

/* No retail code calls it. */
void fade_tint_restore(void) {
    g_fade_tint.r = g_fade_tint_backup.r;
    g_fade_tint.g = g_fade_tint_backup.g;
    g_fade_tint.b = g_fade_tint_backup.b;
}

/*
 * Start a screen color-modulation fade towards (red, green, blue).
 *
 * The duration is scaled by the vsync interval; a fade that rounds to zero
 * frames only records the target color, and a zero duration does nothing. The
 * overlay draw mode is rebuilt with the given semi-transparency mode for both
 * screen polarities.
 */
void fade_tint_start_fade(u16 blend_mode, u16 red, u16 green, u16 blue, u16 duration) {
    s16 frames;

    if (duration != 0) {
        frames = duration / g_system_vsync_interval;
        g_fade_tint_target.r = red;
        g_fade_tint_target.g = green;
        g_fade_tint_target.b = blue;
        g_fade_tint_fade_frames = frames;
        if (frames != 0) {
            g_fade_tint_value[0] = g_fade_tint.r << 8;
            g_fade_tint_value[1] = g_fade_tint.g << 8;
            g_fade_tint_value[2] = g_fade_tint.b << 8;
            g_fade_tint_step[0] = ((g_fade_tint_target.r - g_fade_tint.r) << 8) / frames;
            g_fade_tint_step[1] = ((g_fade_tint_target.g - g_fade_tint.g) << 8) / frames;
            g_fade_tint_step[2] = ((g_fade_tint_target.b - g_fade_tint.b) << 8) / frames;
            SetDrawMode(&g_fade_tint_draw_modes[0], 0, 0, blend_mode << 5, &g_fade_tint_tex_window);
            g_fade_tint_draw_modes[1] = g_fade_tint_draw_modes[0];
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
void fade_tint_update_fade(void) {
    u16 count = g_fade_tint_fade_frames;
    u16 remaining;
    u16 red;
    u16 green;
    u16 blue;

    if (count != 0) {
        remaining = count - 1;
        g_fade_tint_fade_frames = remaining;
        red = g_fade_tint_value[0] + g_fade_tint_step[0];
        g_fade_tint_value[0] = red;
        green = g_fade_tint_value[1] + g_fade_tint_step[1];
        g_fade_tint_value[1] = green;
        blue = g_fade_tint_value[2] + g_fade_tint_step[2];
        g_fade_tint_value[2] = blue;
        g_fade_tint.r = red >> 8;
        g_fade_tint.g = green >> 8;
        g_fade_tint.b = blue >> 8;
        if (remaining == 0) {
            g_fade_tint.r = g_fade_tint_target.r;
            g_fade_tint.g = g_fade_tint_target.g;
            g_fade_tint.b = g_fade_tint_target.b;
        }
    }
}

/* Queues the screen color-modulation overlay for this frame, the polygon and its
 * draw mode of the current screen polarity, unless all three components are 0.
 * `unused` reproduces the frame size. */
void fade_tint_draw_overlay(void) {
    char unused[24];

    if ((g_fade_tint.r | g_fade_tint.g | g_fade_tint.b) != 0) {
        g_fade_tint_polygons[g_gfx_screen_polarity].r0 = g_fade_tint.r;
        g_fade_tint_polygons[g_gfx_screen_polarity].g0 = g_fade_tint.g;
        g_fade_tint_polygons[g_gfx_screen_polarity].b0 = g_fade_tint.b;
        AddPrim(gfx_get_battle_otag(), &g_fade_tint_polygons[g_gfx_screen_polarity]);
        AddPrim(gfx_get_battle_otag(), &g_fade_tint_draw_modes[g_gfx_screen_polarity]);
    }
}
