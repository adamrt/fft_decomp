#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/event_attack.h"
#include "fft/script.h"
#include "psx/types.h"

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
