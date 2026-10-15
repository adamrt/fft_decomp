/* ENTD loading, weather and the map initialization sequence. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/event_attack.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Dropping either volatile view moves the mode load past the initial ENTD store. */
extern volatile s32 g_battle_entd_selection_mode;

/* Chooses the encounter's ENTD id and starts reading its ENTD file.
 *
 * The id is script variable EVENT_SCRIPT_VAR_CURRENT_ENTD unless
 * g_battle_entd_selection_mode overrides it: 250, a random id 75..79, a random id
 * 1..59, or the map id when that is not 0. Returns main_entd_open_file's result. */
s32 battle_unit_select_and_open_entd(void) {
    volatile s32* current_event_id;
    s32 event_id;
    s32 selection_mode;

    event_id = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_ENTD);
    selection_mode = g_battle_entd_selection_mode;
    current_event_id = &g_main_entd_current_event_id;
    *current_event_id = event_id;

    if (selection_mode == BATTLE_ENTD_SELECTION_ENTD_250) {
        *current_event_id = 250;
    } else if (selection_mode == BATTLE_ENTD_SELECTION_RANDOM_75_TO_79) {
        *current_event_id = (rand() * 5) / 0x8000 + 75;
    } else if (selection_mode == BATTLE_ENTD_SELECTION_RANDOM_1_TO_59) {
        *current_event_id = (rand() * 59) / 0x8000 + 1;
    } else if ((selection_mode == BATTLE_ENTD_SELECTION_MAP_ID) && (g_map_id != 0)) {
        *current_event_id = g_map_id;
    }

    return main_entd_open_file();
}

/* Loads the encounter's ENTD units once the ENTD file has arrived, then frees the
 * file. Returns 1 while the file is still loading, else 0 (also when there is no
 * current encounter). */
s32 battle_unit_poll_and_load_entd_units(void) {
    entd_encounter_t* entd_data;

    entd_data = main_entd_get_encounter();
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
 * The calendar cutoffs divide the year into four weather periods. Each row of
 * signed adjustments is indexed by a uniform `rand()` bucket and applied to
 * the current weather strength before it is clamped to 0..4.
 */
void battle_weather_randomize_from_calendar(void) {
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

/* Runs one step of the battle map initialization sequence and returns the
 * next step; steps that poll a loader stay on the same step until it
 * finishes. `unused_18` reproduces the target's unreferenced frame slot at sp+0x18.
 *
 * Jump table: .rodata 0x80068260, 13 32-bit addresses. */
s32 map_step_init_sequence(s32 map_id, s32 step) {
    s32 result;
    u8 unused_18[4];

    switch (step) {
    case 0:
        /* No sound id is loaded: the callee receives map_id, still in $a0. */
        main_sound_stop_sfx(map_id);
        step++;
        camera_init_defaults();
        map_init_palette_state();
        map_load_palette_data(&g_map_weather_particle_palette, MAP_CLUT_ROW_SPRITES, 0, 0);
        g_main_gfx_unit_graphics_load_queue.count = 0;
        menu_request_open_small();
        break;
    case 1:
        if (menu_poll_small_load() == 0) {
            step++;
        }
        break;
    case 2:
        if (map_load_data(map_id, MAP_LOAD_COMMAND_START) != 0) {
            step++;
        }
        break;
    case 3:
        if (map_load_data(map_id, MAP_LOAD_COMMAND_POLL) == 0) {
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
        sound_start_weather_sfx();
        /* Seed the three color fades from the colors the loaded map set. */
        map_init_background_gradient();
        map_init_ambient_light();
        map_init_darkness();
        map_upload_clut_rows();
        action_run_between_turn_control(BATTLE_BETWEEN_TURN_MODE_RESET);
        ai_init_workspace();
        step++;
        break;
    }
    return step;
}

/* Runs one step of the expanded battle-map initialization sequence.
 *
 * Loader steps retain their current state while work is pending. The final
 * steps initialize scenario audio, map lighting, and AI state. */
s32 map_init_units_sprites_event_and_music(s32 map_id, s32 step, s32 deployment_running) {
    s32 result;
    u8 unused_18[4];

    switch (step) {
    case 0:
        /* Called with no argument: the callee receives map_id, still in $a0
         * (passing it explicitly changes the register allocation). */
        ((void (*)(void))main_sound_stop_sfx)();
        camera_init_defaults();
        map_init_palette_state();
        if (g_main_system_frontend_world_result == 1) {
            battle_weather_randomize_from_calendar();
        }
        step++;
        map_load_palette_data(&g_map_weather_particle_palette, MAP_CLUT_ROW_SPRITES, 0, 0);
        g_main_gfx_unit_graphics_load_queue.count = 0;
        break;
    case 1:
        step++;
        break;
    case 2:
        if (map_load_data(map_id, MAP_LOAD_COMMAND_START) != 0) {
            step++;
        }
        break;
    case 3:
        if (map_load_data(map_id, MAP_LOAD_COMMAND_POLL) == 0) {
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
        if (g_map_data_load_complete != 0) {
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
            attack_sound_resync_scenario_music_and_apply_map_darkness();
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
        sound_start_weather_sfx();
        /* Seed the three color fades from the colors the loaded map set. */
        map_init_background_gradient();
        map_init_ambient_light();
        map_init_darkness();
        map_upload_clut_rows();
        action_run_between_turn_control(BATTLE_BETWEEN_TURN_MODE_RESET);
        ai_init_workspace();
        step++;
        break;
    }
    return step;
}
