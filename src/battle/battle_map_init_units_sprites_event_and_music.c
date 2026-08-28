#include "fft/battle.h"
#include "fft/battle_ai.h"
#include "fft/battle_gfx.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/script_variables.h"
#include "psx/types.h"

extern void attack_sound_resync_scenario_music_and_apply_map_darkness(void);

/* Runs one step of the expanded battle-map initialization sequence.
 *
 * Loader steps retain their current state while work is pending. The final
 * steps initialize scenario audio, map lighting, and AI state. */
s32 battle_map_init_units_sprites_event_and_music(s32 map_id, s32 step, s32 deployment_running) {
    s32 result;
    u8 unused_18[4];

    switch (step) {
    case 0:
        /* Called without its sound id argument. */
        ((void (*)(void))main_sound_stop_sfx)();
        battle_camera_init_defaults();
        battle_map_init_palette_state();
        if (g_main_system_frontend_world_result == 1) {
            battle_weather_randomize_from_calendar();
        }
        step++;
        battle_map_load_palette_data(g_battle_map_weather_particle_palette, 2, 0, 0);
        g_unit_graphics_load_count = 0;
        break;
    case 1:
        step++;
        break;
    case 2:
        if (battle_map_load_data(map_id, 0x75) != 0) {
            step++;
        }
        break;
    case 3:
        if (battle_map_load_data(map_id, 0x76) == 0) {
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
            if (g_battle_unit_graphics_load_complete != 0) {
                /* The definition takes no arguments; this caller passes the map id. */
                ((void (*)(s32))battle_gfx_clear_four_state_words)(map_id);
                step = 7;
            } else {
                step++;
                battle_gfx_reset_unit_graphics_load_state();
            }
        }
        break;
    case 6:
        if (battle_gfx_step_load_unit_shp_seq_data() == 8) {
            g_battle_unit_graphics_load_complete = 1;
            /* The definition takes no arguments; this caller passes the map id. */
            ((void (*)(s32))battle_gfx_clear_four_state_words)(map_id);
            step++;
        }
        break;
    case 7:
        if (battle_script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
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
            attack_sound_resync_scenario_music_and_apply_map_darkness();
        }
        break;
    case 11:
        if (g_main_system_frontend_world_result == 1
            || battle_script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
            main_sound_unload_current_scenario_music();
            /* Called without its sound id argument. */
            ((void (*)(void))main_sound_stop_sfx)();
            main_sound_set_current_music_track(g_battle_random_battle_music_tracks[rand() / 0x2000]);
        }
        step++;
        break;
    case 12:
        battle_sound_start_weather_sfx();
        /* The target calls these three-parameter initializers without loading arguments. */
        ((void (*)(void))battle_map_init_background_gradient)();
        ((void (*)(void))battle_map_init_ambient_light)();
        ((void (*)(void))battle_map_init_darkness)();
        battle_map_update_lighting();
        battle_action_run_between_turn_control(1);
        battle_ai_init_workspace();
        step++;
        break;
    }
    return step;
}
