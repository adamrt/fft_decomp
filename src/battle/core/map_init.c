/* Load ENTD units and advance map and deployment initialization. */

#include "fft/battle/ai.h"
#include "fft/battle/camera.h"
#include "fft/battle/cursor.h"
#include "fft/battle/deploy.h"
#include "fft/battle/encounter.h"
#include "fft/battle/turn.h"
#include "fft/battle/weather.h"
#include "fft/game/battle_unit.h"
#include "fft/game/entd.h"
#include "fft/gfx/gfx.h"
#include "fft/gfx/sprite_sheet.h"
#include "fft/map/map.h"
#include "fft/map/map_light.h"
#include "fft/map/map_palette.h"
#include "fft/map/map_unit.h"
#include "fft/script/script.h"
#include "fft/script/script_variable.h"
#include "fft/sys/math.h"
#include "fft/sys/memory.h"
#include "fft/sys/music.h"
#include "fft/sys/overlay.h"
#include "fft/sys/sound.h"
#include "fft/world/world_map.h"
#include "psx/libc.h"
#include "psx/libgte.h"
#include "psx/types.h"

extern u8 g_map_random_battle_music_tracks[4];

static s32 s_sprite_unit_graphics_load_complete;

extern palette_t g_weather_particle_palette;

/* Resets the camera for a new map: screen offset (0x100, 0xa0, 0x280), zoom ONE,
 * pitch 0x12e (the low tilt), yaw 0xe00, position at the origin, cursor on tile
 * (0, 0), no acting or casting unit, no rotation, tilt or zoom in progress and
 * directional panning off. */
static void init_camera_defaults(void) {
    vector_set_long(&g_camera_screen_offset, 0x100, 0xA0, 0x280);
    vector_set_long(&g_camera_zoom, ONE, ONE, ONE);
    vector_set_short(&g_camera_render_state, 0x12E, 0xE00, 0);
    vector_set_long(&g_camera_real_coords, 0, 0, 0);

    g_unit_casting_map_unit_id = 0xFF;
    g_unit_source_map_unit_id = 0xFF;
    g_cursor_y = 0;
    g_cursor_layer = 0;
    g_cursor_x = 0;
    g_camera_rotation_action = CAMERA_ROTATION_NONE;
    g_camera_zoom_action = CAMERA_STEP_NONE;
    g_camera_zoom_target = CAMERA_ZOOM_IN;
    g_camera_tilt_action = CAMERA_STEP_NONE;
    g_camera_tilt_target = CAMERA_TILT_LOW;
    g_camera_pan_enabled = 0;
}

/* Appends one unit to the graphics load queue that
 * sprite_update_queued_load works through.
 *
 * A spritesheet id of SPRITE_SHEET_ID_COUNT or more is replaced by
 * Ramza's chapter 1 sheet. The count stops at 16, the size of the
 * queue, so a seventeenth call writes the record one past its end. */
void sprite_queue_unit_graphics_load(s32 map_x, s32 map_y, s32 map_level, s16 facing, s16 spritesheet_id,
    s16 palette_id, s16 map_unit_id, battle_unit_t* unit, u32 flags) {
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
    g_unit_graphics_load_queue.descriptors[g_unit_graphics_load_queue.count].map_unit_id = map_unit_id;
    g_unit_graphics_load_queue.descriptors[g_unit_graphics_load_queue.count].battle_unit = unit;
    g_unit_graphics_load_queue.descriptors[g_unit_graphics_load_queue.count].flags = flags;
    if (g_unit_graphics_load_queue.count < MAP_UNIT_SLOT_COUNT) {
        g_unit_graphics_load_queue.count++;
    }
}

/* Always 0. No code calls it. */
s32 unit_return_zero_1(void) {
    return 0;
}

/* Chooses the encounter's ENTD id and starts reading its ENTD file.
 *
 * The id is script variable SCRIPT_VAR_CURRENT_ENTD unless
 * g_encounter_selection_mode overrides it: 250, a random id 75..79, a random id
 * 1..59, or the map id when that is not 0. Returns encounter_start_file_load's result. */
static s32 start_entd_open(void) {
    /* The bytes require volatile: it keeps the script id's store ahead of every override. */
    volatile s32* current_encounter_id;
    s32 encounter_id;
    s32 selection_mode;

    encounter_id = script_variable_get(SCRIPT_VAR_CURRENT_ENTD);
    selection_mode = g_encounter_selection_mode;
    current_encounter_id = &g_encounter_current_id;
    *current_encounter_id = encounter_id;

    if (selection_mode == ENCOUNTER_SELECTION_ENTD_250) {
        *current_encounter_id = 250;
    } else if (selection_mode == ENCOUNTER_SELECTION_RANDOM_75_TO_79) {
        *current_encounter_id = (rand() * 5) / 0x8000 + 75;
    } else if (selection_mode == ENCOUNTER_SELECTION_RANDOM_1_TO_59) {
        *current_encounter_id = (rand() * 59) / 0x8000 + 1;
    } else if (selection_mode == ENCOUNTER_SELECTION_MAP_ID && g_map_id != 0) {
        *current_encounter_id = g_map_id;
    }

    return encounter_start_file_load();
}

/* Loads the encounter's ENTD units once the ENTD file has arrived, then frees the
 * file. Returns 1 while the file is still loading, else 0 (also when there is no
 * current encounter). */
static s32 poll_entd_units_load(void) {
    const entd_encounter_t* entd_data = encounter_get_current();

    if (entd_data != (const entd_encounter_t*)-1) {
        if (entd_data == 0) {
            return 1;
        }
        battle_unit_load_entd(entd_data, 0);
        heap_release_block(g_encounter_set);
    }
    return 0;
}

enum { WEATHER_CALENDAR_PERIOD_COUNT = 4, WEATHER_RANDOM_VARIANT_COUNT = 10 };

/* Randomizes the battle's weather and time of day for the current date.
 *
 * The calendar cutoffs divide the year into four weather periods (the last
 * cutoff wraps back to the first period's row). Each row of signed
 * adjustments is indexed by a uniform `rand()` bucket and applied to the
 * current weather strength before it is clamped to none..strong storm. One
 * battle in three takes place at night.
 */
static void randomize_weather_from_calendar(void) {
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

    month = script_variable_get(SCRIPT_VAR_MONTH);
    day = script_variable_get(SCRIPT_VAR_DAY);
    weather = script_variable_get(SCRIPT_VAR_WEATHER);

    for (period = 0; period < WEATHER_CALENDAR_PERIOD_COUNT; period++) {
        if (month < period_end_month[period]) {
            break;
        }
        if (month == period_end_month[period] && day < period_end_day[period]) {
            break;
        }
    }

    selected_weather_period = weather_period[period];
    random_bucket = (rand() * WEATHER_RANDOM_VARIANT_COUNT) / 0x8000;
    weather += weather_adjustments[selected_weather_period][random_bucket];
    if (weather < WEATHER_NONE) {
        weather = WEATHER_NONE;
    } else if (weather >= WEATHER_SNOW) {
        weather = WEATHER_STRONG_STORM;
    }
    script_variable_set(SCRIPT_VAR_WEATHER, weather);

    if ((rand() * 3) / 0x8000 != 0) {
        script_variable_set(SCRIPT_VAR_TIME_OF_DAY, TIME_OF_DAY_DAY);
    } else {
        script_variable_set(SCRIPT_VAR_TIME_OF_DAY, TIME_OF_DAY_NIGHT);
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
s32 map_update_init_sequence(s32 map_id, s32 step) {
    s32 result;
    u8 unused_18[4];

    switch (step) {
    case 0:
        /* No sound id is loaded: the callee receives map_id, still in $a0. */
        sound_stop_sfx(map_id);
        step++;
        init_camera_defaults();
        map_palette_init_state();
        map_palette_load_data(&g_weather_particle_palette, MAP_CLUT_ROW_SPRITES, 0, 0);
        g_unit_graphics_load_queue.count = 0;
        overlay_small_request();
        break;
    case 1:
        if (overlay_small_poll() == 0) {
            step++;
        }
        break;
    case 2:
        if (map_update_load(map_id, MAP_LOAD_COMMAND_START) != 0) {
            step++;
        }
        break;
    case 3:
        if (map_update_load(map_id, MAP_LOAD_COMMAND_POLL) == 0) {
            step++;
        }
        break;
    case 4:
        if (start_entd_open() != 0) {
            step++;
        }
        break;
    case 5:
        if (poll_entd_units_load() == 0) {
            step++;
        }
        break;
    case 6:
        sprite_clear_load_state(map_id);
        step++;
        break;
    case 7:
        do {
            result = sprite_update_queued_load();
        } while (result == 2);
        if (result == 0) {
            step++;
        }
        break;
    case 8:
        deploy_init_units(g_deploy_deployed_unit_map_coordinates, 0);
        step++;
        break;
    case 9:
    case 10:
        step++;
        break;
    case 11:
        if (g_system_frontend_world_result == WORLD_MAP_RESULT_RANDOM_BATTLE
            || script_variable_get(SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
            music_unload_current_scenario();
            /* Called with no argument: the sound id is whatever
             * music_unload_current_scenario left in $a0. */
            ((void (*)(void))sound_stop_sfx)();
            music_set_track(g_map_random_battle_music_tracks[rand() / 0x2000]);
        }
        step++;
        break;
    case 12:
        weather_start_sfx();
        /* Seed the three color fades from the colors the loaded map set. */
        map_light_init_background();
        map_light_init_ambient();
        map_light_init_darkness();
        map_palette_upload_clut_rows();
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
 * Unlike map_update_init_sequence it randomizes a random battle's weather,
 * loads the shared SHP and SEQ data once (skipped when already loaded), waits
 * at step 8 until the deployment screen or deploy_skip_thread
 * releases it (deploy_mark_finished), and at step 10 waits for
 * deployment to end before restoring the scenario music and darkness. Loader
 * steps retain their current state while work is pending. */
s32 map_update_deployment_init_sequence(s32 map_id, s32 step, s32 deployment_running) {
    s32 result;
    u8 unused_18[4];

    switch (step) {
    case 0:
        /* Called with no argument: the callee receives map_id, still in $a0
         * (passing it explicitly changes the register allocation). */
        ((void (*)(void))sound_stop_sfx)();
        init_camera_defaults();
        map_palette_init_state();
        if (g_system_frontend_world_result == WORLD_MAP_RESULT_RANDOM_BATTLE) {
            randomize_weather_from_calendar();
        }
        step++;
        map_palette_load_data(&g_weather_particle_palette, MAP_CLUT_ROW_SPRITES, 0, 0);
        g_unit_graphics_load_queue.count = 0;
        break;
    case 1:
        step++;
        break;
    case 2:
        if (map_update_load(map_id, MAP_LOAD_COMMAND_START) != 0) {
            step++;
        }
        break;
    case 3:
        if (map_update_load(map_id, MAP_LOAD_COMMAND_POLL) == 0) {
            step++;
        }
        break;
    case 4:
        if (start_entd_open() != 0) {
            step++;
        }
        break;
    case 5:
        if (poll_entd_units_load() != 0) {
            break;
        }
        if (s_sprite_unit_graphics_load_complete) {
            sprite_clear_load_state(map_id);
            step = 7;
        } else {
            step++;
            sprite_reset_unit_graphics();
        }
        break;
    case 6:
        if (sprite_update_shared_load() == 8) {
            s_sprite_unit_graphics_load_complete = 1;
            sprite_clear_load_state(map_id);
            step++;
        }
        break;
    case 7:
        if (script_variable_get(SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
            step++;
            break;
        }
        do {
            result = sprite_update_queued_load();
        } while (result == 2);
        if (result == 0) {
            step++;
        }
        break;
    case 8:
        if (g_deploy_finished) {
            step++;
            deploy_init_placed_units();
            g_deploy_skipped = 0;
        }
        break;
    case 9:
        do {
            result = sprite_update_queued_load();
        } while (result == 2);
        if (result == 0) {
            step++;
        }
        break;
    case 10:
        if (!deployment_running) {

            step++;
            scenario_restore_music_and_darkness();
        }
        break;
    case 11:
        if (g_system_frontend_world_result == WORLD_MAP_RESULT_RANDOM_BATTLE
            || script_variable_get(SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
            music_unload_current_scenario();
            /* Called with no argument: the sound id is whatever
             * music_unload_current_scenario left in $a0. */
            ((void (*)(void))sound_stop_sfx)();
            music_set_track(g_map_random_battle_music_tracks[rand() / 0x2000]);
        }
        step++;
        break;
    case 12:
        weather_start_sfx();
        /* Seed the three color fades from the colors the loaded map set. */
        map_light_init_background();
        map_light_init_ambient();
        map_light_init_darkness();
        map_palette_upload_clut_rows();
        turn_run_clock(TURN_CLOCK_MODE_RESET);
        ai_init_workspace();
        step++;
        break;
    }
    return step;
}
